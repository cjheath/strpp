/*
 * Monitor: finds a deadlock, a stall, a full queue and a growing error buffer
 * as they happen, and reports each once. Built with STRPP_MONITOR, which
 * `make monitor_test` arranges.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<cstring>
#include	<functional>

#include	<thread.h>
#include	<condition.h>
#include	<lock.h>
#include	<msgqueue.h>
#include	<strpp_msg.h>
#include	<monitor.h>

static int	fails = 0;

static void
expect(const char* what, bool ok)
{
	if (!ok)
		fails++;
	printf("  %-62s %s\n", what, ok ? "ok" : "FAIL");
}

class	Runner
: public Thread
{
	std::function<void()>	body;
public:
	Runner(const char* a_name, std::function<void()> a_body)
	: Thread(params(a_name)), body(a_body)
	{ resume(); }

	int	run()	{ body(); return 0; }

private:
	static const ThreadParams*	params(const char* name)
	{
		static ThreadParams	p[8];
		static int		next = 0;
		ThreadParams&		t = p[next++ % 8];
		t.name = name;
		return &t;
	}
};

// The next report of a kind, and about the named thread or queue if a name is given,
// waiting up to timeout_ms; empty if none comes
static VariantArray
wait_report(MessageQueue& reports, const char* kind, long timeout_ms, const char* about = 0)
{
	for (long waited = 0; waited < timeout_ms; waited += 50)
	{
		Variant		item = reports.pop(Milliseconds(50));
		if (item.is_null() || item.type() != Variant::VarArray)
			continue;
		VariantArray	report = item.as_variant_array();
		if (report.length() >= 2 && report[1].as_strval() == kind
		 && (!about || (report.length() > 2 && report[2].type() == Variant::String && report[2].as_strval() == about)))
			return report;
	}
	return VariantArray();
}

static MonitorMemory	fake_memory = { 500000, 400000, 300000 };

static bool
fake_probe(MonitorMemory& memory)
{
	memory = fake_memory;
	return true;
}

int
main(int argc, const char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	MessageQueue		reports("reports");
	MonitorSettings		settings;
	settings.interval = Milliseconds(100);
	settings.stall = Milliseconds(300);
	settings.confirm = Milliseconds(100);
	settings.probe = fake_probe;
	settings.low_free = 600000;
	Monitor			monitor(reports, settings);

	printf("\nThe monitor looks, and reports what it finds once\n");
	VariantArray		memory = wait_report(reports, "memory", 2000);
	expect("it reports low memory from the probe", memory.length() == 5 && memory[2].as_long() == 500000 && memory[3].as_long() == 400000);
	{
		ReadWindow<MonitorData>	w(monitor.published, Milliseconds(1000));
		expect("a Window shows its data: it has looked, and found memory low",
			w.holding() && w->samples > 0 && w->memory.free_bytes == 500000 && w->findings.length() == 1);
		expect("...and counted the threads, queues and error buffers",
			w.holding() && w->threads >= 2 && w->queues >= 2);
	}
	expect("it does not say it again", wait_report(reports, "memory", 500).length() == 0);

	printf("\nShort of memory\n");
	fake_memory.largest_block = 100;			// Too little for a look
	VariantArray	shortage = wait_report(reports, "no-memory", 2000);
	expect("with too little memory it says so, in words",
		shortage.length() == 3 && shortage[2].as_strval() == "Not enough memory to report");
	{
		ReadWindow<MonitorData>	w(monitor.published, Milliseconds(1000));
		expect("...and a Window shows it skipped looking", w.holding() && w->short_of_memory && w->skipped > 0);
	}
	expect("...and does not say it again", wait_report(reports, "no-memory", 500).length() == 0);
	fake_memory.largest_block = 400000;
	Thread::yield(Milliseconds(400));
	{
		ReadWindow<MonitorData>	w(monitor.published, Milliseconds(1000));
		expect("once there is enough it looks again", w.holding() && !w->short_of_memory);
	}

	printf("\nA deadlock\n");
	SIXLock		first, second;
	Runner		one("one", [&]
	{
		ExclLock	a(first);
		Thread::yield(Milliseconds(200));
		ExclLock	b(second, Milliseconds(2500));	// Gives up, or this test would never end
	});
	Runner		two("two", [&]
	{
		ExclLock	a(second);
		Thread::yield(Milliseconds(200));
		ExclLock	b(first, Milliseconds(2500));
	});
	VariantArray	deadlock = wait_report(reports, "deadlock", 2000);
	expect("it reports the deadlock", deadlock.length() == 3);
	bool		both = false;
	if (deadlock.length() == 3)
	{
		VariantArray	names = deadlock[2].as_variant_array();
		bool		has_one = false, has_two = false;
		for (VariantArray::Index i = 0; i < names.length(); i++)
		{
			has_one |= names[i].as_strval() == "one";
			has_two |= names[i].as_strval() == "two";
		}
		both = names.length() == 2 && has_one && has_two;
	}
	expect("...naming both threads", both);
	{
		ReadWindow<MonitorData>	w(monitor.published, Milliseconds(1000));
		bool		shown = false;
		for (unsigned i = 0; w.holding() && i < w->findings.length(); i++)
			shown |= w->findings[i].as_variant_array()[1].as_strval() == "deadlock";
		expect("...and it is among the findings a Window shows", shown);
	}
	one.join();
	two.join();
	Thread::yield(Milliseconds(400));
	{
		ReadWindow<MonitorData>	w(monitor.published, Milliseconds(1000));
		bool		gone = w.holding();
		for (unsigned i = 0; w.holding() && i < w->findings.length(); i++)
			gone &= w->findings[i].as_variant_array()[1].as_strval() != "deadlock";
		expect("once the threads give up, it is no longer found", gone);
	}

	printf("\nA stall\n");
	SIXLock		lock;
	Latch		latch;
	Condition	nothing_comes;
	Runner		reader("reader", [&]
	{
		SharedLock	window(lock);
		latch.enter();
		long	ms = 1500;
		nothing_comes.wait(ms, &latch);
		latch.leave();
	});
	VariantArray	stall = wait_report(reports, "stall", 3000, "reader");
	expect("a thread waiting too long with a lock held is reported",
		stall.length() == 6 && stall[2].as_strval() == "reader" && stall[3].as_long() >= 300
		&& stall[4].as_strval() == "condition" && stall[5].as_int() == 1);
	reader.join();

	printf("\nA nearly full queue\n");
	MessageQueue	busy("busy");
	for (int i = 0; i < MSGQUEUE_HIGH_WATER-2; i++)
		busy.push(Variant(i));
	VariantArray	queue = wait_report(reports, "queue", 3000, "busy");
	expect("it is reported, with its depth", queue.length() == 6 && queue[2].as_strval() == "busy"
		&& queue[3].as_int() == MSGQUEUE_HIGH_WATER-2);
	while (!busy.isEmpty())
		busy.pop();

	printf("\nAn error buffer that grows\n");
	Runner		noisy("noisy", []
	{
		for (int i = 0; i < 9; i++)
			ErrorTHR_CreateFailed("something", i);
		Thread::yield(Milliseconds(500));
		ErrBuffer()->clear();		// A thread must not end with errors
	});
	VariantArray	errors = wait_report(reports, "errors", 3000, "noisy");
	expect("it is reported, with the thread that owns it",
		errors.length() == 5 && errors[2].as_strval() == "noisy" && errors[3].as_int() == 9);
	noisy.join();

	printf("\nEnding it\n");
	monitor.requests.push(Variant(VariantArray() << "quit"));
	expect("a quit request ends the thread", monitor.join() == 0);

	printf("\n%s\n", fails ? "FAILED" : "all monitor checks passed");
	return fails != 0;
}
