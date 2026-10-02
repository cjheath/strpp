#if !defined(LOCKFREE_H)
#define LOCKFREE_H
/*
 * Lockfree locking primitives based on atomic transfers
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<atomic>
#include	<assert.h>

#include	<threadid.h>
#include	<strassert.h>			// A latch that cannot be taken stops the program
#include	<watch.h>			// What a monitor needs to know about who holds and waits

#if	defined(HAVE_FREERTOS)
#include	<freertos/semphr.h>
#elif	defined(MSW)
#include	<windows.h>			// SYSTEM_INFO/GetSystemInfo, for get_num_cores()
#endif

class Latch
{
public:
	inline Latch();
	inline ~Latch();

	// A latch may not be copied: the copy would hold the same mutex, and
	// destroying either would destroy it under the other
	Latch(const Latch&) = delete;
	Latch& operator=(const Latch&) = delete;

	inline bool		probe();	// Gain the latch if possible immediately
	inline void		enter();	// Wait for the latch
	inline bool		holding();	// Latch is held by calling thread?
	inline void		leave();	// Release the latch

#if	defined(HAVE_PTHREADS)
	// The PTHREADs implementation allows us to use these with pthread condition variables
	pthread_mutex_t	mutex;

	static	std::atomic<bool>	initialised;
	static	pthread_mutexattr_t	attr;

#elif	defined(HAVE_FREERTOS)
	/*
	 * Use a real (recursive) mutex, not the CAS-spin-loop fallback below: FreeRTOS is
	 * a priority-preemptive RTOS, and a spinlock risks genuine priority inversion (a
	 * low-priority holder starved forever by a higher-priority busy-spinner) that a
	 * real mutex with priority inheritance avoids. Recursive (rather than pthreads'
	 * error-checking, which fails a second same-thread lock with EDEADLK) because
	 * FreeRTOS's plain mutex has no self-deadlock detection at all - a second
	 * xSemaphoreTake() from the same task would simply hang forever, which recursive
	 * locking avoids. See the note in the implementation plan for this trade-off.
	 */
	SemaphoreHandle_t	mutex;

#elif	defined(MSW)
	// On Windows, condition variables are based on system events so we can do this using atomic
	std::atomic<ThreadId>	mutex;

	inline ThreadId		value() volatile const { return mutex; }
	inline ThreadId		probe(ThreadId);
	inline void		latch(ThreadId);
	inline void		unlatch(ThreadId);

	// Number of cores is used when deciding whether to spin before yielding
	static	int	get_num_cores();
	static	int	num_cores;
#else
	/*
	 * NO_THREAD, or no model selected at all - in which case thread.h reports
	 * it. With one thread there is nothing to lock, so this is the whole of it.
	 */
#endif
};

#if	defined(HAVE_PTHREADS)
Latch::Latch()
{
	if (!initialised)
	{
		initialised = true;
		pthread_mutexattr_init(&Latch::attr);
		pthread_mutexattr_settype(&Latch::attr, PTHREAD_MUTEX_ERRORCHECK);
	}

	pthread_mutex_init(&mutex, &attr);
}

Latch::~Latch()
{
	pthread_mutex_destroy(&mutex);
}

bool
Latch::probe()		// Gain the latch if possible immediately
{
	bool	got = pthread_mutex_trylock(&mutex) == 0;
	if (got)
		watch_held_add(this, HoldWriter);
	return got;
}

void
Latch::enter()		// Wait for the latch
{
	/*
	 * A latch that cannot be taken leaves the critical section it protects
	 * unprotected, and there is no result this can return that the caller
	 * could act on - so it stops, with the reason reported. EDEADLK is the
	 * caller's own recursive enter(), which this (error-checking) mutex
	 * refuses by design.
	 */
	WatchWait	waiting(this, WatchLatch, WatchForWriter);
	int	ret = pthread_mutex_lock(&mutex);
	StrppAssert(ret == 0);
	watch_held_add(this, HoldWriter);
}

bool
Latch::holding()	// Latch is held by calling thread?
{
	int ret = pthread_mutex_lock(&mutex);
	if (ret == EDEADLK)
		return true;
	if (ret == 0)
		pthread_mutex_unlock(&mutex);
	return false;
}

void
Latch::leave()		// Release the latch
{
	// Holding it is the whole of the check: this thread must have it. Without
	// the check, leaving a latch another thread holds would acquire and then
	// release it, quietly breaking that thread's critical section.
	StrppAssert(pthread_mutex_lock(&mutex) == EDEADLK);
	watch_held_remove(this);
	pthread_mutex_unlock(&mutex);
}

#elif	defined(HAVE_FREERTOS)
Latch::Latch()
{
	mutex = xSemaphoreCreateRecursiveMutex();
	assert(mutex);
}

Latch::~Latch()
{
	vSemaphoreDelete(mutex);
}

bool
Latch::probe()		// Gain the latch if possible immediately
{
	bool	got = xSemaphoreTakeRecursive(mutex, 0) == pdTRUE;
	if (got)
		watch_held_add(this, HoldWriter);
	return got;
}

void
Latch::enter()		// Wait for the latch
{
	// A latch that cannot be taken leaves the critical section it protects
	// unprotected, and there is no result to give the caller: stop, reported
	WatchWait	waiting(this, WatchLatch, WatchForWriter);
	BaseType_t	ok = xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
	StrppAssert(ok == pdTRUE);
	watch_held_add(this, HoldWriter);
}

bool
Latch::holding()	// Latch is held by calling thread?
{
	return xSemaphoreGetMutexHolder(mutex) == xTaskGetCurrentTaskHandle();
}

void
Latch::leave()		// Release the latch
{
	// Failing here means this thread did not hold it, or held it fewer times
	// than it has left it: either way the latch's state is not what the caller
	// believes, so there is no safe way to carry on
	watch_held_remove(this);
	BaseType_t	ok = xSemaphoreGiveRecursive(mutex);
	StrppAssert(ok == pdTRUE);
}

#elif	defined(MSW)
/*
 * REVISIT: this branch cannot compile as it stands, and nothing in this tree
 * builds MSW, so nothing has noticed. It calls Thread::currentId() and
 * Thread::yield(), and Thread is declared in thread.h, which includes *this*
 * header before anything else - so the class is not in scope here. Fixing it
 * means moving this half of Latch into a .cpp, or moving Latch into thread.h.
 */

#define	LATCH_SPIN_COUNT	1000	// 1000 volatile decrements delay
#define	LATCH_YIELD_SLEEP	1	// 1 millisecond

Latch::Latch()
: mutex(0)
{ }

Latch::~Latch()
{ }

// Gain the latch for this thread if possible immediately, or return false
bool
Latch::probe()
{
	return probe(Thread::currentId()) == 0;
}

// Wait until we can gain the latch for this thread
void
Latch::enter()
{
	latch(Thread::currentId());
}

// Is the latch held by the current thread?
bool
Latch::holding()
{
	return value() == Thread::currentId();
}

// Release the latch which must be held by the current thread
void
Latch::leave()
{
	unlatch(Thread::currentId());
}

// Set the latch to tid if it was free (and return 0), otherwise return the existing value
ThreadId
Latch::probe(ThreadId tid)
{
	ThreadId	expected(0);
	return mutex.compare_exchange_strong(expected, tid) ? 0 : expected;
}

void
Latch::latch(ThreadId tid)		// Wait for the latch
{
	ThreadId	holder;
	while ((holder = probe(tid)) != 0)	// Take it if it is free
	{		// Some other thread has the latch
		assert(holder != tid);
		// Instead of yielding immediately, if there are other cores, spin a while first
		if (get_num_cores() > 1)
		{
			volatile int	count = LATCH_SPIN_COUNT;
			while ((--count > 0) && value() != 0)
				;
			if (count != 0)
				continue;	// Latch appears free, try now
		}
		Thread::yield(LATCH_YIELD_SLEEP);	// Latch still held, wait a bit
	}
}

void
Latch::unlatch(ThreadId tid)		// Release the latch
{
	// Only the holder can release it, and a failed exchange means this is not it
	bool	unlatched_ok = mutex.compare_exchange_strong(tid, 0);
	StrppAssert(unlatched_ok);
}

int
Latch::get_num_cores()
{
	if (num_cores > 0)
		return num_cores;

	SYSTEM_INFO	si;
	GetSystemInfo(&si);
	num_cores = si.dwNumberOfProcessors > 0 ? (int)si.dwNumberOfProcessors : 1;
	return num_cores;
}

#else	/* NO_THREAD, or no model selected */

inline Latch::Latch()			{ }
inline Latch::~Latch()			{ }
inline bool	Latch::probe()		{ return true; }	// Always free: there is one thread
inline void	Latch::enter()		{ }
inline bool	Latch::holding()	{ return true; }	// It is always this thread
inline void	Latch::leave()		{ }

#endif

#endif	/* LOCKFREE_H */
