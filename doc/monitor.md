## The monitor: finding deadlocks, stalls and runaway buffers

The Monitor is a background thread which watches for deadlocked threads,
queues that fill faster then they are emptied, or an error buffer filling
with reports that are not being delivered, or a device running out of
memory. The Monitor wakes periodically and has a `look` to see if there
seem to be problems.

### Switching it on

Build everything with `STRPP_MONITOR` to enable the Monitor thread and
the Watch points that it uses to check things. If you don't enable it,
nothing is added to your code and no extra work is undertaken.

In ESP-IDF, the `strpp` section of `menuconfig` provides `STRPP_MONITOR`.
On other platforms you can set it in the Makefiles.

The Watch point added to `Latch`, `Condition` and the Locks in [lock.h](lock.md)
record the time that a thread starts to wait, gets a lock or lets it go. Each
thread that takes a lock can record up to `STRPP_WATCH_HELD` locks (8 by default).
Up to `STRPP_WATCH_THREADS` threads (default 32) may be monitored, and a
thread-local slot is allocated.

### Settings

Each setting has a default which you can change with `-D` when you build,
or in `menuconfig` under ESP-IDF. For `make`, pass them in `EXTRA_COPT`:

	make EXTRA_COPT='-DSTRPP_MONITOR -DSTRPP_WATCH_THREADS=64'

| define | default | what it sets |
|---|---|---|
| `STRPP_WATCH_THREADS` | 32 | how many threads can be monitored |
| `STRPP_WATCH_HELD` | 8 | how many active locks for one thread |
| `STRPP_MONITOR_STACK_BYTES` | 6144 | stack size for the monitor thread |
| `STRPP_MONITOR_QUEUES` | 16 | How many queues can be monitored |
| `STRPP_MONITOR_ERRBUFS` | 16 | How many error buffers can be monitored |
| `MSGQUEUE_HIGH_WATER` | 16 | items a message queue holds before a push blocks |

Each time the Monitor looks, it builds arrays on its stack, not in allocated
memory. If you raise the counts, expand the stack limit, allowing about 40 bytes
for each thread, 60 for each queue and 40 for each error buffer, on top of 2KB base.
All parts of the program should be built with the same settings, or they will
disagree on the sizes of these records (`STRPP_WATCH_THREADS` and `STRPP_WATCH_HELD`)

It is preferred to give each thread a name for reporting:

	ThreadParams	params;
	params.name = "scanner";	// A literal, or anything that outlives the thread
	Thread(&params);

### Starting the monitor

	MessageQueue	reports("reports");
	Monitor		monitor(reports);

`reports` is where findings go, so give it to a thread that reads them. You
can change settings that control when and what the monitor looks for:

	MonitorSettings	settings;
	settings.interval = Milliseconds(2000);		// Between looks (5000)
	settings.stall = Milliseconds(10000);		// A stall is a wait this long (5000)
	settings.queue_warning = 12;			// Items in a queue worth reporting
	settings.error_warning = 8;			// Errors in a buffer worth reporting
	settings.probe = heap_probe;			// How to ask about memory
	settings.low_free = 20000;			// Report when less is free
	Monitor		monitor(reports, settings);

The monitor takes about 11 KB of heap for its records and a 6 KB stack. It
asks about memory through a function you give it, so that strpp does not need
to know your platform:

	static bool heap_probe(MonitorMemory& memory)
	{
		memory.free_bytes = ...;
		memory.largest_block = ...;
		memory.lowest_free = ...;
		return true;
	}

A look allocates memory, and a failed allocation stops the program, so the
monitor checks first. Before each look it asks the probe, and if the largest
free block is smaller than a look needs, it looks at nothing and says so instead
(see `no-memory` below). It works out what a look needs from the sizes you
built with, or you can set `settings.reserve` to your own number of bytes.
Without a probe, it has no way to ask, and looks anyway.

### What it reports

Each finding becomes a message pushed to a MessageQueue `reports` when it first appears:

- `["monitor", "deadlock", [name, ...]]` - the named threads each wait for a lock,
  or for room in a queue, that the next one holds or should pop. The monitor looks
  twice, a moment apart, and only reports a cycle if it hasn't changed, so a wait
  that happened to overlap is not a deadlock.
- `["monitor", "stall", name, milliseconds, what, locks held]` - a thread has
  waited too long while it holds a lock. This catches the deadlocks that go
  through a condition or an empty queue, where the monitor has no knowledge of
  who should signal it. A reader that holds a read [Window](window.md) and waiting
  for a reply from the owner thread is the most likely case.
- `["monitor", "queue", name, depth, peak, pushers waiting]` - a queue is nearly
  full, or a thread is waiting to push to it. See [MessageQueue](msgqueue.md).
- `["monitor", "errors", name, errors, parameters]` - a thread has many errors
  in its buffer that have not been reported or forwarded. See [Errors](error.md).
- `["monitor", "memory", free, largest block, lowest free]` - the probe says
  memory is low.
- `["monitor", "no-memory", "Not enough memory to report"]` - there was too little
  for a look, so it did not look. You get this once, until there is enough
  again. The monitor makes this message when it starts, so saying it allocates
  nothing itself. The queue you push it to might.

Monitor reports are never repeated. If a condition goes away and returns, that's a
new report. The monitor never waits to push a report. If `reports` is full, it counts
the report as dropped and carries on.

What it found at its last look is also published in its data. You can open a Window
to read it:

	Window<Monitor>	w(monitor, Milliseconds(100));
	if (w.holding())                // We opened a Window
		show(w->findings);	// The same reports, for what is found now

`samples` counts its looks, `skipped` counts the looks it did not make for want of
memory, `short_of_memory` says whether the last one was skipped, `dropped` counts
the reports it lost, and `threads`, `queues` and `error_buffers` say how much it saw.

### Controlling the Monitor

You can push a request message to `monitor.requests`:

- `["sample"]` - look now, instead of waiting for the interval.
- `["quit"]` - end the thread. You can then `join()` it.

### What the Monitor cannot see

The Monitor sees only what the library knows about. Locks and conditions that
go through strpp, and threads that have taken a lock. A queue, a lock or a
thread that strpp did not make is invisible. A thread that strpp did not start
keeps its record for as long as the program runs.

If a lock-dependency cycle goes through a condition, that cannot be found by
looking at the locks, because nothing says which thread will signal the condition.
This will show up as a stall report instead.

### Public methods

`Monitor`, in [monitor.h](https://github.com/cjheath/strpp/blob/main/include/monitor.h):

- `Monitor(MessageQueue& reports, const MonitorSettings& settings)` - start it.
  `reports` must outlive it.
- `requests` - the queue to push `["sample"]` and `["quit"]` to.
- `data()` is private to the monitor; read what it found through a `Window<Monitor>`.

The Monitor uses `WatchSnapshot`, `WatchFindCycle` and `WatchFindStalls` from
[watch.h](https://github.com/cjheath/strpp/blob/main/include/watch.h).
You can call them yourself to look for a deadlock when you choose, such as when a
watchdog fires. The figures for queues and error buffers are available from
`MessageQueue::snapshot` and `ErrBuf::snapshot` give you
