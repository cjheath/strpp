#if !defined(CONDITION_SIGNAL_TEST_H)
#define CONDITION_SIGNAL_TEST_H
/*
 * One signal on a Condition releases one waiter, even when every waiter wakes
 * before the signaller lets go of the latch. Shared by thread_test.cpp and the
 * FreeRTOS console, so that it runs on every threading model.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<lockfree.h>
#include	<thread.h>
#include	<condition.h>

// Waits once on a condition, and counts itself if a signal woke it before its timeout
class	CountingWaiter
	: public Thread
{
public:
	struct	Shared
	{
		Condition	condition;
		Latch		latch;
		int		woken;		// Guarded by latch
		Shared() : woken(0) { }
	};

	CountingWaiter(Shared* a_shared)
	: shared(a_shared)
	{ resume(); }

	int	run()
	{
		shared->latch.enter();
		long	timeout = 3000;
		shared->condition.wait(timeout, &shared->latch);
		if (timeout > 0)
			shared->woken++;
		shared->latch.leave();
		return 0;
	}

private:
	Shared*		shared;
};

// How many waiters had woken at each of the three points the test looks
struct	ConditionSignalResult
{
	int	after_signal;		// Wanted 1: one signal, two waiters
	int	after_broadcast;	// Wanted 2: the waiter that is left
	int	after_later_signal;	// Wanted 3: a new waiter, and one more signal
};

inline int
condition_signal_woken(CountingWaiter::Shared& shared)
{
	shared.latch.enter();
	int	woken = shared.woken;
	shared.latch.leave();
	return woken;
}

inline ConditionSignalResult
condition_signal_test()
{
	ConditionSignalResult	result;
	CountingWaiter::Shared	shared;
	CountingWaiter		a(&shared), b(&shared);
	Thread::yield(Milliseconds(300));		// Both are waiting

	// Holding the latch keeps both woken threads from returning, so
	// neither has given back its ticket when the other looks for one
	shared.latch.enter();
	shared.condition.signal();
	Thread::yield(Milliseconds(300));
	shared.latch.leave();
	Thread::yield(Milliseconds(300));
	result.after_signal = condition_signal_woken(shared);

	// Whatever the signal left behind must not cost the next one
	shared.condition.broadcast();			// Releases the waiter that is left
	a.join();
	b.join();
	result.after_broadcast = condition_signal_woken(shared);

	CountingWaiter		c(&shared);
	Thread::yield(Milliseconds(300));
	shared.latch.enter();
	shared.condition.signal();
	shared.latch.leave();
	c.join();
	result.after_later_signal = condition_signal_woken(shared);
	return result;
}

#endif /* CONDITION_SIGNAL_TEST_H */
