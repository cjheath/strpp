#if !defined(WATCH_H)
#define WATCH_H
/*
 * The night Watch: Record each lock held or awaited by each thread so that
 * a monitor can find a deadlock or a thread stuck for too long.
 *
 * Configure STRPP_MONITOR to record it. If disabled, every hook compiles
 * to nothing and no record. Enabling it makes Latch, Condition and the
 * locks in lock.h tell this code when a thread starts waiting, when it gets
 * the lock, and when it lets it go.
 *
 * All code in the same program must be compiled with the same setting.
 *
 * A Thread records which locks it holds (by address and mode), and what it is
 * waiting for and since when. The record is made on the first lock being taken.
 * The recorded data uses only atomic instructions. A monitor reads without
 * stopping anyone, so it needs to double-check.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<stdint.h>

#include	<threadid.h>

#if	!defined(STRPP_WATCH_HELD)
#define	STRPP_WATCH_HELD	8	// Locks a thread's record can list; more are counted, not listed
#endif
#if	!defined(STRPP_WATCH_THREADS)
#define	STRPP_WATCH_THREADS	32	// Threads one snapshot can hold
#endif

// What kind of thing a thread is waiting for
enum	WatchKind
{
	WatchNothing = 0,
	WatchLatch,			// A Latch
	WatchLock,			// A Lock or SIXLock
	WatchCondition			// A Condition: nothing says who will signal it
};

// What the thread needs, so we know why it's blocked
enum	WatchMode
{
	WatchForShared,			// A shared hold: writers hold it up
	WatchForWriter,			// An intent or exclusive hold, or a Latch: writers hold it up
	WatchForDrain,			// An upgrade to exclusive: shared holds hold it up
	WatchForSignal			// A condition: no one in particular
};

// How a thread is holding a lock
enum	WatchHold
{
	HoldShared = 1,
	HoldWriter = 2			// A Latch, or an intent or exclusive hold
};

struct	WatchHeld
{
	const void*		object;
	unsigned char		mode;		// A WatchHold
};

// Snapshot of one thread reported by the monitor
struct	WatchRecord
{
	ThreadId		id;
	const char*		name;		// From ThreadParams, or null
	unsigned		held_count;	// How many entries of held are filled in
	unsigned		held_dropped;	// Holds that did not fit in held
	WatchHeld		held[STRPP_WATCH_HELD];
	WatchKind		wait_kind;
	WatchMode		wait_mode;
	const void*		wait_object;
	uint32_t		wait_since;	// WatchNowMs() when the wait began
};

/*
 * Concurrency analysis functions.
 * These read a snapshot and nothing else, so they work without STRPP_MONITOR,
 * and they allocate no memory.
 *
 * Return the number of WatchRecords written to `out`.
 *
 * WatchFindCycle finds one cycle of threads, each waiting for a lock
 * the next holds, and writes them in that order; it returns 0 when there is
 * none. A thread that waits for a lock only it holds is not reported.
 *
 * WatchFindStalls lists the threads that have waited at least threshold_ms,
 * only those holding a lock if only_holding.
 */
unsigned	WatchFindCycle(const WatchRecord* records, unsigned count, unsigned* out, unsigned max_out);
unsigned	WatchFindStalls(
			const WatchRecord* records, unsigned count,
			uint32_t now_ms, uint32_t threshold_ms, bool only_holding,
			unsigned* out, unsigned max_out
		);

#if	defined(STRPP_MONITOR)

uint32_t	WatchNowMs();			// A clock that does not go back, in ms; it wraps
unsigned	WatchSnapshot(WatchRecord* out, unsigned max_out);	// Every live thread's record

void		watch_thread_start(const char* name);	// This is a strpp Thread beginning
void		watch_thread_exit();			// ...and ending: give its record back
void		watch_held_add(const void* object, WatchHold mode);
void		watch_held_remove(const void* object);

struct	ThreadWatch;

// Mark the calling thread as waiting for the life of this object. Only the
// outermost wait is recorded: a SIXLock's own Latch and Condition are not.
class	WatchWait
{
public:
	WatchWait(const void* object, WatchKind kind, WatchMode mode);
	~WatchWait();
	void		retarget(WatchMode mode);	// The same wait, now needing something else

	WatchWait(const WatchWait&) = delete;
	WatchWait&	operator=(const WatchWait&) = delete;

private:
	ThreadWatch*	record;
	bool		outermost;
};

#else

inline void	watch_thread_start(const char*)		{}
inline void	watch_thread_exit()			{}
inline void	watch_held_add(const void*, WatchHold)	{}
inline void	watch_held_remove(const void*)		{}

class	WatchWait
{
public:
	WatchWait(const void*, WatchKind, WatchMode)	{}
	void		retarget(WatchMode)		{}
};

#endif

#endif	// WATCH_H
