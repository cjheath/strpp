/*
 * Transactional: a reader sees a thread's data only between its messages, and holds
 * the thread off while the Window is open.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<time.h>

#include	<thread.h>
#include	<msgqueue.h>
#include	<transactional.h>

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
{
	long		work_ms;
public:
	MessageQueue		requests;
	Transactional<AdderData>	published;

	Adder(long a_work_ms) : work_ms(a_work_ms) { resume(); }

	int	run()
	{
		for (;;)
		{
			Variant		request = requests.pop();
			if (request.is_null() || request.as_int() < 0)
				return 0;
			published.update([&](AdderData& d)
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
		ReadWindow<AdderData>	w(adder.published);
		expect("a ReadWindow opens on an idle thread", w.holding());
		expect("...and shows its data", w->total == 0 && (*w).handled == 0);
		ReadWindow<AdderData>	w2(adder.published, Milliseconds(0));
		expect("two ReadWindows are open together", w2.holding());
	}

	adder.requests.push(Variant(5));	// The thread starts handling this: 300ms of work
	Thread::yield(Milliseconds(50));
	long		start = now_ms();
	ReadWindow<AdderData>	w(adder.published);
	long		waited = now_ms()-start;
	expect("a ReadWindow waits while a message is handled", waited >= 150 && waited < 4000);
	expect("...and then sees the result", w->total == 5 && w->handled == 1);

	adder.requests.push(Variant(7));	// Cannot be handled while w is open
	Thread::yield(Milliseconds(500));
	expect("an open ReadWindow holds off the next message", w->total == 5 && w->handled == 1);
	ReadWindow<AdderData>	timed(adder.published, Milliseconds(0));
	expect("...and the thread, now with a message, admits no new ReadWindow", !timed.holding());
	w.close();

	Thread::yield(Milliseconds(500));
	ReadWindow<AdderData>	after(adder.published);
	expect("closing the ReadWindow lets the message through", after->total == 12 && after->handled == 2);
	after.close();

	adder.requests.push(Variant(-1));
	adder.join();
}

static void
free_standing_tests()
{
	printf("\nA Transactional that belongs to no thread\n");
	Transactional<AdderData>	tx;

	{
		UpdateWindow<AdderData>	u(tx);
		expect("an UpdateWindow opens on an idle value", u.holding());
		u->total = 3;
		(*u).handled = 1;
		expect("...and no other Window opens meanwhile",
			!ReadWindow<AdderData>(tx, Milliseconds(0)).holding()
			&& !UpdateWindow<AdderData>(tx, Milliseconds(0)).holding());
	}
	{
		ReadWindow<AdderData>	r(tx);
		expect("a ReadWindow sees what the update wrote", r->total == 3 && r->handled == 1);
		expect("...and holds off any UpdateWindow", !UpdateWindow<AdderData>(tx, Milliseconds(0)).holding());
	}

	tx.update([](AdderData& d) { d.total += 4; });
	expect("update() runs its function in an UpdateWindow", tx.unguarded().total == 7);

	Transactional<AdderData>	copy_made((AdderData()));
	expect("a Transactional starts with the value you give it", copy_made.unguarded().total == 0);
}

int
main(int argc, const char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	window_tests();
	free_standing_tests();

	printf("\n%s\n", fails ? "FAILED" : "all window checks passed");
	return fails != 0;
}
