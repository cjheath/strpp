#if !defined(MONITOR_H)
#define MONITOR_H
/*
 * Monitor: a thread that watches the others, and says what it finds.
 *
 * Only with STRPP_MONITOR: see watch.h. A Monitor wakes every interval and
 * looks for:
 *
 *	deadlock	Threads each waiting for a lock, or for room in a queue, that
 *			the next one holds or pops. Seen twice, a moment apart, so a
 *			wait that merely overlapped is not reported.
 *	stall		A thread that has waited too long for anything while holding a lock.
 *	queue		A MessageQueue that is nearly full, or has a pusher waiting.
 *	errors		An error buffer with many errors that no one has dealt with.
 *	memory		Free memory below what you set, if you gave it a way to ask.
 *
 * Looking and reporting both allocate. If you gave it a way to ask about memory,
 * it checks that the largest free block can hold what a look needs before it
 * starts, and when it cannot it looks at nothing and says so once instead:
 *	["monitor", "no-memory", "Not enough memory to report"]
 * until there is enough again. That message is made when the monitor starts, so
 * saying it allocates nothing itself, but the queue it is pushed to might.
 *
 * It reports each finding once, when it first appears, as a message
 * ["monitor", kind, ...] pushed to the queue you gave it. It never waits for
 * that queue: when it is full, the report is counted in `dropped` and lost.
 * What it has found now is also in its data, which you read through a Window.
 *
 * The messages are:
 *	["monitor", "deadlock", [thread name, ...]]
 *	["monitor", "stall", thread name, milliseconds waited, what, locks held]
 *	["monitor", "queue", queue name, depth, peak, pushers waiting]
 *	["monitor", "errors", thread name, errors, parameters]
 *	["monitor", "memory", bytes free, largest block, lowest free ever]
 *	["monitor", "no-memory", "Not enough memory to report"]
 * A name that was not given is "(unnamed)".
 *
 * Send it ["sample"] to look now, or ["quit"] to end it.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#if	defined(STRPP_MONITOR)

#include	<stddef.h>

/*
 * Size limits you can set with -D when you build, or in menuconfig under ESP-IDF.
 * The arrays a look uses live on the monitor's stack, so a larger count of
 * threads, queues or error buffers needs a larger stack: allow about 40 bytes
 * for each thread (STRPP_WATCH_THREADS, see watch.h), 60 for each queue and 40
 * for each error buffer, on top of 2 KB.
 */
#if	!defined(STRPP_MONITOR_STACK_BYTES)
#define	STRPP_MONITOR_STACK_BYTES	6144	// The monitor thread's stack
#endif
#if	!defined(STRPP_MONITOR_QUEUES)
#define	STRPP_MONITOR_QUEUES		16	// Queues one look can see
#endif
#if	!defined(STRPP_MONITOR_ERRBUFS)
#define	STRPP_MONITOR_ERRBUFS		16	// Error buffers one look can see
#endif

#include	<thread.h>
#include	<msgqueue.h>
#include	<window.h>
#include	<watch.h>

// What a platform can say about its memory
struct	MonitorMemory
{
	size_t		free_bytes;
	size_t		largest_block;
	size_t		lowest_free;	// Least free there has ever been
};

// Fill in the memory figures and return true, or return false if you cannot
typedef bool	(*MonitorMemoryProbe)(MonitorMemory& memory);

struct	MonitorSettings
{
	MonitorSettings()
	: interval(5000), stall(5000), confirm(250)
	, queue_warning((MSGQUEUE_HIGH_WATER*3)/4), error_warning(8)
	, probe(0), low_free(0), low_largest(0), reserve(0)
	{}

	Milliseconds		interval;	// Between looks
	Milliseconds		stall;		// A wait this long while holding a lock is a stall
	Milliseconds		confirm;	// Between the two looks that confirm a deadlock
	unsigned		queue_warning;	// Items in a queue that are worth reporting
	unsigned		error_warning;	// Errors in a buffer that are worth reporting
	MonitorMemoryProbe	probe;		// How to ask about memory, or none
	size_t			low_free;	// Report when less than this is free (0: never)
	size_t			low_largest;	// ...or the largest block is smaller than this (0: never)
	size_t			reserve;	// The largest block a look needs; 0 works it out from the sizes
};

// What a Window<Monitor> shows
struct	MonitorData
{
	MonitorData()
	: samples(0), skipped(0), dropped(0), threads(0), queues(0), error_buffers(0), short_of_memory(false)
	{}

	unsigned	samples;	// Looks so far
	unsigned	skipped;	// Looks it did not make, for want of memory
	unsigned	dropped;	// Reports it could not push
	unsigned	threads;	// Threads, queues and error buffers seen at the last look
	unsigned	queues;
	unsigned	error_buffers;
	bool		short_of_memory;	// The last look was skipped
	MonitorMemory	memory;		// From the probe, if it has one
	VariantArray	findings;	// The reports for everything found at the last look
};

class	Monitor
: public Thread
, public Windowed<Monitor, MonitorData>
{
public:
	// Starts the thread. `reports` must outlive it.
	Monitor(MessageQueue& reports, const MonitorSettings& settings = MonitorSettings());
	~Monitor();

	int		run();

	MessageQueue	requests;	// ["sample"] and ["quit"]

private:
	MessageQueue&		reports;
	MonitorSettings		settings;
	WatchRecord*		records;	// The latest look at the threads
	VariantArray		known;		// Keys of what was reported at the last look
	Variant			no_memory;	// The message to send when short of memory, made in advance
	bool			was_short;	// The last look was skipped
	bool			alloc_failed;	// A look's own allocation was refused

	void		sample();
	bool		confirmed(const unsigned* cycle, unsigned length);
	bool		enough_memory();
	void		skip_look();
	const char*	name_of(ThreadId id, unsigned count) const;
};

#endif	// STRPP_MONITOR

#endif	// MONITOR_H
