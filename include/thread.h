#if !defined(THREAD_H)
#define THREAD_H
/*
 * Threads, a lightweight wrapper around platform functionality
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<assert.h>

#include	<threadid.h>
#include	<lockfree.h>

#if	defined(HAVE_FREERTOS)
#include	<array.h>
#else
#include	<cowmap.h>
#endif

class	Condition;

/*
 * Optional parameters to the Thread constructor. Currently just the stack
 * size, applied on every backend (pthreads: pthread_attr_setstacksize();
 * Windows: CreateThread's size argument; FreeRTOS: xTaskCreate's stack
 * depth, converted from bytes to StackType_t words).
 */
struct	ThreadParams
{
	size_t	stackBytes = 0;		// 0 = platform default; see THREAD_DEFAULT_STACK_BYTES
};

/*
 * THREAD_DEFAULT_STACK_BYTES is used whenever stackBytes is 0 (no ThreadParams
 * given, or stackBytes left at its default). On pthreads/Windows, 0 there just
 * means "let the OS choose its own normal default stack size" - only FreeRTOS
 * requires an actual number, since xTaskCreate has no notion of a "default".
 * Both macros can be set from the Makefile (see COPT), the same way HAVE_PTHREADS is.
 */
#if	!defined(THREAD_DEFAULT_STACK_BYTES)
#if	defined(HAVE_FREERTOS)
#define	THREAD_DEFAULT_STACK_BYTES	4096
#else
#define	THREAD_DEFAULT_STACK_BYTES	0
#endif
#endif
#if	!defined(THREAD_DEFAULT_PRIORITY)
#define	THREAD_DEFAULT_PRIORITY	1	// Only meaningful under HAVE_FREERTOS
#endif

/*
 * On FreeRTOS, an Array of threads is created. If you define MAX_THREAD,
 * that sets the initial size of the array, which won't be reallocated
 * if you don't exceed that maximum. This reduces the OOM hazard a little.
 * If you want the array used in other cases, define USE_THREAD_ARRAY.
 */
#if	defined(HAVE_FREERTOS)
#define	USE_THREAD_ARRAY
#endif

class	Thread
{
public:
	using	Id = ThreadId;

	typedef enum { New, Started, Ended } State;

	inline virtual		~Thread();
	inline			Thread(const ThreadParams* params = 0);
	virtual int		run() = 0;	// Override this

	ThreadId		id() const { return thread_id; }
	inline void		suspend();	// All threads start suspended
	inline void		resume();	// Constructor should resume()
	void			join();		// Wait for this thread to end

	static	inline void		yield(unsigned long milliseconds = 0);
	static	Thread*			joinAny();
	static	inline ThreadId		currentId();	// current thread id, fast
	static	inline ProcessId	currentProcessId();
	static	inline Thread*		current();	// current thread, slower
	inline void			exit(int);	// exit the current thread
	static	inline Thread*		main();		// main thread. REVISIT: needed?

	// REVISIT: Iterate over all live threads
	// REVISIT: Return count of live threads

protected:
	ThreadId		thread_id;
	State			state;
	size_t			stack_bytes;	// From ThreadParams, or 0 (platform default)

	static	Thread*		main_thread;
	static	Latch		thread_latch;		// control access to threads registry
#if	defined(USE_THREAD_ARRAY)
	static	Array<Thread*>		threads;
#else
	static	CowMap<Thread*, ThreadId> threads;
#endif
	static	std::atomic<int> ended_count;
	static  Condition	ended_threads_condition;

	static	inline	Thread*	find(ThreadId);
	static	inline	void	registerThread(Thread*);	// Add to the registry; caller holds thread_latch
	static	inline	void	unregisterThread(ThreadId);	// Remove from the registry if present; caller holds thread_latch

	static int		ThreadProc(void* _this);
	inline void		remove_ended();

	// REVISIT: Implement error buffer:
	// ErrBuf*		err_buf;
	// VariantArray		arglist;

	// REVISIT: atomic counter for ended threads, and a condition variable for access?

#if	defined(MSW)
	HANDLE			thread_handle;
	HANDLE			handle() const { return thread_handle; }
#endif
};

class MainThread
: public Thread
{
public:
	MainThread()
	{
		thread_id = Thread::currentId();
		state = Started;
		main_thread = this;	// Override prior initialisation in superclass constructor
#if	defined(MSW)
		if (!DuplicateHandle(
			GetCurrentProcess(),
			GetCurrentThread(),
			GetCurrentProcess(),
			&thread_handle,
			0,
			FALSE,
			DUPLICATE_SAME_ACCESS
		))
			thread_handle = 0;

#endif
	}

	~MainThread() { state = Ended; }
	int	run() { assert(!"Can't run main thread"); }
};

/*
 * Under FreeRTOS, xTaskGetCurrentTaskHandle() (used by currentId(), below)
 * is only meaningful once the scheduler is actually running the calling task -
 * so MainThread must be constructed from a task that's already running after
 * vTaskStartScheduler() has started it (e.g. that task's own entry function),
 * not from code that runs before the scheduler starts. There's no portable way
 * to detect "has the scheduler started yet" here, so this is a usage
 * requirement, not something this class can enforce.
 */
Thread::Thread(const ThreadParams* params)
: thread_id(0)
, state(New)
, stack_bytes(params ? params->stackBytes : 0)
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
	// if (!thread_handle) { Error(ERR_THREAD_CREATE, ...); }	// REVISIT:

	thread_id = tid;
	thread_latch.enter();
	registerThread(this);
	thread_latch.leave();
	if (thread_handle)
		ResumeThread(thread_handle);
#endif
}

Thread::~Thread()
{
	if (this == main_thread)
		return;
#if	defined(HAVE_PTHREADS) || defined(HAVE_FREERTOS)
	assert(state == Ended);

	/*
	 * REVISIT: FreeRTOS can force-terminate another task via vTaskDelete(), similar to
	 * Windows' TerminateThread below, but with the same hazards (can leave latches held
	 * forever) - not done here, matching pthreads (which has no such capability at all),
	 * because it is intrinsically unsafe anyway.
	 */
#elif	defined(MSW)
	if (state != Ended)
	{		// It is possible to kill a thread on Windows but seriously ill-advised, because it can leave latches set, etc.
		if (thread_handle)
			TerminateThread(thread_handle, 1);	// Can also use GetLastError to check for errors
	}
	if (thread_handle)
		CloseHandle(thread_handle);
	thread_handle = 0;
#endif
	remove_ended();
}

void
Thread::suspend()
{
#if	defined(HAVE_PTHREADS)
	// REVISIT: Not possible
#elif	defined(HAVE_FREERTOS)
	vTaskSuspend(thread_id);
#elif	defined(MSW)
	if (thread_handle)
		SuspendThread(thread_handle);	// suspend the thread
#endif
}

void
Thread::resume()
{
#if	defined(HAVE_PTHREADS)
	pthread_attr_t	attr;
	pthread_attr_init(&attr);
	if (stack_bytes)
		pthread_attr_setstacksize(&attr, stack_bytes);

	thread_latch.enter();
	void	*(*proc)(void *) = (void *(*)(void *))Thread::ThreadProc;	// pthread procs return void*
	int	code = pthread_create(&thread_id, &attr, proc, this);
	// if (!code) { Error(ERR_THREAD_CREATE, ...); }		// REVISIT: Report failure to start thread
	pthread_attr_destroy(&attr);

	registerThread(this);
	thread_latch.leave();
#elif	defined(HAVE_FREERTOS)
	thread_latch.enter();
	size_t		bytes = stack_bytes ? stack_bytes : THREAD_DEFAULT_STACK_BYTES;
	BaseType_t	ok = xTaskCreate(
				(TaskFunction_t)Thread::ThreadProc,
				"Thread",			// REVISIT: allow a name to be supplied?
				bytes / sizeof(StackType_t),
				this,
				THREAD_DEFAULT_PRIORITY,
				&thread_id
			);
	// if (ok != pdPASS) { Error(ERR_THREAD_CREATE, ...); }	// REVISIT: Report failure to start thread
	(void)ok;
	registerThread(this);
	thread_latch.leave();
#elif	defined(MSW)
	if (thread_handle)
		ResumeThread(thread_handle);	// (re)start the thread
#endif
}

Thread*
Thread::find(ThreadId tid)
{
#if	defined(USE_THREAD_ARRAY)
	for (Array<Thread*>::Index i = 0; i < threads.length(); i++)
		if (threads[i]->id() == tid)
			return threads[i];
	return 0;
#else
	auto it = threads.find(tid);
	return it != threads.end() ? it->second : 0;
#endif
}

void
Thread::registerThread(Thread* t)
{
#if	defined(USE_THREAD_ARRAY)
	// Don't enforce MAX_THREAD
	// assert(threads.length() < MAX_THREAD);
	threads.push(t);
#else
	threads.insert(t->id(), t);
#endif
}

void
Thread::unregisterThread(ThreadId tid)
{
#if	defined(USE_THREAD_ARRAY)
	for (Array<Thread*>::Index i = 0; i < threads.length(); i++)
		if (threads[i]->id() == tid)
		{
			threads.remove(i, 1);
			return;
		}
#else
	threads.remove(tid);
#endif
}

void
Thread::remove_ended()
{
	thread_latch.enter();
	if (Thread::find(thread_id))
	{
		if (state == Ended)
			ended_count--;
		unregisterThread(thread_id);	// Remove from registry
	}
	thread_latch.leave();
	thread_id = 0;
}

ThreadId Thread::currentId()
{
#if	defined(HAVE_PTHREADS)
	return pthread_self();
#elif	defined(HAVE_FREERTOS)
	return xTaskGetCurrentTaskHandle();
#elif	defined(MSW)
	return GetCurrentThreadId();
#endif
}

ProcessId Thread::currentProcessId()
{
#if	defined(HAVE_PTHREADS)
	return getpid();
#elif	defined(HAVE_FREERTOS)
	return 0;	// No concept of a "process" under FreeRTOS
#elif	defined(MSW)
	return GetCurrentProcessId();
#endif
}

void Thread::yield(unsigned long milliseconds)
{
#if	defined(HAVE_PTHREADS)
	struct timespec request, remaining;
	request.tv_sec = (unsigned long)milliseconds/1000;
	request.tv_nsec = (unsigned long)milliseconds%1000*1000000;
	while (nanosleep(&request, &remaining) == -1 && errno == EINTR)
		request = remaining;
#elif	defined(HAVE_FREERTOS)
	if (milliseconds == 0)
		taskYIELD();
	else
		vTaskDelay(pdMS_TO_TICKS(milliseconds));
#elif	defined(MSW)
	if (milliseconds == 0)
		milliseconds = 1;
	Sleep(milliseconds);
#endif
}

Thread* Thread::current()
{
	Thread*		thread;
	thread_latch.enter();
	thread = Thread::find(currentId());
	thread_latch.leave();
	return thread;
}

Thread* Thread::main()
{
	return main_thread;
}

#endif
