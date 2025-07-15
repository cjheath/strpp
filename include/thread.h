#if !defined(THREAD_H)
#define THREAD_H
/*
 * Threads, a lightweight wrapper around platform functionality
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<map>
#include	<assert.h>

#if	!defined(THREAD_ID)
#define	THREAD_ID
#if	defined(HAVE_PTHREADS)
#include	<unistd.h>
#include	<pthread.h>

typedef pthread_t	ThreadId;
typedef pid_t		ProcessId;
#elif	defined(MSW)
typedef DWORD		ThreadId;
typedef ThreadId	ProcessId;
//#else Add more threading systems here
#endif
#endif

#include	<lockfree.h>

class	Condition;

class	Thread
{
public:
	using	Id = ThreadId;

	typedef enum { New, Started, Ended } State;

	inline virtual		~Thread();
	inline			Thread();
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

	static	Thread*		main_thread;
	static	Latch		thread_latch;		// control access to threads map
	static	std::map<ThreadId, Thread*> threads;
	static	std::atomic<int> ended_count;
	static  Condition	ended_threads_condition;

	static	inline	Thread*	find(ThreadId);

#if	defined(HAVE_PTHREADS)
	static void*		ThreadProc(void* _this);
#elif	defined(MSW)
	static int		ThreadProc(void* _this);
#endif
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

Thread::Thread()
{
#if	defined(HAVE_PTHREADS)
	// pthread_create starts the thread immediately, before subclass construction has finished,
	// so don't do it here. The subclass should call resume() to start it.
#elif	defined(MSW)
	thread_handle = CreateThread(
				(SECURITY_ATTRIBUTES*)0,
				0,		// Default stack size 1Mb
				(LPTHREAD_START_ROUTINE)&Thread::ThreadProc,
				(void*)this,
				CREATE_SUSPENDED,// CreationFlags
				&thread_id
			);
	// if (!thread_handle) { Error(ERR_THREAD_CREATE, ...); }	// REVISIT:

	thread_id = tid;
	thread_latch.enter();
	threads.insert({thread_id, this});
	thread_latch.leave();
	if (thread_handle)
		ResumeThread(thread_handle);
#endif
}

Thread::~Thread()
{
	if (this == main_thread)
		return;
#if	defined(HAVE_PTHREADS)
	assert(state == Ended);
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
#elif	defined(MSW)
	if (thread_handle)
		SuspendThread(thread_handle);	// suspend the thread
#endif	
}	

void
Thread::resume()
{
#if	defined(HAVE_PTHREADS)
	thread_latch.enter();
	int	code = pthread_create(&thread_id, (pthread_attr_t*)0, Thread::ThreadProc, this);
	// if (!code) { Error(ERR_THREAD_CREATE, ...); }		// REVISIT: Report failure to start thread

	threads.insert({thread_id, this});
	thread_latch.leave();
#elif	defined(MSW)
	if (thread_handle)
		ResumeThread(thread_handle);	// (re)start the thread
#endif
}

Thread*
Thread::find(ThreadId tid)
{
	auto it = threads.find(tid);
	return it != threads.end() ? it->second : 0;
}

void
Thread::remove_ended()
{
	thread_latch.enter();
	if (Thread::find(thread_id))
	{
		if (state == Ended)
			ended_count--;
		threads.erase(thread_id);	// Remove from map
	}
	thread_latch.leave();
	thread_id = 0;
}

ThreadId Thread::currentId()
{
#if	defined(HAVE_PTHREADS)
	return pthread_self();
#elif	defined(MSW)
	return GetCurrentThreadId();
#endif
}

ProcessId Thread::currentProcessId()
{
#if	defined(HAVE_PTHREADS)
	return getpid();
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
