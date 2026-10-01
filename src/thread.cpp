/*
 * Threads, a lightweight wrapper around platform functionality
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<thread.h>
#include	<condition.h>
#include	<strpp_msg.h>			// A thread the host would not create says so
#include	<errno.h>

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

/*
 * Under MSW the thread is created here, suspended, and started by resume() -
 * which is where its creation is reported, since this constructor also runs
 * for the main thread, during static initialisation, where the error buffer is
 * not to be relied on. A NULL handle is remembered and reported there.
 */
Thread::Thread(const ThreadParams* params)
: thread_id(0)
, state(New)
, stack_bytes(params ? params->stackBytes : 0)
, exit_code(0)
{
#if	defined(HAVE_PTHREADS) || defined(HAVE_FREERTOS)
	// Thread creation starts the thread immediately, before subclass construction has finished,
	// so don't do it here. The subclass should call resume() to start it.
#elif	defined(MSW)
	thread_handle = CreateThread(
				(SECURITY_ATTRIBUTES*)0,
				stack_bytes,	// 0 = default stack size (currently 1Mb)
				(LPTHREAD_START_ROUTINE)&Thread::ThreadProc,
				(void*)this,
				CREATE_SUSPENDED,// CreationFlags
				&thread_id
			);
	if (!thread_handle)
		thread_id = 0;		// CreateThread leaves it untouched when it fails
	else
	{
		thread_latch.enter();
		registerThread(this);
		thread_latch.leave();
	}
#else
	// NO_THREAD: there is one thread, and it is already running
#endif
}

/*
 * Start a thread that has not started yet. A host that will not create it says
 * so here: the thread is not running, and it is not registered either, so
 * looking it up will not find it and joining it is a mistake.
 *
 * The two attribute calls are reported separately, because a refused stack
 * size is not a failed creation: the thread is still made, with the host's
 * default stack.
 */
void
Thread::resume()
{
#if	defined(HAVE_PTHREADS)
	pthread_attr_t	attr;
	int		code = pthread_attr_init(&attr);
	if (code)
	{
		ErrorTHR_CreateFailed("pthread_attr_init", code);
		return;			// Nothing was started
	}

	if (stack_bytes && (code = pthread_attr_setstacksize(&attr, stack_bytes)) != 0)
		ErrorTHR_StackRefused("pthread_attr_setstacksize", stack_bytes, code);

	thread_latch.enter();
	void	*(*proc)(void *) = (void *(*)(void *))Thread::ThreadProc;	// pthread procs return void*
	code = pthread_create(&thread_id, &attr, proc, this);
	pthread_attr_destroy(&attr);

	if (code)
	{
		thread_id = 0;
		thread_latch.leave();
		ErrorTHR_CreateFailed("pthread_create", code);
		return;
	}
	registerThread(this);
	thread_latch.leave();
#elif	defined(HAVE_FREERTOS)
	thread_latch.enter();
	size_t		bytes = stack_bytes ? stack_bytes : THREAD_DEFAULT_STACK_BYTES;
	size_t		depth = bytes / sizeof(StackType_t);
	if (depth > (size_t)(configSTACK_DEPTH_TYPE)-1)		// The platform's count is a narrower field
	{
		ErrorTHR_StackRefused("xTaskCreate", bytes, 0);		// Truncated below
		depth = (size_t)(configSTACK_DEPTH_TYPE)-1;
	}
	BaseType_t	ok = xTaskCreate(
				Thread::ThreadProcTask,
				"Thread",			// REVISIT: allow a name to be supplied?
				(configSTACK_DEPTH_TYPE)depth,
				this,
				THREAD_DEFAULT_PRIORITY,
				&thread_id
			);
	if (ok != pdPASS)
	{
		thread_id = 0;
		thread_latch.leave();
		ErrorTHR_CreateFailed("xTaskCreate", (int)ok);
		return;
	}
	registerThread(this);
	thread_latch.leave();
#elif	defined(MSW)
	if (!thread_handle)
		ErrorTHR_CreateFailed("CreateThread", (int)GetLastError());
	else if (ResumeThread(thread_handle) == (DWORD)-1)
		ErrorTHR_CreateFailed("ResumeThread", (int)GetLastError());
#else
	assert(!"No threads can be started when there is no threading model");
#endif
}

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
	t->exit_code = ret;		// What join() returns, before the thread is joined
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

#if	defined(HAVE_FREERTOS)
void
Thread::ThreadProcTask(void* _this)	// TaskFunction_t's signature; ThreadProc returns int for the other models
{
	ThreadProc(_this);
}
#endif

/*
 * End the calling thread, with this code as its exit code - which join()
 * returns, and which is stored here because two of the three models have
 * nowhere of their own to put it.
 *
 * The thread ends without unwinding: the rest of run() does not happen, and
 * under pthreads neither do the destructors of the objects run() has
 * constructed on its stack (pthread_exit runs no C++ unwinding). A caller that
 * must release something before exiting does it before calling this.
 */
void Thread::exit(int code)
{
	Thread*		thread = current();
	if (!thread)
		return;		// exited before properly started
	thread_latch.enter();
	thread->exit_code = code;
	thread->state = Ended;
	thread = Thread::find(currentId());
	if (thread)
	{
		ended_count++;
		ended_threads_condition.broadcast();
	}
	thread_latch.leave();

#if	defined(HAVE_PTHREADS)
	pthread_exit(0);	// The code is recorded above, so the pthread value is not used
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
	if (threads.length() == 0)
#else
	if (threads.size() == 0)
#endif
	{
		thread_latch.leave();
		// The main thread is not registered so not counted here.
		return 0;
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

/*
 * Wait for this thread to end, and return its exit code: what run() returned,
 * or what exit() was given. A thread that never ran - or one whose wait failed
 * - returns 0, which is what a thread that ended with 0 also returns; the
 * report is what tells those apart.
 *
 * The thread is unregistered and the count of ended threads decremented only
 * when the wait actually succeeded: doing it after a failed wait is what drove
 * ended_count negative, which left joinAny() spinning.
 */
int
Thread::join()
{
	Id	caller = currentId();
	StrppAssert(caller != thread_id);	// Can't wait for self!
	StrppAssert(thread_id != 0);		// Can't join a thread that hasn't started

#if	defined(HAVE_PTHREADS)
	void*	retval; // Value returned from pthread_exit, or PTHREAD_CANCELLED
	int	error = pthread_join(thread_id, &retval);
	if (error)
	{
		ErrorTHR_JoinFailed("pthread_join", error);
		return 0;		// It did not end, so its exit code is not known
	}

#elif	defined(HAVE_FREERTOS)
	/*
	 * No native join primitive under FreeRTOS: wait on the same
	 * ended_count/ended_threads_condition machinery joinAny() uses,
	 * filtered to this specific thread_id.
	 */
	thread_latch.enter();
	for (;;)
	{
		Thread*	thread = Thread::find(thread_id);
		if (!thread || thread->state == Ended)
			break;
		ended_threads_condition.wait(&thread_latch);
	}
	thread_latch.leave();

#elif	defined(MSW)
	if (state != Ended)		// The thread hasn't finished yet, so wait
	{
		DWORD	wait = WaitForSingleObject(thread_handle, INFINITE);
		if (wait == WAIT_FAILED)
		{
			ErrorTHR_JoinFailed("WaitForSingleObject", (int)GetLastError());
			return 0;	// Perhaps someone else joined this thread before us
		}
	}

#endif

	thread_latch.enter();
	if (Thread::find(thread_id))
	{
		unregisterThread(thread_id);
		ended_count--;
	}
	thread_latch.leave();

	return exit_code;
}

/*
 * Give the processor up, having waited for the given time first where the host
 * can do that. A zero time - or a negative one, which is a caller's way of
 * saying "don't wait" - is a plain yield, and so is a null Milliseconds, which
 * is no time at all. The unit is the Millisecond class, so a caller cannot
 * pass seconds by mistake.
 */
void
Thread::yield(Milliseconds milliseconds)
{
#if	defined(HAVE_PTHREADS)
	Tick		ticks = milliseconds.asInterval().ticks();	// 10^-8 second ticks
	if (ticks < 0)			// Zero, or the null tick: no time at all
		ticks = 0;
	struct timespec	request, remaining;
	request.tv_sec = (time_t)(ticks / TicksPerSecond);
	request.tv_nsec = (long)(ticks % TicksPerSecond * 10);	// 10^-8 s to 10^-9 s
	while (nanosleep(&request, &remaining) == -1)
	{
		if (errno != EINTR)
		{
			ErrorTHR_DelayFailed("nanosleep", errno);
			return;
		}
		request = remaining;
	}
#elif	defined(HAVE_FREERTOS)
	unsigned long	ms = (unsigned long)milliseconds.ms();
	if (ms == 0)
		taskYIELD();
	else
		vTaskDelay(pdMS_TO_TICKS(ms));
#elif	defined(MSW)
	unsigned long	ms = (unsigned long)milliseconds.ms();
	if (ms == 0)
		ms = 1;			// Sleep(0) would yield only to an equal-priority thread
	Sleep(ms);
#else
	(void)milliseconds;	// NO_THREAD: there is no other thread to yield to
#endif
}
