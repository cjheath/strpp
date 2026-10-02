## Transactional message processing and read Windows

A thread may maintain a protected block of variables which publish some
state.  These variables are only available to view if you open a **Window**
on that thread.  An open Window stops the variables from being updated. The
owning thread may only update the variables under control of an `update`
block, which is only allowed to proceed when the data has no open Windows.
As long as update leaves the data in a valid state, no Window will ever see
invalid or half-updated state.

Because published data should be value-only (or using copy-on-write to
emulate value semantics), you can open a Window to snapshot the data and
close it quickly to work on the snapshot. Just be aware that as soon as the
Window is closed, you have no idea whether the data has been changed.
Multiple threads may open a Window on the same data, but as soon as the
owner wants to update it, any new Window requests will be delayed until
other windows have closed and the data has been updated.

Using this publication strategy, it's not usually necessary to include data in
messages. Just announce that new data is available and let whoever cares look
at it on their own time.

`#include <window.h>`, which brings in `thread.h` and `lock.h`.

### An example

Here is a thread which keeps a running total of some integers it receives in
a message. The struct `CounterData` defines the published data:

	struct CounterData
	{
		CounterData() : count(0) {}
		int		count;
	};

	class Counter
	: public Thread
	, public Windowed<Counter, CounterData>
	{
		int run()
		{
			for (;;)
			{
				Variant		message = MessageQueue::mine()->pop();
                                // Do any costly work prior to calling update():
				int		next = data().count + message.as_int();
				update(
                                        [&](Data& d) { d.count = next; }
                                );
			}
		}
	};

Another thread reads the count:

        {
                Window<Counter>		w(counter);	// Wait until it is stable
                show(w->count);
	}					// The Window closes at the end of the scope

The `Counter` thread gets a read-only view of its own state with `data()`,
which needs no Window, because only `Counter` writes it. It writes only
through `update()`, which calls your lambda with a `Data&` while no Window
is open. A write anywhere else does not compile.

### Use a Message to announce a change

Don't send the new state in a message. Update the state, then announce that
it changed, notifying each reader to fetch what it wants:

	update([&](Data& d) { d.last_scan = aps; d.scan_count++; });
	replies.push(Variant(VariantArray() << "scan" << (int)data().scan_count));

This announcement carries a generation number, `scan_count`. A reader that
misses an announcement, or sees several at once, opens a Window and reads the
latest. A reader that starts late does the same. The reader compares the
number with the last one it saw to tell whether anything changed.

Copy what you need out of the Window (and any other threads Windows!) and close
them before you do anything slow with it. A `VariantArray` or `StrVal` uses
shared data, so the copy costs little:

	VariantArray	aps;
	{
		Window<WifiScanner>	w(scanner, Milliseconds(1000));
		if (!w.holding())
			return;
		aps = w->last_scan;
	}
	print(aps);

Note that if you open multiple Windows, the order of acquisition of the read
locks can affect the success of the operation, or lead to deadlocks if you get
it wrong.

### The rules of a transaction

An `update()` is a transaction on the thread's state. These rules enforce it:

- **Leave the state consistent.** A reader can peek through an open Window
  between any two updates. Put everything that makes one consistent change in
  one `update()`.
- **Do the slow work first.** Build the new value before `update()`, and only
  assign it inside. Every reader must wait while you update. A scan that takes
  two seconds belongs outside, and the assignment that publishes its result
  can go inside.
- **Don't wait for anything inside `update()`.** Don't pop a queue, wait on a
  condition or open a Window there.
- **Don't nest updates.** An `update()` inside an `update()` waits for itself, and
  the program stops, reporting a deadlock.
- **Handle each message quickly.** A message that takes a long time holds up
  the messages behind it, whether or not any Window is open. If a job is
  long, make it a series of steps: do one, publish its progress with an
  `update()`, and send yourself a message to do the next. The thread can then
  answer other messages between steps.

### The rules of a reader

- **Close every Window before you wait for the thread.** If you open a Window,
  and then push a request and wait for the reply, the thread cannot update
  until you close the Window. It never replies, and you wait for ever.
- **Use a timeout if you can't afford to wait.** A user interface thread
  passes a `Milliseconds` and checks it worked with `holding()`, then shows
  what it saw, or tries again on the next frame.
- **Don't open a Window on your own thread during your update.** A thread that
  waits for itself is deadlocked. Outside `update()` the thread can open a
  Window on itself, but it has `data()` for that.
- **Close every Window before the thread is destroyed.**

### How the locks work

A Window holds a `SharedLock` on the thread, and `update()` takes an
`IntentLock` to block new SharedLocks, and then upgrades it to an `ExclLock`
when the last granted SharedLock is released. See [Lock and SIXLock](lock.md).
The result:

- Any number of Windows can be open to read the data at once.
- When the thread wants to update, already open Windows continue, but no new
  Window opens. The update starts when the last one closes.
- The thread then updates, and blocked Windows can open.

A reader can never keep the thread from updating by opening Windows one after
another, because new Windows wait once the thread is waiting.

### Memory

`update()` takes your lambda by value, as a template argument. It does not use
`std::function` and does not allocate. Capture with `[&]`, which costs one
pointer for each variable you name. The lambda may allocate memory, as it could
anywhere. Appending to a `VariantArray` allocates, but assigning one to another
does not.

### Public methods

`Windowed<Owner, Data>`, in
[window.h](https://github.com/cjheath/strpp/blob/main/include/window.h).
Derive your `Thread` from this, where `Data` is your type, using a default
constructor:

- `Data` - the state's type, which you can name in your thread's code.
- `data()` - the state, read-only. Only the owner calls this.
- `update(f)` - call `f(Data&)` with no Window open. Only the owner calls this.

`Window<Owner>`:

- `Window(Owner&)` - open a Window, waiting as long as it takes.
- `Window(Owner&, Milliseconds)` - the same, giving up after the timeout.
- `holding()` - true if your Window is open.
- `close()` - close the Window now, instead of at the end of the scope.
- `operator->()`, `operator*()` - the state, read-only. The program panics if
  your Window is not open.
