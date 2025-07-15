/*
 * Threads, a lightweight wrapper around platform functionality
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<map>
#include	<thread.h>
#include	<condition.h>

Thread*		Thread::main_thread;
Latch		Thread::thread_latch;
std::map<ThreadId, Thread*> Thread::threads;
MainThread	main_thread;
std::atomic<int> Thread::ended_count;
Condition	Thread::ended_threads_condition;

#if	defined(HAVE_PTHREADS)
void*
#elif	defined(MSW)
int
#endif
Thread::ThreadProc(void* _this)
{
	Thread* t = (Thread*)_this;

#if	defined(HAVE_PTHREADS)
	// This code can be run *before* pthread_create has assigned the thread_id. Wait for it.
	thread_latch.enter();
	assert(t->id());
	thread_latch.leave();
#elif	defined(MSW)
	assert(t->handle());
#endif

	t->state = Started;

#if	defined(HAVE_PTHREADS)
	void*	ret = (void*)(long)t->run();
#elif	defined(MSW)
	int	ret = t->run();
#endif

	thread_latch.enter();
	t->state = Ended;
	// Increment count of ended threads and signal the condition for any waiters
	// Only increment zombie_threads if we're in the thread map.
	// If a thread's destructor is called before it exits, this
	// won't be true and joinAny will hang.	 Of course, if the
	if (Thread::find(t->thread_id))
	{
		ended_count++;
		ended_threads_condition.broadcast();
	}
	thread_latch.leave();
	return ret;
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
#elif	defined(MSW)
	ExitThread(code);
#endif
}

Thread*
Thread::joinAny()
{
	thread_latch.enter();
	if (threads.size() <= 1)
	{
		thread_latch.leave();
		return 0;	// No threads except main
	}

	// Search for a thread whose state is Ended
	// Remove it from the map and decrement the ended_threads count
	for (;;)
	{
		if (ended_count == 0)	// None have exited but not been joined
		{
			ended_threads_condition.wait(&thread_latch);
			continue;
		}

		// Find any ended thread, remove it from the map and return it
		for (auto it = threads.begin(); it != threads.end(); it++)
		{
			Thread* thread = it->second;
			if (!thread || thread->state != Ended)
				continue;
			// threads.erase(it);
			Id	tid = it->first;
			threads.erase(tid);
			assert(!Thread::find(tid));
			ended_count--;
			thread_latch.leave();
			return thread;
		}
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
		int erased = threads.erase(thread_id);
		assert(erased == 1);
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
		int erased = threads.erase(thread_id);
		assert(erased == 1);
		ended_count--;
	}
	thread_latch.leave();

#endif
}
