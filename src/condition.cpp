/*
 * Condition variable
 *
 * Any thread waiting on a condition variable sleeps until the condition is signalled.
 * When it is, one or all waiting threads continue. Until signalled threads have
 * continued, any thread that tries to signal it again will block.
 */
#include	<unistd.h>
#include	<time.h>

#include	<thread.h>
#include	<condition.h>
#include	<str_msg.h>			// A condition that was never created says so

/*
 * With one thread - NO_THREAD, or no model selected at all, which thread.h
 * reports - every method is a no-op defined in the header, and there is nothing
 * to define here.
 */
#if	defined(HAVE_PTHREADS) || defined(HAVE_FREERTOS) || defined(MSW)


#if	defined(HAVE_FREERTOS)
/*
 * The bit used within the FreeRTOS event group to signal waiters. Only one
 * bit is needed - which waiter(s) actually wake and continue is governed by
 * the generation-count algorithm below, not by which bit was set.
 */
#define	CONDITION_EVENT_BIT	((EventBits_t)0x01)
#endif

Condition::~Condition()
{
#if	defined(HAVE_PTHREADS)
	pthread_cond_destroy(&cond);
#elif	defined(HAVE_FREERTOS)
	assert(waiters_count == 0);
	vEventGroupDelete(eventGroup);
#elif	defined(MSW)
	assert(waiters_count == 0);
	CloseHandle(hEvent);
#else
#error	"Not implemented"
#endif
}

/*
 * The failure is recorded rather than reported: this may be constructed during
 * static initialisation (the library's own ended_threads_condition is), where
 * the error buffer is not to be relied on. ok() tells the truth about it, and
 * the first wait or signal that finds it reports it.
 */
Condition::Condition()
#if	defined(HAVE_PTHREADS)
: init_error(0)
#elif	defined(HAVE_FREERTOS)
: waiters_count(0)
, release_count(0)
, generation_count(0)
, eventGroup(0)
#elif	defined(MSW)
: waiters_count(0)
, release_count(0)
, generation_count(0)
, hEvent(0)
#endif
{
#if	defined(HAVE_PTHREADS)
	init_error = pthread_cond_init(&cond, (pthread_condattr_t*)0);
#elif	defined(HAVE_FREERTOS)
	eventGroup = xEventGroupCreate();
#elif	defined(MSW)
	hEvent = CreateEventW(NULL, TRUE, FALSE, 0);
#else
#error	"Not implemented"
#endif
}

bool
Condition::ok() const
{
#if	defined(HAVE_PTHREADS)
	return init_error == 0;
#elif	defined(HAVE_FREERTOS)
	return eventGroup != 0;
#elif	defined(MSW)
	return hEvent != 0;
#else
#error	"Not implemented"
	return false;
#endif
}

/*
 * A condition variable whose primitive was never made has nothing to wait on
 * and nothing to signal, and calling the platform with it is undefined. Every
 * use asks this first; the first one to find out says so, and returns with the
 * safe nothing.
 */
bool
Condition::usable(const char* operation) const
{
	if (ok())
		return true;
	ErrorTHR_NoCondition(operation);
	return false;
}

void
Condition::wait(
	Latch*		user_latch
)
{
	// Waiting means giving up the latch while another thread changes the
	// condition; with no latch there is nothing to give up, and the platform
	// needs one to wait on
	StrppAssert(user_latch);
	if (!usable("wait on"))
		return;

#if	defined(HAVE_PTHREADS)
	int		retcode = pthread_cond_wait(&cond, &user_latch->mutex);
	if (retcode)
		ErrorTHR_WaitFailed("pthread_cond_wait", retcode);
#elif	defined(HAVE_FREERTOS)
	// Increment the count of waiters and grab the generation count:
	latch.enter();
	++waiters_count;
	int		my_generation = generation_count;
	latch.leave();

	if (user_latch)
		user_latch->leave();
	bool	released = false;
	do
	{
		(void) xEventGroupWaitBits(eventGroup, CONDITION_EVENT_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
		latch.enter();
		released = release_count > 0
			&& my_generation != generation_count;
		latch.leave();
	} while (!released);
	if (user_latch)
		user_latch->enter();

	// Only a waiter that was released holds a ticket to hand back
	latch.enter();
	--waiters_count;
	bool	last = released && --release_count == 0;
	latch.leave();
	if (last)
		xEventGroupClearBits(eventGroup, CONDITION_EVENT_BIT);
#elif	defined(MSW)
	// Increment the count of waiters and grab the generation count:
	ThreadId	tid = Thread::currentId();
	latch.latch(tid);
	++waiters_count;
	int		my_generation = generation_count;
	latch.unlatch(tid);

	if (user_latch)
		user_latch->unlatch(tid);
	bool	released = false;
	do
	{
		DWORD	wait = WaitForSingleObject(hEvent, INFINITE);
		if (wait == WAIT_FAILED)
		{
			ErrorTHR_WaitFailed("WaitForSingleObject", (int)GetLastError());
			break;
		}
		latch.latch(tid);
		released = release_count > 0
			&& my_generation != generation_count;
		latch.unlatch(tid);
	} while (!released);
	if (user_latch)
		user_latch->latch(tid);

	// Only a waiter that was released holds a ticket to hand back
	latch.latch(tid);
	--waiters_count;
	bool	last = released && --release_count == 0;
	latch.unlatch(tid);
	if (last)
		ResetEvent(hEvent);
#else
#error	"Not implemented"
#endif
}

void
Condition::signal()
{
#if	defined(HAVE_PTHREADS)
	int		code = pthread_cond_signal(&cond);
	if (code)	// EINVAL: a condition variable that was never made
		ErrorTHR_NoCondition("signal");
#elif	defined(HAVE_FREERTOS)
	latch.enter();
	if (waiters_count > release_count)
	{
		xEventGroupSetBits(eventGroup, CONDITION_EVENT_BIT);
		++release_count;	// Wake one thread only
		++generation_count;
	}
	latch.leave();
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
	int		code = pthread_cond_broadcast(&cond);
	if (code)	// EINVAL: a condition variable that was never made
		ErrorTHR_NoCondition("broadcast");
#elif	defined(HAVE_FREERTOS)
	latch.enter();
	if (waiters_count > 0)
	{
		xEventGroupSetBits(eventGroup, CONDITION_EVENT_BIT);
		release_count = waiters_count;	// Release all waiters
		++generation_count;
	}
	latch.leave();
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
	/*
	 * pthread_cond_timedwait wants an absolute deadline on the condition
	 * variable's clock - CLOCK_REALTIME, unless its attributes say otherwise -
	 * and not a duration. Handing it the duration makes every wait return at
	 * once, since 1970 is long past.
	 */
	struct timespec	start, deadline;
	clock_gettime(CLOCK_REALTIME, &start);
	deadline.tv_sec = start.tv_sec + timeout/1000;
	deadline.tv_nsec = start.tv_nsec + timeout%1000 * 1000000L;
	if (deadline.tv_nsec >= 1000000000L)
	{
		deadline.tv_sec++;
		deadline.tv_nsec -= 1000000000L;
	}

	int		retcode = pthread_cond_timedwait(&cond, &user_latch->mutex, &deadline);
	if (retcode == 0)
	{
		// Signalled, so hand back the time that was not used
		struct timespec	now;
		clock_gettime(CLOCK_REALTIME, &now);
		long	elapsed = (long)((now.tv_sec-start.tv_sec)*1000 + (now.tv_nsec-start.tv_nsec)/1000000L);
		timeout = elapsed >= timeout ? 0 : timeout-elapsed;
	}
	else
	{
		if (retcode != ETIMEDOUT)
			ErrorTHR_WaitFailed("pthread_cond_timedwait", retcode);
		timeout = 0;		// Timed out, or the wait failed: there is nothing left to wait for
	}
#elif	defined(HAVE_FREERTOS)
	if (timeout == 0)
		return;			// Nothing to wait for, and so no ticket taken

	TickType_t	start = xTaskGetTickCount();

	// Increment the count of waiters and grab the generation count:
	latch.enter();
	++waiters_count;
	int		my_generation = generation_count;
	latch.leave();

	if (user_latch)
		user_latch->leave();
	bool	released = false;
	while (!released && (unsigned long)timeout > 0)
	{
		(void) xEventGroupWaitBits(eventGroup, CONDITION_EVENT_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout));

		// Update the remaining time-to-wait:
		TickType_t	now = xTaskGetTickCount();
		unsigned long	elapsed = (unsigned long)(now-start) * portTICK_PERIOD_MS;
		start = now;
		timeout = elapsed >= (unsigned long)timeout ? 0 : timeout-elapsed; // Calculate remaining time

		// Time to awake yet?
		latch.enter();
		released = release_count > 0
			&& my_generation != generation_count;
		latch.leave();
	}
	if (user_latch)
		user_latch->enter();

	// Only a waiter that was released holds a ticket to hand back
	latch.enter();
	--waiters_count;
	bool	last = released && --release_count == 0;
	latch.leave();
	if (last)
		xEventGroupClearBits(eventGroup, CONDITION_EVENT_BIT);
#elif	defined(MSW)
	if (timeout == 0)
		return;			// Nothing to wait for, and so no ticket taken

	DateTime	start = DateTime::now();

	// Increment the count of waiters and grab the generation count:
	ThreadId	tid = Thread::currentId();
	latch.latch(tid);
	++waiters_count;
	int		my_generation = generation_count;
	latch.unlatch(tid);

	if (user_latch)
		user_latch->unlatch(tid);
	bool	released = false;
	while (!released && (unsigned long)timeout > 0)
	{
		DWORD	wait = WaitForSingleObject(hEvent, (DWORD)timeout);
		if (wait == WAIT_FAILED)
		{
			ErrorTHR_WaitFailed("WaitForSingleObject", (int)GetLastError());
			break;
		}

		// Update the remaining time-to-wait:
		DateTime	now = DateTime::now();
		Milliseconds	elapsed(Interval(now - start));
		start = now;
		timeout = elapsed.ms() >= timeout ? 0 : timeout-(long)elapsed.ms(); // Calculate remaining time

		// Time to awake yet?
		latch.latch(tid);
		released = release_count > 0
			&& my_generation != generation_count;
		latch.unlatch(tid);
	}
	if (user_latch)
		user_latch->latch(tid);

	// Only a waiter that was released holds a ticket to hand back
	latch.latch(tid);
	--waiters_count;
	bool	last = released && --release_count == 0;
	latch.unlatch(tid);
	if (last)
		ResetEvent(hEvent);
#else
#error	"Not implemented"
#endif
}

#endif
