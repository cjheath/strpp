## Windows: transactional message processing

`#include <window.h>`, which brings in `thread.h` and `lock.h`.

Threads talk to each other with messages, and a message makes a good request
or announcement. A message makes a poor way to ask "what is the state now?",
because the answer has gone stale by the time you read it, and the thread has
to do work to build it.

A thread can instead keep its state in a protected area that other threads
read directly. You open a **Window** onto the thread, read what you want, and
close it. While your Window is open, the thread changes none of that state.
The thread changes its state only inside `update()`, a short transaction that
no Window can see half-finished.

So you use both:

- **Messages** say what to do, and what has happened.
- **Windows** show what the state is.

### An example

A thread counts what it is sent. Its state is `CounterData`, which you define
before the thread:

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
				int		next = data().count + message.as_int();
				update([&](Data& d) { d.count = next; });
			}
		}
	};

Another thread reads the count:

	Window<Counter>		w(counter);	// Waits until it is not updating
	show(w->count);
						// The Window closes at the end of the scope

`Counter` reads its own state with `data()`, which needs no Window, because
only `Counter` writes it. It writes only through `update()`, which calls your
lambda with a `Data&` while no Window is open. A write anywhere else does not
compile.

### Announcing a change

Don't send the new state in a message. Update the state, then announce that
it changed, and let each reader fetch what it wants:

	update([&](Data& d) { d.last_scan = aps; d.scan_count++; });
	replies.push(Variant(VariantArray() << "scan" << (int)data().scan_count));

The announcement carries a generation number, `scan_count`. A reader that
misses an announcement, or sees several at once, opens a Window and reads the
latest. A reader that starts late does the same. The reader compares the
number with the last one it saw to tell whether anything changed.

Copy what you need out of the Window and close it before you do anything
slow with it. A `VariantArray` or `StrVal` copy shares its data until someone
changes it, so the copy costs little:

	VariantArray	aps;
	{
		Window<WifiScanner>	w(scanner, Milliseconds(1000));
		if (!w.holding())
			return;
		aps = w->last_scan;
	}
	print(aps);

### The rules of a transaction

An `update()` is a transaction on the thread's state. These rules keep it one:

- **Leave the state consistent.** A reader can open a Window between any two
  updates. Put everything that makes one consistent change in one `update()`:
  a list and its count, not the list in one and the count in the next.
- **Do the slow work first.** Build the new value before `update()`, and only
  assign it inside. Every reader waits while you update. A scan that takes two
  seconds belongs outside, and the assignment that publishes its result
  inside.
- **Don't wait for anything inside `update()`.** Don't pop a queue, wait on a
  condition or open a Window there.
- **Don't nest them.** An `update()` inside an `update()` waits for itself, and
  the program stops with the reason reported.
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
  passes a `Milliseconds` and checks `holding()`, then shows what it had or
  tries again on the next frame.
- **Don't open a Window on your own thread while you update.** That asks the
  thread to wait for itself. Outside `update()` the thread can open a Window on
  itself, but it has `data()` for that.
- **Close every Window before the thread is destroyed.**

### What the locks do

A Window holds a `SharedLock` on the thread, and `update()` takes an
`IntentLock` and then upgrades it to an `ExclLock`. See
[Lock and SIXLock](lock.md). The result:

- Any number of Windows can be open at once.
- When the thread wants to update, Windows already open continue, and no new
  Window opens. The update starts when the last one closes.
- The thread then updates, and Windows can open again.

A reader can never keep the thread from updating by opening Windows one after
another, because new Windows wait once the thread is waiting.

### Memory

`update()` takes your lambda by value, as a template argument. It does not use
`std::function` and does not allocate. Capture with `[&]`, which costs one
pointer for each variable you name. What your lambda does inside can allocate,
as it could anywhere. Appending to a `VariantArray` allocates, and assigning
one to another does not.

### Public methods

`Windowed<Owner, Data>`, in
[window.h](https://github.com/cjheath/strpp/blob/main/include/window.h). You
derive your `Thread` from it, and `Data` is a type you define, with a default
constructor:

- `Data` - the state's type, which you can name in your thread's code.
- `data()` - the state, read-only. Only the owner calls this.
- `update(f)` - call `f(Data&)` with no Window open. Only the owner calls this.

`Window<Owner>`:

- `Window(Owner&)` - open a Window, waiting as long as it takes.
- `Window(Owner&, Milliseconds)` - the same, giving up after the timeout.
- `holding()` - whether you have a Window.
- `close()` - close it now, instead of at the end of the scope.
- `operator->()`, `operator*()` - the state, read-only. They stop the program if
  you don't hold a Window.
