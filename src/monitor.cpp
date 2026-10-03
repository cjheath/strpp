/*
 * Monitor: a thread that watches the others. See include/monitor.h.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<monitor.h>

#if	defined(STRPP_MONITOR)

#include	<stdint.h>
#include	<new>

#include	<strval.h>
#include	<errbuf.h>

static const size_t	MONITOR_STACK_BYTES = STRPP_MONITOR_STACK_BYTES;
static const unsigned	MONITOR_QUEUES = STRPP_MONITOR_QUEUES;
static const unsigned	MONITOR_ERRBUFS = STRPP_MONITOR_ERRBUFS;

static const ThreadParams*
monitor_params()
{
	static ThreadParams	params;
	params.stackBytes = MONITOR_STACK_BYTES;
	params.name = "monitor";
	return &params;
}

Monitor::Monitor(MessageQueue& a_reports, const MonitorSettings& a_settings)
: Thread(monitor_params())
, requests("monitor")
, reports(a_reports)
, settings(a_settings)
, records(new WatchRecord[STRPP_WATCH_THREADS])
, no_memory(Variant(VariantArray() << "monitor" << "no-memory" << "Not enough memory to report"))
, was_short(false)
, alloc_failed(false)
{
	resume();
}

Monitor::~Monitor()
{
	delete[] records;
}

static const char*
kind_name(WatchKind kind)
{
	switch (kind)
	{
	case WatchLatch:	return "latch";
	case WatchLock:		return "lock";
	case WatchCondition:	return "condition";
	case WatchQueue:	return "queue";
	default:		return "nothing";
	}
}

const char*
Monitor::name_of(ThreadId id, unsigned count) const
{
	for (unsigned i = 0; i < count; i++)
		if (records[i].id == id && records[i].name)
			return records[i].name;
	return "(unnamed)";
}

/*
 * A cycle is real if looking again, a moment later, finds the same threads
 * still waiting for the same things, and since the same time: a wait that
 * ended and began again will have a later start.
 */
bool
Monitor::confirmed(const unsigned* cycle, unsigned length)
{
	struct	Wait { ThreadId id; const void* object; uint32_t since; };
	Wait		first[STRPP_WATCH_THREADS];
	for (unsigned i = 0; i < length; i++)
	{
		first[i].id = records[cycle[i]].id;
		first[i].object = records[cycle[i]].wait_object;
		first[i].since = records[cycle[i]].wait_since;
	}

	Thread::yield(settings.confirm);

	WatchRecord*	again = new (std::nothrow) WatchRecord[STRPP_WATCH_THREADS];
	if (!again)
	{
		alloc_failed = true;
		return false;
	}
	unsigned	count = WatchSnapshot(again, STRPP_WATCH_THREADS);
	bool		same = true;
	for (unsigned i = 0; i < length && same; i++)
	{
		bool	found = false;
		for (unsigned j = 0; j < count; j++)
			if (again[j].id == first[i].id)
				found = again[j].wait_object == first[i].object && again[j].wait_since == first[i].since;
		same = found;
	}
	delete[] again;
	return same;
}

/*
 * A look needs room for a second set of thread records, the findings, and the
 * strings that name them. Without a probe there is no way to know, so it goes on.
 */
bool
Monitor::enough_memory()
{
	if (!settings.probe)
		return true;
	MonitorMemory	memory;
	if (!settings.probe(memory))
		return true;
	size_t		needed = settings.reserve ? settings.reserve
				: STRPP_WATCH_THREADS * sizeof(WatchRecord) + 4096;
	return memory.largest_block >= needed;
}

// Say once that looking was not possible, in a way that allocates nothing
void
Monitor::skip_look()
{
	bool		first = !was_short;
	was_short = true;
	bool		pushed = !first || reports.try_push(no_memory);
	published.update([&](MonitorData& d)
	{
		d.skipped++;
		d.short_of_memory = true;
		if (!pushed)
			d.dropped++;
	});
}

void
Monitor::sample()
{
	if (!enough_memory())
	{
		skip_look();
		return;
	}
	was_short = false;
	alloc_failed = false;

	VariantArray	findings;		// What there is now
	VariantArray	keys;			// What identifies each, so it is reported once

	unsigned	threads = WatchSnapshot(records, STRPP_WATCH_THREADS);
	uint32_t	now = WatchNowMs();

	unsigned	cycle[STRPP_WATCH_THREADS];
	unsigned	length = WatchFindCycle(records, threads, cycle, STRPP_WATCH_THREADS);
	bool		in_cycle[STRPP_WATCH_THREADS];
	for (unsigned i = 0; i < STRPP_WATCH_THREADS; i++)
		in_cycle[i] = false;
	bool		real_cycle = length > 0 && confirmed(cycle, length);
	if (alloc_failed)
	{		// Not even the second set of records fitted
		skip_look();
		return;
	}
	if (real_cycle)
	{
		VariantArray	names;
		for (unsigned i = 0; i < length; i++)
		{
			names << name_of(records[cycle[i]].id, threads);
			in_cycle[cycle[i]] = true;
		}
		findings << Variant(VariantArray() << "monitor" << "deadlock" << Variant(names));
		keys << StrVal::format("deadlock:{1}:{2}",
			VariantArray() << (unsigned long)records[cycle[0]].wait_since << (int)length);
	}

	unsigned	stalled[STRPP_WATCH_THREADS];
	unsigned	stalls = WatchFindStalls(records, threads, now, (uint32_t)settings.stall.ms(), true, stalled, STRPP_WATCH_THREADS);
	for (unsigned i = 0; i < stalls; i++)
	{
		const WatchRecord&	r = records[stalled[i]];
		if (in_cycle[stalled[i]])
			continue;		// Already reported as part of a deadlock
		findings << Variant(VariantArray() << "monitor" << "stall"
			<< (r.name ? r.name : "(unnamed)") << (long)(uint32_t)(now - r.wait_since)
			<< kind_name(r.wait_kind) << (int)(r.held_count + r.held_dropped));
		keys << StrVal::format("stall:{1}:{2}",
			VariantArray() << (r.name ? r.name : "(unnamed)") << (unsigned long)r.wait_since);
	}

	MessageQueueRecord	queues[MONITOR_QUEUES];
	unsigned	queue_count = MessageQueue::snapshot(queues, MONITOR_QUEUES);
	for (unsigned i = 0; i < queue_count; i++)
	{
		const MessageQueueRecord&	q = queues[i];
		if (q.depth < settings.queue_warning && q.blocked == 0)
			continue;
		findings << Variant(VariantArray() << "monitor" << "queue"
			<< (q.name ? q.name : "(unnamed)") << (int)q.depth << (int)q.peak << (int)q.blocked);
		keys << StrVal::format("queue:{1}", VariantArray() << (unsigned long)(uintptr_t)q.queue);
	}

	ErrBufRecord	buffers[MONITOR_ERRBUFS];
	unsigned	buffer_count = ErrBuf::snapshot(buffers, MONITOR_ERRBUFS);
	for (unsigned i = 0; i < buffer_count; i++)
	{
		const ErrBufRecord&	b = buffers[i];
		if (b.live < settings.error_warning)
			continue;
		findings << Variant(VariantArray() << "monitor" << "errors"
			<< name_of(b.owner, threads) << (int)b.live << (int)b.parameters);
		keys << StrVal::format("errors:{1}", VariantArray() << (unsigned long)(uintptr_t)b.buffer);
	}

	MonitorMemory	memory;
	bool		have_memory = false;
	if (settings.probe)
	{
		have_memory = settings.probe(memory);
		if (have_memory
		 && ((settings.low_free && memory.free_bytes < settings.low_free)
		  || (settings.low_largest && memory.largest_block < settings.low_largest)))
		{
			findings << Variant(VariantArray() << "monitor" << "memory"
				<< (long)memory.free_bytes << (long)memory.largest_block << (long)memory.lowest_free);
			keys << "memory";
		}
	}

	// Report what was not found last time
	unsigned	dropped = 0;
	for (VariantArray::Index i = 0; i < keys.length(); i++)
	{
		bool	old = false;
		for (VariantArray::Index j = 0; j < known.length() && !old; j++)
			old = known[j].as_strval() == keys[i].as_strval();
		if (!old && !reports.try_push(findings[i]))
			dropped++;
	}
	known = keys;

	published.update([&](MonitorData& d)
	{
		d.samples++;
		d.short_of_memory = false;
		d.dropped += dropped;
		d.threads = threads;
		d.queues = queue_count;
		d.error_buffers = buffer_count;
		if (have_memory)
			d.memory = memory;
		d.findings = findings;
	});
}

int
Monitor::run()
{
	for (;;)
	{
		Variant		request = requests.pop(settings.interval);
		if (request.is_null())
		{
			sample();		// The interval passed with no request
			continue;
		}
		if (request.type() != Variant::VarArray || request.as_variant_array().length() == 0
		 || request.as_variant_array()[0].type() != Variant::String)
			continue;
		StrVal		command = request.as_variant_array()[0].as_strval();
		if (command == "sample")
			sample();
		else if (command == "quit")
			return 0;
	}
}

#endif
