/*
 * Threads, a lightweight wrapper around platform functionality
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<thread.h>
#include	<condition.h>

Thread*		Thread::main_thread;
Latch		Thread::thread_latch;

/*
 * On FreeRTOS, an Array of threads is created. If you define MAX_THREAD,
 * that sets the initial size of the array, which won't be reallocated
 * if you don't exceed that maximum. This reduces the OOM hazard a little.
 * If you want the array used in other cases, define USE_THREAD_ARRAY.
 */
#if	defined(HAVE_FREERTOS)
#define	USE_THREAD_ARRAY
#endif

#if	defined(USE_THREAD_ARRAY)
#if	defined(MAX_THREAD)
Array<Thread*>	Thread::threads((Thread**)0, 0, MAX_THREAD);	// Allocated exactly once, at startup
#else
Array<Thread*>	Thread::threads;
#endif
#else
CowMap<Thread*, ThreadId> Thread::threads;
#endif

MainThread	main_thread;
std::atomic<int> Thread::ended_count;
Condition	Thread::ended_threads_condition;

int
Thread::ThreadProc(void* _this)
{
	Thread* t = (Thread*)_this;

#if	defined(HAVE_PTHREADS) || defined(HAVE_FREERTOS)
	/*
	 * This code can be run *before* the thread/task creation call has assigned
	 * the thread_id (resume() holds thread_latch across that call and the
	 * following registerThread(), so waiting for the latch here guarantees
	 * thread_id is set by the time we read it).
	 */
	thread_latch.enter();
	assert(t->id());
	thread_latch.leave();
#elif	defined(MSW)
	assert(t->handle());
#endif

	t->state = Started;

	int	ret = t->run();

	thread_latch.enter();
	t->state = Ended;

	/*
	 * Increment count of ended threads and signal the condition for any waiters.
	 * Only increment zombie_threads if we're in the thread registry.
	 * If a thread's destructor is called before it exits, this won't be true
	 * and joinAny will hang.
	 */
	if (Thread::find(t->thread_id))
	{
		ended_count++;
		ended_threads_condition.broadcast();
	}
	thread_latch.leave();

#if	defined(HAVE_PTHREADS)
	return ret;
#elif	defined(HAVE_FREERTOS)
	vTaskDelete(0);		// FreeRTOS task functions must never simply return
	return ret;		// Not reached on real hardware; keeps this well-formed for the compile-check stub
#elif	defined(MSW)
	return ret;
#endif
}

void Thread::exit(int code)
{
	Thread*		thread = current();
	if (!thread)
		return;		// exited before properly started
	thread_latch.enter();
	thread->state = Ended;
	thread = Thread::find(currentId());
	if (thread)
	{
		ended_count++;
		ended_threads_condition.broadcast();
	}
	thread_latch.leave();

#if	defined(HAVE_PTHREADS)
#elif	defined(HAVE_FREERTOS)
	vTaskDelete(0);
#elif	defined(MSW)
	ExitThread(code);
#endif
}

Thread*
Thread::joinAny()
{
	thread_latch.enter();
#if	defined(USE_THREAD_ARRAY)
	if (threads.length() <= 1)
#else
	if (threads.size() <= 1)
#endif
	{
		thread_latch.leave();
		return 0;	// No threads except main
	}

	// Search for a thread whose state is Ended
	// Remove it from the registry and decrement the ended_threads count
	for (;;)
	{
		if (ended_count == 0)	// None have exited but not been joined
		{
			ended_threads_condition.wait(&thread_latch);
			continue;
		}

		// Find any ended thread, remove it from the registry and return it
#if	defined(USE_THREAD_ARRAY)
		for (Array<Thread*>::Index i = 0; i < threads.length(); i++)
		{
			Thread* thread = threads[i];
			if (!thread || thread->state != Ended)
				continue;
			threads.remove(i, 1);
			ended_count--;
			thread_latch.leave();
			return thread;
		}
#else
		for (auto it = threads.begin(); it != threads.end(); it++)
		{
			Thread* thread = it->second;
			if (!thread || thread->state != Ended)
				continue;
			Id	tid = it->first;
			threads.remove(tid);
			assert(!Thread::find(tid));
			ended_count--;
			thread_latch.leave();
			return thread;
		}
#endif
	}
	// Never returns by this path
}

void
Thread::join()		// Wait for this thread to end
{
	Id	caller = currentId();
	assert(caller != thread_id);	// Can't wait for self!
	assert(thread_id != 0);		// Can't join a thread that hasn't started

#if	defined(HAVE_PTHREADS)
	void*	retval; // Value returned from pthread_exit, or PTHREAD_CANCELLED
	int	error = pthread_join(thread_id, &retval);
	if (error)
		; // posixError("join", error, thread_id);	// Translate and report the error

	thread_latch.enter();
	Thread* thread = Thread::find(thread_id);
	if (thread)
	{
		unregisterThread(thread_id);
		ended_count--;
	}
	thread_latch.leave();

#elif	defined(HAVE_FREERTOS)
	/*
	 * No native join primitive under FreeRTOS: wait on the same
	 * ended_count/ended_threads_condition machinery joinAny() uses,
	 * filtered to this specific thread_id.
	 */
	thread_latch.enter();
	Thread*	thread;
	for (;;)
	{
		thread = Thread::find(thread_id);
		if (!thread || thread->state == Ended)
			break;
		ended_threads_condition.wait(&thread_latch);
	}
	if (thread)
	{
		unregisterThread(thread_id);
		ended_count--;
	}
	thread_latch.leave();

#elif	defined(MSW)
	if (state < THREAD_STATE_ENDED) // Thread hasn't finished yet, so wait
	{
		int	wait = WaitForSingleObject(thread_handle, INFINITE);
		if (wait == WAIT_FAILED)
			return; // Perhaps someone else joined this thread before us?
	}
	thread_latch.enter();
	Thread* thread = Thread::find(thread_id);
	if (thread)
	{
		unregisterThread(thread_id);
		ended_count--;
	}
	thread_latch.leave();

#endif
}
