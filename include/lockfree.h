#if !defined(LOCKFREE_H)
#define LOCKFREE_H
/*
 * Lockfree locking primitives based on atomic transfers
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<atomic>
#include	<assert.h>

#if	!defined(THREAD_ID)
#define	THREAD_ID
#if	defined(HAVE_PTHREADS)
#include        <unistd.h>
#include        <pthread.h>

typedef pthread_t       ThreadId;
typedef	pid_t		ProcessId;
#elif	defined(MSW)
typedef DWORD		ThreadId;
typedef	ThreadId	ProcessId;
//#else Add more threading systems here
#endif
#endif

class Latch
{
public:
	inline Latch();
	inline ~Latch();

	inline bool		probe();	// Gain the latch if possible immediately
	inline void		enter();	// Wait for the latch
	inline bool		holding();	// Latch is held by calling thread?
	inline void		leave();	// Release the latch

#if	defined(HAVE_PTHREADS)
	// The PTHREADs implementation allows us to use these with pthread condition variables
	pthread_mutex_t	mutex;

	static	std::atomic<bool>	initialised;
	static	pthread_mutexattr_t	attr;

#else
	// On Windows, condition variables are based on system events so we can do this using atomic
	std::atomic<ThreadId>	mutex;

	inline ThreadId		value() volatile const { return mutex; }
	inline ThreadId		probe(ThreadId);
	inline void		latch(ThreadId);
	inline void		unlatch(ThreadId);

	// Number of cores is used when deciding whether to spin before yielding
	static	int	get_num_cores();
	static	int	num_cores;
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
	return pthread_mutex_trylock(&mutex) == 0;
}

void
Latch::enter()		// Wait for the latch
{
	int ret = pthread_mutex_lock(&mutex);
	assert(ret != EDEADLK);
}

bool
Latch::holding()	// Latch is held by calling thread?
{
	int ret = pthread_mutex_lock(&mutex);
	if (ret == EDEADLK)
		return true;
	if (ret == 0)
		leave();
	return false;
}

void
Latch::leave()		// Release the latch
{
	assert(pthread_mutex_lock(&mutex) == EDEADLK);
	pthread_mutex_unlock(&mutex);
}

#else	/* Not HAVE_PTHREADS */

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
bool
Latch::probe(ThreadId tid)
{
	ThreadId	expected(0);
	return mutex.compare_exchange_strong(expected, tid) ? 0 : expected;
}

void
Latch::latch(ThreadId tid)		// Wait for the latch
{
	ThreadId	holder;
	while ((holder = probe()) != 0)
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
	bool	unlatched_ok = mutex.compare_exchange_strong(tid, 0);
	assert(unlatched_ok);
}

int
Latch::get_num_cores()
{
	if (num_cores > 0)
		return num_cores;
#if	defined(MSW)
	SYSTEM_INFO	si;
	GetSystemInfo(&si);
	return num_cores == si.dwNumberOfProcessors;
#else
	num_cores = 1;
#endif
}

#endif	/* Not HAVE_PTHREAD */

#endif	/* LOCKFREE_H */
