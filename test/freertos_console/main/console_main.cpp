/*
 * Serial console test driver for strpp on a real FreeRTOS (ESP32-S3), with WiFi scanning
 * as the work to do. With the monitor on it also starts the threads that deadlock, stall,
 * and so on, for the monitor to find.
 *
 * The main thread reads the console and prints what arrives in its own MessageQueue.
 * A WifiScanner thread does the scanning, and nothing else touches WiFi.
 *
 * This file uses stdio for the console, which strpp has no primitives for yet.
 */
#include "sdkconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "thread.h"
#include "msgqueue.h"
#include "transactional.h"
#include "wifi_scanner.h"
#include "../../condition_signal_test.h"

#if defined(STRPP_MONITOR)
#include "esp_heap_caps.h"
#include "lock.h"
#include "monitor.h"
#include "strpp_msg.h"
#endif

extern "C" {
	void app_main();
}

static const char*	PROMPT = "wifi> ";

static const char*	HELP =
	"scan          scan now\n"
	"auto <secs>   scan every <secs> seconds; 'auto 0' stops\n"
	"info          free memory and stack use\n"
	"tasks         list every FreeRTOS task\n"
	"condition     check that one signal wakes one waiter (takes about 2 s)\n"
	"threadreuse   check that an ended, unjoined thread survives its task handle being reused\n"
#if defined(STRPP_MONITOR)
	"monitor       what the monitor sees now\n"
	"deadlock      two threads that deadlock, for about 12 s\n"
	"stall         a thread that holds a lock and waits, for about 12 s\n"
	"errors        a thread with 20 unhandled errors, for about 12 s\n"
	"jam           a thread blocked pushing to a full queue, for about 12 s\n"
	"ballast <KB>  hold that much memory; 'ballast 0' frees it\n"
	"reap          wait for those threads to end and clean them up\n"
#endif
	"quit          stop the scanner thread\n"
	"help          this text\n";

// The USB console sends its output to the host only on a newline, or when told to
static void
flush()
{
	fflush(stdout);
	fsync(fileno(stdout));
}

#if defined(STRPP_MONITOR)

static Monitor*	the_monitor;

// Memory figures for the monitor
static bool
heap_probe(MonitorMemory& memory)
{
	memory.free_bytes = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
	memory.largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
	memory.lowest_free = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
	return true;
}

// What the test threads fight over
struct	Trial
{
	Trial() : jam("jam") {}
	SIXLock		first, second;
	Latch		latch;
	Condition	cond;
	MessageQueue	jam;
};

enum	Scene { SceneDeadlockA, SceneDeadlockB, SceneStall, SceneErrors, SceneJam };

class	Scenario
: public Thread
{
public:
	Scenario(const char* a_name, Trial& a_trial, Scene a_scene)
	: Thread(params(a_name)), trial(a_trial), scene(a_scene)
	{ resume(); }

	int	run();

private:
	Trial&		trial;
	Scene		scene;

	static const ThreadParams*	params(const char* name)
	{
		static ThreadParams	slots[8];
		static int		next = 0;
		ThreadParams&		p = slots[next++ % 8];
		p.name = name;
		return &p;
	}
};

static const long	SCENE_MS = 12000;	// How long each test thread holds on

int
Scenario::run()
{
	switch (scene)
	{
	case SceneDeadlockA:
	case SceneDeadlockB:
	{
		SIXLock&	mine = scene == SceneDeadlockA ? trial.first : trial.second;
		SIXLock&	theirs = scene == SceneDeadlockA ? trial.second : trial.first;
		ExclLock	a(mine);
		Thread::yield(Milliseconds(300));
		ExclLock	b(theirs, Milliseconds(SCENE_MS));	// Gives up, so the test ends
		break;
	}
	case SceneStall:
	{
		SharedLock	window(trial.first);
		trial.latch.enter();
		long		ms = SCENE_MS;
		trial.cond.wait(ms, &trial.latch);
		trial.latch.leave();
		break;
	}
	case SceneErrors:
		for (int i = 0; i < 20; i++)
			ErrorTHR_CreateFailed("test", i);
		Thread::yield(Milliseconds(SCENE_MS));
		ErrBuffer()->clear();		// A thread must not end with errors
		break;
	case SceneJam:
		for (int i = 0; i < MSGQUEUE_HIGH_WATER; i++)
			trial.jam.try_push(Variant(i));
		trial.jam.push(Variant(0), Milliseconds(SCENE_MS));	// Waits for room that never comes
		break;
	}
	return 0;
}

#endif	// STRPP_MONITOR

class	Console
{
public:
	Console(WifiScanner& a_scanner, MessageQueue& a_inbox)
	: scanner(a_scanner)
	, inbox(a_inbox)
	, length(0)
	, after_cr(false)
	, stopping(false)
	, stopped(false)
#if defined(STRPP_MONITOR)
	, scenario_count(0)
	, ballast(0)
#endif
	{}

	void		loop();

private:
	WifiScanner&	scanner;
	MessageQueue&		inbox;
	char		line[80];
	size_t		length;
	bool		after_cr;
	bool		stopping;	// A quit request is sent
	bool		stopped;	// The scanner has ended

	void		prompt();
	void		key(int c);
	void		command();
	void		request(const char* name, int argument = -1);
	void		show(const Variant& message);
	void		show_scan();
	void		info();
	void		tasks();
	void		condition();
	void		threadreuse();
#if defined(STRPP_MONITOR)
	static const int	MAX_THREADS = 8;
	Scenario*	scenarios[MAX_THREADS];
	Trial*		trials[MAX_THREADS];
	int		scenario_count;
	char*		ballast;

	void		start(const char* name, Trial* trial, Scene scene);
	void		reap();
	void		hold(const char* arg);
	void		monitor_status();
	void		show_monitor(const VariantArray& parts);
#endif
};

void
Console::prompt()
{
	printf("%s%.*s", PROMPT, (int)length, line);
	flush();
}

void
Console::request(const char* name, int argument)
{
	if (stopped || stopping)
	{
		printf("The scanner has stopped; reset the board to restart it\n");
		return;
	}
	VariantArray	message = VariantArray() << name;
	if (argument >= 0)
		message << argument;
	scanner.requests.push(Variant(message));
}

void
Console::info()
{
	printf("Heap free %u, lowest ever %u\n",
		(unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size());
	printf("Stack bytes never used: console %u, scanner %u\n",
		(unsigned)uxTaskGetStackHighWaterMark(NULL),
		(unsigned)(stopped ? 0 : uxTaskGetStackHighWaterMark(scanner.id())));

	ReadWindow<WifiScan>	w(scanner.scan, Milliseconds(100));
	if (w.holding())
		printf("Scanner: %s, %u scans, auto %ld ms\n", w->ready ? "running" : "not running", w->scan_count, w->auto_ms);
	else
		printf("Scanner: busy\n");
}

void
Console::tasks()
{
	size_t		size = 64 * (size_t)uxTaskGetNumberOfTasks();
	char*		text = (char*)malloc(size);
	if (!text)
	{
		printf("Out of memory listing tasks\n");
		return;
	}
	vTaskList(text);
	printf("Name            State  Prio  Stack-free  Num  Core\n%s", text);
	free(text);
}

void
Console::condition()
{
	ConditionSignalResult	r = condition_signal_test();
	printf("woken after one signal %d (want 1), after a broadcast %d (want 2), after a later signal %d (want 3): %s\n",
		r.after_signal, r.after_broadcast, r.after_later_signal,
		r.after_signal == 1 && r.after_broadcast == 2 && r.after_later_signal == 3 ? "ok" : "FAIL");
}

// A thread that ends at once, and exposes the registry state the reuse check reports
class ReuseProbe
: public Thread
{
public:
	int		run() { return 7; }
	void		start() { resume(); }
	bool		ended() const { return state == Ended; }
	bool		listed() { thread_latch.enter(); bool r = registered(this); thread_latch.leave(); return r; }
	static int	ended_total() { return ended_count; }
};

void
Console::threadreuse()
{
	static const int	MAX_TRIES = 64;
	ReuseProbe*		probes[MAX_TRIES + 1];
	int			made = 0;
	int			reused = -1;	// Index of the first probe whose task handle equals an earlier probe's

	// Each probe ends and is left unjoined, so its task is deleted and its handle can be handed out again
	for (; made <= MAX_TRIES && reused < 0; made++)
	{
		ReuseProbe*	p = new ReuseProbe;
		probes[made] = p;
		p->start();
		while (!p->ended())
			vTaskDelay(pdMS_TO_TICKS(5));
		vTaskDelay(pdMS_TO_TICKS(50));		// The idle task frees a deleted task's memory
		for (int i = 0; i < made; i++)
			if (p->id() == probes[i]->id())
				reused = made;
	}

	int		listed = 0;
	for (int i = 0; i < made; i++)
		if (probes[i]->listed())
			listed++;
	int		counted = ReuseProbe::ended_total();
	if (reused < 0)
		printf("%d threads made and no task handle was reused, so this run proves nothing\n", made);
	else
		printf("a task handle was reused, first by thread %d\n", reused);
	printf("%d threads made: %d registered (want %d), %d counted ended (want %d); calling joinAny\n",
		made, listed, made, counted, made);
	flush();

	int		joined = 0;
	for (int i = 0; i < made; i++)
	{
		Thread*		t = Thread::joinAny();
		if (!t)
			break;
		joined++;
	}
	for (int i = 0; i < made; i++)
		delete probes[i];

	printf("joinAny returned %d (want %d)\n", joined, made);
	printf("%s\n", reused >= 0 && listed == made && counted == made && joined == made ? "ok" : "FAIL");
}

#if defined(STRPP_MONITOR)

void
Console::start(const char* name, Trial* trial, Scene scene)
{
	if (scenario_count >= MAX_THREADS)
	{
		printf("Too many test threads: type 'reap' first\n");
		return;
	}
	trials[scenario_count] = trial;
	scenarios[scenario_count++] = new Scenario(name, *trial, scene);
}

void
Console::reap()
{
	if (scenario_count == 0)
	{
		printf("No test threads\n");
		return;
	}
	printf("Waiting for %d test thread(s)...\n", scenario_count);
	for (int i = 0; i < scenario_count; i++)
	{
		scenarios[i]->join();
		delete scenarios[i];
		if (i == scenario_count-1 || trials[i+1] != trials[i])
			delete trials[i];		// The last of the threads that shared it
	}
	scenario_count = 0;
	ErrBuffer()->clear();
}

void
Console::hold(const char* arg)
{
	long		kb = arg ? strtol(arg, 0, 10) : -1;
	if (kb < 0 || kb > 180)
	{
		printf("ballast needs a number of KB from 0 to 180\n");
		return;
	}
	free(ballast);
	ballast = kb ? (char*)malloc(kb*1024) : 0;
	if (kb && !ballast)
		printf("Out of memory holding %ld KB\n", kb);
	else if (ballast)
		memset(ballast, 1, kb*1024);
}

void
Console::monitor_status()
{
	if (!the_monitor)
		return;
	ReadWindow<MonitorData>	window(the_monitor->published, Milliseconds(200));
	if (!window.holding())
	{
		printf("The monitor is busy\n");
		return;
	}
	printf("Monitor: %u looks, %u reports dropped; %u threads, %u queues, %u error buffers\n",
		window->samples, window->dropped, window->threads, window->queues, window->error_buffers);
	printf("Memory: %u free, %u largest block, %u lowest ever\n",
		(unsigned)window->memory.free_bytes, (unsigned)window->memory.largest_block, (unsigned)window->memory.lowest_free);
	VariantArray		findings = window->findings;
	window.close();
	printf("%u finding(s)\n", (unsigned)findings.length());
	for (VariantArray::Index i = 0; i < findings.length(); i++)
		show_monitor(findings[i].as_variant_array());
}

void
Console::show_monitor(const VariantArray& parts)
{
	StrVal		kind = parts[1].as_strval();
	if (kind == "deadlock")
	{
		printf("  DEADLOCK between");
		VariantArray	names = parts[2].as_variant_array();
		for (VariantArray::Index i = 0; i < names.length(); i++)
			printf(" %s", names[i].as_strval().asUTF8());
		printf("\n");
	}
	else if (kind == "stall")
		printf("  STALL: %s waited %ld ms on a %s, holding %d lock(s)\n", parts[2].as_strval().asUTF8(),
			parts[3].as_long(), parts[4].as_strval().asUTF8(), parts[5].as_int());
	else if (kind == "queue")
		printf("  QUEUE %s: %d items, peak %d, %d pusher(s) waiting\n", parts[2].as_strval().asUTF8(),
			parts[3].as_int(), parts[4].as_int(), parts[5].as_int());
	else if (kind == "errors")
		printf("  ERRORS: %s has %d unhandled (%d parameters)\n", parts[2].as_strval().asUTF8(),
			parts[3].as_int(), parts[4].as_int());
	else if (kind == "memory")
		printf("  MEMORY: %ld free, largest block %ld, lowest ever %ld\n",
			parts[2].as_long(), parts[3].as_long(), parts[4].as_long());
}

#endif	// STRPP_MONITOR

void
Console::command()
{
	line[length] = '\0';
	length = 0;

	char*		word = strtok(line, " \t");
	if (!word)
		return;
	char*		arg = strtok(NULL, " \t");

	if (!strcmp(word, "scan"))
		request("scan");
	else if (!strcmp(word, "auto"))
	{
		char*		end = 0;
		long		secs = arg ? strtol(arg, &end, 10) : -1;
		if (!arg || *end || secs < 0 || secs > 86400)
			printf("auto needs a number of seconds, from 0 to 86400\n");
		else
			request("auto", (int)(secs*1000));
	}
	else if (!strcmp(word, "info"))
		info();
	else if (!strcmp(word, "tasks"))
		tasks();
	else if (!strcmp(word, "condition"))
		condition();
	else if (!strcmp(word, "threadreuse"))
		threadreuse();
#if defined(STRPP_MONITOR)
	else if (!strcmp(word, "monitor"))
		monitor_status();
	else if (!strcmp(word, "deadlock"))
	{
		Trial*		t = new Trial;
		start("dead-a", t, SceneDeadlockA);
		start("dead-b", t, SceneDeadlockB);
	}
	else if (!strcmp(word, "stall"))
		start("stalled", new Trial, SceneStall);
	else if (!strcmp(word, "errors"))
		start("noisy", new Trial, SceneErrors);
	else if (!strcmp(word, "jam"))
		start("jammed", new Trial, SceneJam);
	else if (!strcmp(word, "ballast"))
		hold(arg);
	else if (!strcmp(word, "reap"))
		reap();
#endif
	else if (!strcmp(word, "quit"))
	{
		request("quit");
		stopping = true;
	}
	else if (!strcmp(word, "help"))
		printf("%s", HELP);
	else
		printf("Unknown command '%s'. Try 'help'\n", word);
}

void
Console::key(int c)
{
	bool		was_cr = after_cr;
	after_cr = false;

	if (c == '\r' || c == '\n')
	{
		if (c == '\n' && was_cr)
			return;			// The second half of CR LF
		after_cr = (c == '\r');
		printf("\n");
		command();
		prompt();
	}
	else if (c == 0x7f || c == 0x08)
	{
		if (length > 0)
		{
			length--;
			printf("\b \b");
			flush();
		}
	}
	else if (c >= 0x20 && c < 0x7f && length < sizeof line-1)
	{
		line[length++] = (char)c;
		putchar(c);
		flush();
	}
}

void
Console::show_scan()
{
	VariantArray	aps;
	{
		ReadWindow<WifiScan>	w(scanner.scan, Milliseconds(1000));
		if (!w.holding())
		{
			printf("The scanner is busy; the access points are not available yet\n");
			return;
		}
		aps = w->last_scan;
	}
	printf("%u access points\n", (unsigned)aps.length());
	if (aps.length() == 0)
		return;
	printf(" %-32s %4s %3s %-9s %s\n", "SSID", "dBm", "Ch", "Auth", "BSSID");
	for (VariantArray::Index i = 0; i < aps.length(); i++)
	{
		VariantArray	ap = aps[i].as_variant_array();
		StrVal		ssid = ap[0].as_strval();
		StrVal		auth = ap[3].as_strval();
		StrVal		bssid = ap[4].as_strval();
		printf(" %-32s %4d %3d %-9s %s\n",
			ssid.asUTF8(), ap[1].as_int(), ap[2].as_int(),
			auth.asUTF8(), bssid.asUTF8());
	}
}

void
Console::show(const Variant& message)
{
	printf("\r\033[K");		// Clear the prompt and what was typed after it
	if (message.type() != Variant::VarArray || message.as_variant_array().length() == 0)
		printf("Unrecognised message from the scanner\n");
	else
	{
		VariantArray	parts = message.as_variant_array();
		StrVal		name = parts[0].as_strval();
		if (name == "ready")
			printf("WiFi started. Type 'help' for commands\n");
		else if (name == "scan")
			show_scan();
		else if (name == "error")
			printf("Error: %s\n", parts[1].as_strval().asUTF8());
#if defined(STRPP_MONITOR)
		else if (name == "monitor")
		{
			printf("Monitor:\n");
			show_monitor(parts);
		}
#endif
		else if (name == "quit")
		{
			scanner.join();
			stopped = true;
			printf("Scanner stopped\n");
		}
		else
			printf("Unrecognised message '%s'\n", name.asUTF8());
	}
	prompt();
}

void
Console::loop()
{
	prompt();
	for (;;)
	{
		bool		idle = true;

		Variant		message;
		while (inbox.try_pop(message))
		{
			show(message);
			idle = false;
		}

		int		c = getchar();
		if (c == EOF)
			clearerr(stdin);	// Nothing typed: the console doesn't block
		else
		{
			key(c);
			idle = false;
		}

		if (idle)
			Thread::yield(Milliseconds(20));
	}
}

void
app_main()
{
	MainThread	main_thread;

	setvbuf(stdin, NULL, _IONBF, 0);

	MessageQueue*		inbox = MessageQueue::mine();
	WifiScanner	scanner(*inbox);
#if defined(STRPP_MONITOR)
	MonitorSettings	settings;
	settings.interval = Milliseconds(3000);
	settings.stall = Milliseconds(4000);
	settings.confirm = Milliseconds(300);
	settings.probe = heap_probe;
	settings.low_free = 100000;
	Monitor		monitor(*inbox, settings);
	the_monitor = &monitor;
#endif
	Console		console(scanner, *inbox);
	console.loop();
}
