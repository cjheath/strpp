## Transactional values and Windows

A `Transactional<T>` holds a value of type `T` and the lock that protects it.
It belongs to no thread. You reach the value only through a **Window**, which
is a lock you hold:

- A `ReadWindow` shows you the value, read-only. Any number of ReadWindows can
  be open at once, and the data is guaranteed not to change while they're open.
- An `UpdateWindow` lets you change the value, and no other Window is open while
  you do.

As long as each update leaves the value valid, no reader ever sees a
half-updated state.

`#include <transactional.h>`, which brings in `lock.h`.

### An example

	struct Count
	{
		Count() : count(0) {}
		int		count;
	};

	Transactional<Count>	counter;

Any thread can change it:

	{
		UpdateWindow<Count>	w(counter);	// Wait for the readers
		w->count++;
	}						// The Window closes here

and any thread can read it:

	{
		ReadWindow<Count>		w(counter);	// Wait for any update
		show(w->count);
	}						// The Window closes here

`counter.update(f)` opens an UpdateWindow, calls `f(Count&)`, and
closes it. Use it when the change is one lambda.

### Give a thread some published state

A thread that publishes its state holds a `Transactional` as a member, and
the readers open ReadWindows on that member:

	class Counter : public Thread
	{
	public:
		Transactional<Count>	published;

		int run()
		{
			for (;;)
			{
				Variant	message = MessageQueue::mine()->pop();
				// Do any costly work prior to update():
				int	next = published.unguarded().count + message.as_int();
				published.update([&](Count& d) { d.count = next; });
			}
		}
	};

	ReadWindow<Count>	w(counter.published);

`unguarded()` reads the value with no Window. That's safe only if you are the
sole writer, as `Counter` is here, or if nobody writes at all. If several
threads write, they must each open a Window to read, too.

You can also share one `Transactional` between threads that neither of them
owns, or put one in a `Thread`'s base class, in an array or a global.

### Use a Message to announce a change

An updater doesn't need to send the new state in a message. Update the
state, then announce that it changed, so each reader can fetch what it
wants:

	published.update([&](ScanData& d) { d.last_scan = aps; d.scan_count++; });
	replies.push(Variant(VariantArray() << "scan" << (int)published.unguarded().scan_count));

In this example, announcement carries a generation number, `scan_count`. A
reader that misses an announcement, or sees several at once, opens a
ReadWindow and reads the latest. A reader that starts late does the same.
The reader compares the number with the last one it saw to tell whether
anything changed.

Copy what you need out of the Window and close it before you do anything slow
with it. A `VariantArray` or `StrVal` uses shared data, so the copy costs
little:

	VariantArray	aps;
	{
		ReadWindow<ScanData>	w(scanner.published, Milliseconds(1000));
		if (!w.holding())
			return;
		aps = w->last_scan;
	}
	print(aps);

If you open several ReadWindows, take them in the same order everywhere, or
you can deadlock.

### The rules of an update

An UpdateWindow is a transaction on the value. These rules enforce it:

- **Leave the value consistent.** A reader can open a Window between any two
  updates. Put everything that makes one consistent change in one
  UpdateWindow.
- **Do the slow work first.** Build the new value before you open the
  UpdateWindow, and only assign it inside. Every reader must wait while you
  update.
- **Don't wait for anything inside it.** Don't pop a queue, wait on a
  condition or open another Window there.
- **Don't nest updates.** An update inside an update on the same value waits
  for itself, and the program stops, reporting a deadlock.
- **Handle each message quickly.** If a thread has published state and a
  message takes a long time, it holds up the messages behind it. If a job is
  long, make it a series of steps: do one, publish its progress, and send
  yourself a message to do the next.

### The rules of a reader

- **Close every Window before you wait for a thread that updates.** If you
  open a ReadWindow, and then push a request and wait for the reply, the
  thread cannot update until you close it. It never replies, and you wait for
  ever.
- **Use a timeout if you can't afford to wait.** A user interface thread
  passes a `Milliseconds` and checks it worked with `holding()`, then shows
  what it saw, or tries again on the next frame.
- **Close every Window before the `Transactional` is destroyed.**

### How the locks work

A ReadWindow holds a `SharedLock`. An UpdateWindow holds an `ExclLock`, which
first takes the intent to write, so that no new SharedLock is granted, and
then waits for the last granted SharedLock to be released. See
[Lock and SIXLock](lock.md). The result:

- Any number of ReadWindows can be open at once.
- When a thread wants to update, already open ReadWindows continue, but no
  new Window opens. The update starts when the last one closes.
- Once the update closes, blocked Windows can open.

A reader can never keep a writer out by opening Windows one after another,
because new Windows wait once a writer is waiting.

### Memory

`update()` takes your lambda by value, as a template argument. It does not use
`std::function` and does not allocate. Capture with `[&]`, which costs one
pointer for each variable you name. The lambda may allocate memory, as it could
anywhere. Appending to a `VariantArray` allocates, but assigning one to another
does not.

### Public methods

`Transactional<T>`, in
[transactional.h](https://github.com/cjheath/strpp/blob/main/include/transactional.h):

- `Transactional()` - default-construct the value.
- `Transactional(const T&)` - start with a copy of this value.
- `Data` - the value's type.
- `update(f)` - open an UpdateWindow, call `f(T&)`, and close it.
- `unguarded()` - the value, read-only, with no Window. See above for when
  this is safe.

`ReadWindow<T>`:

- `ReadWindow(Transactional<T>&)` - open a Window, waiting as long as it
  takes.
- `ReadWindow(Transactional<T>&, Milliseconds)` - the same, giving up after
  the timeout.
- `holding()` - true if your Window is open.
- `close()` - close the Window now, instead of at the end of the scope.
- `operator->()`, `operator*()` - the value, read-only. The program panics if
  your Window is not open.

`UpdateWindow<T>` has the same constructors, `holding()` and `close()`, and
its `operator->()` and `operator*()` return the value for writing.
