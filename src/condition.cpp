/*
 * Condition variable
 *
 * Any thread waiting on a condition variable sleeps until the condition is signalled.
 * When it is, one or all waiting threads continue. Until signalled threads have
 * continued, any thread that tries to signal it again will block.
 */
#include	<unistd.h>

#include	<thread.h>
#include	<condition.h>

Condition::~Condition()
{
#if	defined(HAVE_PTHREADS)
	pthread_cond_destroy(&cond);
#elif	defined(MSW)
	assert(waiters_count == 0);
	CloseHandle(hEvent);
#else
#error	"Not implemented"
#endif
}

Condition::Condition()
#if	defined(HAVE_PTHREADS)
	// Nothing to do here
#elif	defined(MSW)
: waiters_count(0)
, release_count(0)
, generation_count(0)
, hEvent(0)
#endif
{
#if	defined(HAVE_PTHREADS)
	pthread_cond_init(&cond, (pthread_condattr_t*)0);
#elif	defined(MSW)
	hEvent = CreateEventW(NULL, TRUE, FALSE, 0);
	assert(hEvent);
#else
#error	"Not implemented"
#endif
}

bool
Condition::ok() const
{
#if	defined(HAVE_PTHREADS)
	return true;
#elif	defined(MSW)
	return hEvent != 0;
#else
#error	"Not implemented"
	return FALSE;
#endif
}

void
Condition::wait(
	Latch*		user_latch
)
{
#if	defined(HAVE_PTHREADS)
	pthread_cond_wait(&cond, &user_latch->mutex);
#elif	defined(MSW)
	// Increment the count of waiters and grab the generation count:
	ThreadId	tid = Thread::currentId();
	latch.latch(tid);
	++waiters_count;
	int		my_generation = generation_count;
	latch.unlatch(tid);

	if (user_latch)
		user_latch->unlatch(tid);
	bool	done = FALSE;
	do
	{
		(void) WaitForSingleObject(hEvent, INFINITE);
		latch.latch(tid);
		done = release_count > 0
			&& my_generation != generation_count;
		latch.unlatch(tid);
	} while (!done);
	if (user_latch)
		user_latch->latch(tid);

	latch.latch(tid);
	--waiters_count;
	done = --release_count == 0;
	latch.unlatch(tid);
	if (done)
		ResetEvent(hEvent);
#else
#error	"Not implemented"
#endif
}

void
Condition::signal()
{
#if	defined(HAVE_PTHREADS)
	pthread_cond_signal(&cond);
#elif	defined(MSW)
	// Increment the count of waiters and grab the generation count:
	ThreadId	tid = Thread::currentId();
	latch.latch(tid);
	if (waiters_count > release_count)
	{
		SetEvent(hEvent);
		++release_count;	// Wake one thread only
		++generation_count;
	}
	latch.unlatch(tid);
#else
#error	"Not implemented"
#endif
}

void
Condition::broadcast()
{
#if	defined(HAVE_PTHREADS)
	pthread_cond_broadcast(&cond);
#elif	defined(MSW)
	// Increment the count of waiters and grab the generation count:
	ThreadId	tid = Thread::currentId();
	latch.latch(tid);
	if (waiters_count > 0)
	{
		SetEvent(hEvent);
		release_count = waiters_count;	// Release all waiters
		++generation_count;
	}
	latch.unlatch(tid);
#else
#error	"Not implemented"
#endif
}

void
Condition::wait(		// Wait for a ticket
	long&	timeout,
	Latch*	user_latch
)
{
#if	defined(HAVE_PTHREADS)
	int		retcode;
	struct timespec	ts;
	ts.tv_sec = (unsigned long)timeout / 1000;
	ts.tv_nsec = (unsigned long)timeout % 1000 * 1000000L;
	retcode = pthread_cond_timedwait(&cond, &user_latch->mutex, &ts);
	if (retcode == ETIMEDOUT)
	{
		timeout = 0;
	}
	else
	{
		// REVISIT: Subtract elapsed time from timeout
	}
#elif	defined(MSW)
	YMDTimeC	start = YMDTimeC::now();

	// Increment the count of waiters and grab the generation count:
	ThreadId	tid = Thread::currentId();
	latch.latch(tid);
	++waiters_count;
	int		my_generation = generation_count;
	latch.unlatch(tid);

	if (user_latch)
		user_latch->unlatch(tid);
	bool	done = FALSE;
	while (!done && (unsigned long)timeout > 0)
	{
		(void) WaitForSingleObject(hEvent, timeout);

		// Update the remaining time-to-wait:
		YMDTimeC	now = YMDTimeC::now();
		MillisecondsC	elapsed = now-start;
		start = now;
		timeout = elapsed >= timeout ? 0 : timeout-elapsed; // Calculate remaining time

		// Time to awake yet?
		latch.latch(tid);
		done = release_count > 0
			&& my_generation != generation_count;
		latch.unlatch(tid);
	}
	if (user_latch)
		user_latch->latch(tid);

	latch.latch(tid);
	--waiters_count;
	done = --release_count == 0;
	latch.unlatch(tid);
	if (done)
		ResetEvent(hEvent);
#else
#error	"Not implemented"
#endif
}
