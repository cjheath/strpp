## The monitor: finding deadlocks, stalls and runaway buffers

`#include <monitor.h>`, which brings in `thread.h`, `msgqueue.h`, `window.h`
and `watch.h`. It exists only when you build with `STRPP_MONITOR`.

A thread that waits for ever, a queue that fills and an error buffer that
nobody empties all fail quietly: the device carries on until it runs out of
memory, or just stops. The monitor is a thread that looks for each of those
and tells you.

### Switching it on

Define `STRPP_MONITOR` when you build the library and everything that uses it.
In ESP-IDF, set `STRPP_MONITOR` in `menuconfig`, under strpp. Without it, no
code for the monitor is built and no lock does any extra work.

With it on, `Latch`, `Condition` and the locks in [lock.h](lock.md) tell the
library when a thread starts to wait, gets a lock or lets it go. Each thread
that takes a lock gets a small record, which holds up to `STRPP_WATCH_HELD`
locks (8 unless you set it). The library keeps one record for each of up to
`STRPP_WATCH_THREADS` threads (32), and a thread-local slot for finding them.
Everything you build together has to agree on the flag, as it has to agree on
the threading model.

### Settings you can build with

Each of these has a default, which you can change with `-D` when you build,
or in `menuconfig` under ESP-IDF. For `make`, pass them in `EXTRA_COPT`:

	make EXTRA_COPT='-DSTRPP_MONITOR -DSTRPP_WATCH_THREADS=64'

| define | default | what it sets |
|---|---|---|
| `STRPP_WATCH_THREADS` | 32 | threads one look can hold |
| `STRPP_WATCH_HELD` | 8 | locks one thread's record can list |
| `STRPP_MONITOR_STACK_BYTES` | 6144 | the monitor thread's stack |
| `STRPP_MONITOR_QUEUES` | 16 | queues one look can see |
| `STRPP_MONITOR_ERRBUFS` | 16 | error buffers one look can see |
| `MSGQUEUE_HIGH_WATER` | 16 | items a queue holds before a push waits |

The arrays for a look live on the monitor's stack, so when you raise the counts,
raise the stack: allow about 40 bytes for each thread, 60 for each queue and 40
for each error buffer, on top of 2 KB. Everything you build together must agree
on `STRPP_WATCH_THREADS` and `STRPP_WATCH_HELD`, because they change the size of
a record.

Give each thread a name so that reports say which one they mean:

	ThreadParams	params;
	params.name = "scanner";	// A literal, or anything that outlives the thread
	Thread(&params);

### Starting it

	MessageQueue	reports("reports");
	Monitor		monitor(reports);

`reports` is where findings go, so give it to a thread that reads them. You
can change when and what the monitor looks for:

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

### What it reports

Each finding becomes a message pushed to `reports` when it first appears:

- `["monitor", "deadlock", [name, ...]]` - these threads each wait for a lock,
  or for room in a queue, that the next one holds or pops. The monitor looks
  twice, a moment apart, and only reports a cycle that is still the same, so a
  wait that happened to overlap is not a deadlock.
- `["monitor", "stall", name, milliseconds, what, locks held]` - a thread has
  waited too long while it holds a lock. This catches the deadlocks that go
  through a condition or an empty queue, where nothing says who will signal
  it. A reader that holds a [Window](window.md) and waits for a reply from the
  thread it is reading is the usual one.
- `["monitor", "queue", name, depth, peak, pushers waiting]` - a queue is
  nearly full, or a thread is waiting to push to it. See [MessageQueue](msgqueue.md).
- `["monitor", "errors", name, errors, parameters]` - a thread has many errors
  in its buffer that nobody has dealt with. See [Errors](error.md).
- `["monitor", "memory", free, largest block, lowest free]` - the probe says
  memory is low.

It says each one once. When it is gone and comes back, it says it again.
The monitor never waits to push a report. If `reports` is full, it counts the
report as dropped and carries on.

What it found at its last look is also in its data. Open a Window to read it:

	Window<Monitor>	w(monitor, Milliseconds(100));
	if (w.holding())
		show(w->findings);		// The same reports, for what is found now

`samples` counts its looks, `dropped` counts the reports it lost, and `threads`,
`queues` and `error_buffers` say how much it saw.

### Controlling it

Push a request to `monitor.requests`:

- `["sample"]` - look now, instead of waiting for the interval.
- `["quit"]` - end the thread. You can then `join()` it.

### What it cannot see

It sees only what the library knows about: locks and conditions that go through
strpp, and threads that have taken a lock. A queue, a lock or a thread that
strpp did not make is invisible to it. A thread that strpp did not start keeps
its record for as long as the program runs.

A cycle that goes through a condition cannot be found by looking at the locks,
because nothing says which thread will signal the condition. The stall report
is how you find those.

### Public methods

`Monitor`, in [monitor.h](https://github.com/cjheath/strpp/blob/main/include/monitor.h):

- `Monitor(MessageQueue& reports, const MonitorSettings& settings)` - start it.
  `reports` must outlive it.
- `requests` - the queue to push `["sample"]` and `["quit"]` to.
- `data()` is private to the monitor; read what it found through a `Window<Monitor>`.

`WatchSnapshot`, `WatchFindCycle` and `WatchFindStalls`, in
[watch.h](https://github.com/cjheath/strpp/blob/main/include/watch.h), are what
the monitor uses. You can call them yourself to look for a deadlock when you
choose, such as when a watchdog fires. `MessageQueue::snapshot` and
`ErrBuf::snapshot` give you the figures for queues and error buffers.
