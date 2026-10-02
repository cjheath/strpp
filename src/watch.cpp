/*
 * The night Watch. Record locks held by each thread and what it is waiting for.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<thread.h>
#include	<watch.h>

// Does a thread waiting like this need what a thread holding like this has?
static bool
holds_up(WatchMode wait, unsigned char hold)
{
	switch (wait)
	{
	case WatchForShared:
	case WatchForWriter:	return hold == HoldWriter;
	case WatchForDrain:	return hold == HoldShared;
	default:		return false;
	}
}

static bool
waits_for(const WatchRecord& waiter, const WatchRecord& holder)
{
	if (waiter.wait_kind == WatchQueue)
		return waiter.wait_has_consumer && waiter.wait_consumer == holder.id;
	if (waiter.wait_kind != WatchLatch && waiter.wait_kind != WatchLock)
		return false;
	for (unsigned i = 0; i < holder.held_count; i++)
		if (holder.held[i].object == waiter.wait_object
		 && holds_up(waiter.wait_mode, holder.held[i].mode))
			return true;
	return false;
}

namespace
{
	struct	CycleSearch
	{
		const WatchRecord*	records;
		unsigned		count;
		unsigned char		state[STRPP_WATCH_THREADS];	// 0 new, 1 on the path, 2 done
		unsigned		path[STRPP_WATCH_THREADS];
		unsigned		depth;
		unsigned		start;			// Where in path the cycle begins

		bool	visit(unsigned u)
		{
			state[u] = 1;
			path[depth++] = u;
			for (unsigned v = 0; v < count; v++)
			{
				if (v == u || !waits_for(records[u], records[v]))
					continue;
				if (state[v] == 1)
				{
					for (start = 0; path[start] != v; start++)
						;
					return true;
				}
				if (state[v] == 0 && visit(v))
					return true;
			}
			state[u] = 2;
			depth--;
			return false;
		}
	};
}

unsigned
WatchFindCycle(const WatchRecord* records, unsigned count, unsigned* out, unsigned max_out)
{
	if (count > STRPP_WATCH_THREADS)
		count = STRPP_WATCH_THREADS;
	CycleSearch	search;
	search.records = records;
	search.count = count;
	search.depth = 0;
	search.start = 0;
	for (unsigned i = 0; i < count; i++)
		search.state[i] = 0;

	for (unsigned i = 0; i < count; i++)
		if (search.state[i] == 0 && search.visit(i))
		{
			unsigned	length = search.depth - search.start;
			for (unsigned j = 0; j < length && j < max_out; j++)
				out[j] = search.path[search.start+j];
			return length < max_out ? length : max_out;
		}
	return 0;
}

unsigned
WatchFindStalls(
	const WatchRecord* records, unsigned count,
	uint32_t now_ms, uint32_t threshold_ms, bool only_holding,
	unsigned* out, unsigned max_out
)
{
	unsigned	found = 0;
	for (unsigned i = 0; i < count && found < max_out; i++)
		if (records[i].wait_kind != WatchNothing
		 && (uint32_t)(now_ms - records[i].wait_since) >= threshold_ms
		 && (!only_holding || records[i].held_count > 0 || records[i].held_dropped > 0))
			out[found++] = i;
	return found;
}

#if	defined(STRPP_MONITOR)

#include	<atomic>
#include	<thread_local.h>

#if	defined(HAVE_PTHREADS)
#include	<time.h>
#endif

uint32_t
WatchNowMs()
{
#if	defined(HAVE_PTHREADS)
	struct timespec	ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint32_t)(ts.tv_sec*1000 + ts.tv_nsec/1000000);
#elif	defined(HAVE_FREERTOS)
	return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
	return 0;
#endif
}

/*
 * One thread's record. Its own thread is the only writer, and a monitor reads
 * it as it stands, so every field a monitor reads is atomic.
 */
struct	ThreadWatch
{
	ThreadWatch()
	: next(0), active(true), id(), name(0)
	, held_count(0), held_dropped(0)
	, wait_object(0), wait_kind(WatchNothing), wait_mode(WatchForSignal), wait_since(0)
	, wait_has_consumer(false), wait_consumer()
	, depth(0)
	{
		for (unsigned i = 0; i < STRPP_WATCH_HELD; i++)
		{
			held_object[i] = 0;
			held_mode[i] = 0;
		}
	}

	ThreadWatch*			next;		// The registry's list, only ever added to
	std::atomic<bool>		active;		// Its thread is alive
	std::atomic<ThreadId>		id;
	std::atomic<const char*>	name;
	std::atomic<const void*>	held_object[STRPP_WATCH_HELD];
	std::atomic<unsigned char>	held_mode[STRPP_WATCH_HELD];
	std::atomic<unsigned>		held_count;
	std::atomic<unsigned>		held_dropped;
	std::atomic<const void*>	wait_object;
	std::atomic<int>		wait_kind;
	std::atomic<int>		wait_mode;
	std::atomic<uint32_t>		wait_since;
	std::atomic<bool>		wait_has_consumer;
	std::atomic<ThreadId>		wait_consumer;
	int				depth;		// Nested waits; only its own thread uses this
};

static std::atomic<ThreadWatch*>	registry;

static ThreadSlot&
slot()
{
	static ThreadSlot	the_slot;
	return the_slot;
}

// A record that is not in use, or a new one, made active for the caller
static ThreadWatch*
claim_record()
{
	for (ThreadWatch* w = registry.load(); w; w = w->next)
	{
		bool	expected = false;
		if (!w->active.load() && w->active.compare_exchange_strong(expected, true))
			return w;
	}
	ThreadWatch*	w = new ThreadWatch;
	w->next = registry.load();
	while (!registry.compare_exchange_weak(w->next, w))
		;
	return w;
}

// The calling thread's record, made on first use if asked; null before FreeRTOS has a scheduler
static ThreadWatch*
mine(bool make = true)
{
#if	defined(HAVE_FREERTOS)
	if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
		return 0;
#endif
	ThreadWatch*		w = (ThreadWatch*)slot().get();
	if (!w && make)
	{
		w = claim_record();
		w->id = Thread::currentId();
		slot().set(w);
	}
	return w;
}

void
watch_thread_start(const char* name)
{
	ThreadWatch*	w = mine();
	if (w)
		w->name = name;
}

void
watch_thread_exit()
{
	ThreadWatch*	w = mine(false);
	if (!w)
		return;
	slot().set(0);
	w->held_count = 0;
	w->held_dropped = 0;
	w->wait_kind = WatchNothing;
	w->wait_object = 0;
	w->depth = 0;
	w->name = 0;
	w->active = false;
}

void
watch_held_add(const void* object, WatchHold mode)
{
	ThreadWatch*	w = mine();
	if (!w)
		return;
	unsigned	n = w->held_count;
	if (n >= STRPP_WATCH_HELD)
	{
		w->held_dropped++;
		return;
	}
	w->held_object[n] = object;
	w->held_mode[n] = (unsigned char)mode;
	w->held_count = n+1;		// Listed only now that it is filled in
}

void
watch_held_remove(const void* object)
{
	ThreadWatch*	w = mine(false);
	if (!w)
		return;
	unsigned	n = w->held_count;
	for (unsigned i = n; i-- > 0; )
		if (w->held_object[i] == object)
		{
			w->held_object[i] = w->held_object[n-1].load();
			w->held_mode[i] = w->held_mode[n-1].load();
			w->held_count = n-1;
			return;
		}
	if (w->held_dropped > 0)	// It was one that did not fit
		w->held_dropped--;
}

WatchWait::WatchWait(const void* object, WatchKind kind, WatchMode mode, const ThreadId* consumer)
: record(mine())
, outermost(record && record->depth++ == 0)
{
	if (!outermost)
		return;
	record->wait_object = object;
	record->wait_mode = mode;
	record->wait_since = WatchNowMs();
	record->wait_has_consumer = consumer != 0;
	if (consumer)
		record->wait_consumer = *consumer;
	record->wait_kind = kind;	// Last: this says there is a wait
}

WatchWait::~WatchWait()
{
	if (!record)
		return;
	record->depth--;
	if (outermost)
		record->wait_kind = WatchNothing;
}

void
WatchWait::retarget(WatchMode mode)
{
	if (outermost)
		record->wait_mode = mode;
}

unsigned
WatchSnapshot(WatchRecord* out, unsigned max_out)
{
	unsigned	n = 0;
	for (ThreadWatch* w = registry.load(); w && n < max_out; w = w->next)
	{
		if (!w->active)
			continue;
		WatchRecord&	r = out[n];
		r.id = w->id;
		r.name = w->name;
		r.held_count = w->held_count;
		if (r.held_count > STRPP_WATCH_HELD)
			r.held_count = STRPP_WATCH_HELD;
		r.held_dropped = w->held_dropped;
		for (unsigned i = 0; i < r.held_count; i++)
		{
			r.held[i].object = w->held_object[i];
			r.held[i].mode = w->held_mode[i];
		}
		r.wait_kind = (WatchKind)w->wait_kind.load();
		r.wait_mode = (WatchMode)w->wait_mode.load();
		r.wait_object = w->wait_object;
		r.wait_since = w->wait_since;
		r.wait_has_consumer = w->wait_has_consumer;
		r.wait_consumer = w->wait_consumer;
		n++;
	}
	return n;
}

#endif
