/*
 * Window: a reader sees a thread's data only between its messages, and holds
 * the thread off while the Window is open.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<time.h>

#include	<thread.h>
#include	<msgqueue.h>
#include	<window.h>

static int	fails = 0;

static void
expect(const char* what, bool ok)
{
	if (!ok)
		fails++;
	printf("  %-56s %s\n", what, ok ? "ok" : "FAIL");
}

static long
now_ms()
{
	struct timespec	ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (long)(ts.tv_sec*1000 + ts.tv_nsec/1000000);
}

struct	AdderData
{
	AdderData() : total(0), handled(0) {}
	int	total;
	int	handled;
};

// Adds each number it is sent to its total, taking a while over it
class	Adder
: public Thread
, public Windowed<Adder, AdderData>
{
	long		work_ms;
public:
	MessageQueue	requests;

	Adder(long a_work_ms) : work_ms(a_work_ms) { resume(); }

	int	run()
	{
		for (;;)
		{
			Variant		request = requests.pop();
			if (request.is_null() || request.as_int() < 0)
				return 0;
			update([&](Data& d)
			{
				yield(Milliseconds(work_ms));	// Slow on purpose: holds Windows off
				d.total += request.as_int();
				d.handled++;
			});
		}
	}
};

static void
window_tests()
{
	printf("\nWindows on a thread's data\n");
	Adder		adder(300);

	{
		Window<Adder>	w(adder);
		expect("a Window opens on an idle thread", w.holding());
		expect("...and shows its data", w->total == 0 && (*w).handled == 0);
		Window<Adder>	w2(adder, Milliseconds(0));
		expect("two Windows are open together", w2.holding());
	}

	adder.requests.push(Variant(5));	// The thread starts handling this: 300ms of work
	Thread::yield(Milliseconds(50));
	long		start = now_ms();
	Window<Adder>	w(adder);
	long		waited = now_ms()-start;
	expect("a Window waits while a message is handled", waited >= 150 && waited < 4000);
	expect("...and then sees the result", w->total == 5 && w->handled == 1);

	adder.requests.push(Variant(7));	// Cannot be handled while w is open
	Thread::yield(Milliseconds(500));
	expect("an open Window holds off the next message", w->total == 5 && w->handled == 1);
	Window<Adder>	timed(adder, Milliseconds(0));
	expect("...and the thread, now with a message, admits no new Window", !timed.holding());
	w.close();

	Thread::yield(Milliseconds(500));
	Window<Adder>	after(adder);
	expect("closing the Window lets the message through", after->total == 12 && after->handled == 2);
	after.close();

	adder.requests.push(Variant(-1));
	adder.join();
}

int
main(int argc, const char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	window_tests();

	printf("\n%s\n", fails ? "FAILED" : "all window checks passed");
	return fails != 0;
}
