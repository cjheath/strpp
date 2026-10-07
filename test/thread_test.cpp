/*
 * Threads: creation, joining, yield, and the condition variable.
 *
 * The fan-out exercises concurrent creation, the registry and joinAny(). The
 * checks around it are what the threading layer had none of: join() returning
 * a thread's exit code, exit() ending the running thread, yield() waiting the
 * time it is given, and a timed condition wait giving up when nothing signals.
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<cstring>
#include	<time.h>

#include	<lockfree.h>
#include	<thread.h>
#include	<condition.h>
#include	<condition_signal_test.h>

#define	FANOUT	25	// This many primary threads will each create this many again. total of N*(N+1)

static int	fails = 0;

static void
expect(const char* what, bool ok)
{
	if (!ok)
		fails++;
	printf("  %-56s %s\n", what, ok ? "ok" : "FAIL");
}

static void
expect_int(const char* what, long got, long want)
{
	expect(what, got == want);
	if (got != want)
		printf("      wanted %ld, got %ld\n", want, got);
}

// Milliseconds on the same clock the timed wait uses
static long
now_ms()
{
	struct timespec	ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (long)(ts.tv_sec*1000 + ts.tv_nsec/1000000);
}

class	HelloThread
	: public Thread
{
	int	count;
public:
	HelloThread(int a_count = 0)
	: count(a_count)
	{ resume(); }

	int	run()
	{
		printf(
			"Hello, world, from thread 0x%llX\n",
			(long long)thread_id
		);
		yield();
		for (int i = 0; i < count; i++)
			(void)new HelloThread();
		printf(
			"Goodbye, cruel world, from thread 0x%llX\n",
			(long long)thread_id
		);
		return 0;
	}
};

// A thread whose exit code is the one it was given: join() returns it
class	CodeThread
	: public Thread
{
	int	code;
public:
	CodeThread(int a_code)
	: code(a_code)
	{ resume(); }

	int	run() { return code; }
};

// A thread that ends itself from inside run(): everything after exit() must
// not happen, and join() returns the code it was given
class	ExitingThread
	: public Thread
{
public:
	ExitingThread() { resume(); }

	int	run()
	{
		exit(7);
		return 99;		// Must not be reached, and must not be the result
	}
};

// Says the condition is signalled, after a delay
class	Signaller
	: public Thread
{
	Condition*	condition;
	long		delay_ms;
public:
	Signaller(Condition* a_condition, long a_delay)
	: condition(a_condition)
	, delay_ms(a_delay)
	{ resume(); }

	int	run()
	{
		yield(Milliseconds(delay_ms));
		condition->signal();
		return 0;
	}
};

static void
join_tests()
{
	printf("\nThread::join returns the exit code\n");
	{
		CodeThread*	thread = new CodeThread(42);
		expect_int("run()'s value is what join() returns", thread->join(), 42);
		delete thread;
	}
	{
		ExitingThread*	exiting = new ExitingThread();
		expect_int("exit()'s code is what join() returns", exiting->join(), 7);
		delete exiting;
	}
}

// Reaches into the registry to play a host that hands a thread's id to a new thread once the old one has ended
class	IdProbe
	: public Thread
{
public:
	int	run() { return 7; }
	void	start() { resume(); }
	bool	ended() { return state == Ended; }

	// This thread, which has not been started, takes over the id of an ended thread that has not been joined
	void	take_over(IdProbe& old)
	{
		thread_latch.enter();
		thread_id = old.thread_id;
		state = Started;
		registerThread(this);
		thread_latch.leave();
	}
	bool	registered() { return find(thread_id) == this; }
	int	ended_total() { return ended_count; }
	void	abandon()	// Leave the registry, and look as if never started
	{
		thread_latch.enter();
		if (find(thread_id) == this)
			unregisterThread(thread_id);
		thread_latch.leave();
		thread_id = 0;
	}
};

static void
reused_id_tests()
{
	printf("\nA new thread that takes over the id of an ended, unjoined one\n");
	IdProbe	old;
	old.start();
	while (!old.ended())
		Thread::yield(Milliseconds(5));
	int	before = old.ended_total();

	IdProbe	fresh;
	fresh.take_over(old);
	expect("the registry finds the new thread by that id", fresh.registered());
	expect_int("the old thread is no longer counted as ended", fresh.ended_total(), before-1);

	expect_int("the old thread still joins", old.join(), 7);
	expect("...and the new thread is still registered", fresh.registered());
	expect_int("...and the ended count is as it was", fresh.ended_total(), before-1);

	fresh.abandon();
}

static void
yield_tests()
{
	printf("\nThread::yield\n");
	long	start = now_ms();
	Thread::yield(Milliseconds(50));
	long	waited = now_ms()-start;
	expect("yield(50ms) waits at least 40ms", waited >= 40);
	expect("yield(50ms) does not wait far longer", waited < 1000);

	start = now_ms();
	Thread::yield();
	waited = now_ms()-start;
	expect("yield() does not wait", waited < 50);
}

static void
condition_signal_tests()
{
	printf("\nCondition::signal\n");
	ConditionSignalResult	r = condition_signal_test();
	expect_int("one signal wakes one of two waiters", r.after_signal, 1);
	expect_int("a broadcast wakes the one that is left", r.after_broadcast, 2);
	expect_int("a later signal still wakes a new waiter", r.after_later_signal, 3);
}

static void
condition_tests()
{
	printf("\nCondition\n");
	Condition	condition;
	expect("a condition variable was created", condition.ok());

	Latch	latch;

	// Nothing signals this one, so it must give up after its timeout
	latch.enter();
	long	timeout = 50;
	long	start = now_ms();
	condition.wait(timeout, &latch);
	long	waited = now_ms()-start;
	latch.leave();
	expect_int("a wait that times out leaves no time to wait", timeout, 0);
	expect("...and waited at least 40ms", waited >= 40);
	expect("...and not far longer than it was asked to", waited < 1000);

	// This one is signalled after 200ms, well inside a 5 second timeout, so it
	// returns early and hands back the time it did not use
	Signaller	signaller(&condition, 200);
	latch.enter();
	timeout = 5000;
	start = now_ms();
	condition.wait(timeout, &latch);
	waited = now_ms()-start;
	latch.leave();
	signaller.join();
	expect("a signalled wait returns before its timeout", waited < 4000);
	expect("...and leaves the time it did not use", timeout > 0);
}

int
main(int argc, const char** argv)
{
	// Ensure that stdout gets flushed:
        setvbuf(stdout, 0, _IONBF, 0);

	printf(
		"ProcessID is %d, Main thread is 0x%llX\n",
		Thread::currentProcessId(),
		(long long)Thread::main()->id()
	);

	join_tests();
	reused_id_tests();
	yield_tests();
	condition_tests();
	condition_signal_tests();

	// The fan-out: FANOUT threads each create FANOUT more, and every one of
	// them is drained through joinAny()
	for (int i = 0; i < FANOUT; i++)
                (void)new HelloThread(FANOUT);

        Thread*        ended = 0;
        while ((ended = Thread::joinAny()) != 0)
        {
		// REVISIT: Show any outstanding errors on this thread
                delete ended;
                printf("Ended %p\n", ended);
        }

	printf("\n%s\n", fails ? "FAILED" : "all thread checks passed");
	return fails != 0;
}
