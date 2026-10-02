/*
 * Lock and SIXLock: shared, intent and exclusive locks, single-threaded for the
 * rules and with other threads for the blocking.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<time.h>

#include	<thread.h>
#include	<lock.h>

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

static const Milliseconds	poll(0);

static void
rule_tests()
{
	printf("\nWhich locks can be held together\n");
	SIXLock		lock;
	{
		SharedLock	s1(lock);
		SharedLock	s2(lock);
		expect("two SharedLocks are held together", s1.holding() && s2.holding() && lock.sharing() == 2);

		IntentLock	i(lock, poll);
		expect("an IntentLock is granted beside SharedLocks", i.holding() && lock.writing());
		SharedLock	s3(lock, poll);
		expect("...but then no new SharedLock is", !s3.holding() && lock.sharing() == 2);
		IntentLock	i2(lock, poll);
		expect("...and no second IntentLock", !i2.holding());

		ExclLock	x(i, poll);
		expect("an upgrade waits for the SharedLocks", !x.holding() && i.holding());
		s1.release();
		s2.release();
		expect("releasing them leaves none", lock.sharing() == 0);
		ExclLock	x2(i, poll);
		expect("...then the upgrade succeeds", x2.holding() && !i.holding());
		SharedLock	s4(lock, poll);
		expect("an ExclLock holds off a SharedLock", !s4.holding());
	}
	expect("everything released leaves the lock free", !lock.writing() && lock.sharing() == 0);

	{
		SharedLock	s(lock);
		ExclLock	x(lock, poll);
		expect("an ExclLock is not granted beside a SharedLock", !x.holding());
		expect("...and a failed ExclLock leaves no writer behind", !lock.writing());
	}
	{
		ExclLock	x(lock);
		expect("an ExclLock on a free lock is granted", x.holding() && lock.writing());
		ExclLock	moved(static_cast<ExclLock&&>(x));
		expect("a moved lock is held by the new object only", moved.holding() && !x.holding());
	}
	expect("a moved lock is released once", !lock.writing());

	Lock		plain;
	{
		ExclLock	x(plain);
		ExclLock	x2(plain, poll);
		expect("a plain Lock is exclusive", x.holding() && !x2.holding());
	}
}

// Holds a lock for a while, then lets go
class	Holder
: public Thread
{
	SIXLock&	lock;
	bool		intent;
	long		hold_ms;
public:
	Holder(SIXLock& a_lock, bool an_intent, long a_hold)
	: lock(a_lock), intent(an_intent), hold_ms(a_hold)
	{ resume(); }

	int	run()
	{
		if (intent)
		{
			IntentLock	i(lock);
			yield(Milliseconds(hold_ms));
		}
		else
		{
			SharedLock	s(lock);
			yield(Milliseconds(hold_ms));
		}
		return 0;
	}
};

static void
blocking_tests()
{
	printf("\nBlocking between threads\n");
	{
		SIXLock		lock;
		Holder		holder(lock, false, 200);
		Thread::yield(Milliseconds(50));		// Let it take the lock
		long		start = now_ms();
		ExclLock	x(lock);
		long		waited = now_ms()-start;
		holder.join();
		expect("an ExclLock waits for another thread's SharedLock", x.holding() && waited >= 100 && waited < 4000);
	}
	{
		SIXLock		lock;
		Holder		holder(lock, true, 200);
		Thread::yield(Milliseconds(50));
		long		start = now_ms();
		SharedLock	s(lock);
		long		waited = now_ms()-start;
		holder.join();
		expect("a SharedLock waits for another thread's IntentLock", s.holding() && waited >= 100 && waited < 4000);
	}
	{
		SIXLock		lock;
		Holder		holder(lock, false, 200);
		Thread::yield(Milliseconds(50));
		long		start = now_ms();
		SharedLock	s(lock);
		long		waited = now_ms()-start;
		holder.join();
		expect("SharedLocks in two threads do not wait", s.holding() && waited < 100);
	}
	{
		SIXLock		lock;
		Holder		holder(lock, true, 300);
		Thread::yield(Milliseconds(50));
		long		start = now_ms();
		SharedLock	s(lock, Milliseconds(50));
		long		waited = now_ms()-start;
		holder.join();
		expect("a timed SharedLock gives up", !s.holding() && waited >= 40 && waited < 250);
	}
}

int
main(int argc, const char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	rule_tests();
	blocking_tests();

	printf("\n%s\n", fails ? "FAILED" : "all lock checks passed");
	return fails != 0;
}
