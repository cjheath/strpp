## MessageQueue: a thread's inbox

`#include <msgqueue.h>`, which brings in `lockfree.h` (the `Latch`),
`condition.h` (the `Condition`), `thread_local.h` (`ThreadLocal`) and
`variant.h` (`Variant`, `VariantArray`).

A `MessageQueue` is a thread-safe FIFO of `Variant`s: a `Latch`-protected
`VariantArray`, with a `Condition` signalling a waiter when something
arrives. It gives threads a way to pass messages without either one
touching the other's data directly - a sender pushes a copy of a
`Variant` or `VariantArray`, and the copy-on-write design in `variant.h`
makes that cheap even for a large nested structure.

There is no name-based registry to look a thread's queue up by. A sender
holds a pointer or reference to the *target* thread's `MessageQueue`, handed to
it when the threads were set up - the same way you would hand it any
other object one thread needs to reach into another's world.

### Each thread's own inbox

`MessageQueue::mine()` returns the calling thread's own `MessageQueue`, created on its
first call in that thread (via a `ThreadLocal<MessageQueue>`, see
[Threads, locks and thread-local storage](threading.md)). A thread reads
its own mail with `MessageQueue::mine()->pop()`, and other threads write to it
by holding the same pointer.

You don't have to use `mine()` at all: a `MessageQueue` is an ordinary object,
so you can also just construct one yourself and hand pointers to it
around explicitly, if a thread wants more than one inbox or the sender
already has the `MessageQueue` some other way.

### Public methods

- `push(const Variant&)`, `push(const VariantArray&)` - append one item,
  or several in order under a single lock, and wake a waiter. Pushing an
  empty `VariantArray` wakes nobody, since nothing arrived. If the queue
  is full, these wait for a pop to make room: see "A full queue" below.
- `push(item, Milliseconds timeout)` - the same, giving up after the timeout.
  It returns false when there was no room in time, and true when it pushed.
- `try_push(item)` - push if there is room now, and return whether it did.
- `pop()` - wait as long as it takes for an item, then return it.
- `pop(Milliseconds timeout)` - the same, but give up after `timeout` and
  return a null `Variant` (`Variant().is_null()` is true) if nothing
  arrived in time.
- `try_pop(Variant& item)` - take an item now if there is one, without
  waiting; returns whether it found one.
- `isEmpty()` - whether the queue currently holds nothing. Like any
  queue shared between threads, this is a snapshot: another thread can
  push or pop the instant after you read it.
- `static mine()` - the calling thread's own `MessageQueue`, created on first
  use.

### A full queue

A queue holds at most `MSGQUEUE_HIGH_WATER` items, default 16. A `push` to
a full queue waits until a `pop` makes room. A batch waits once, and
can then take the queue past the high water by the size of the batch.

A queue that stays full tells you something that its consumer is not
keeping up, or it is stuck.

A thread that pushes can wait for ever, so choose the form that suits it:

- A thread with nothing better to do can use `push`.
- A thread that must not wait, such as a user interface thread, uses
  `try_push`, and decides what to do when the queue is full.
- A thread that can wait a while uses `push` with a timeout.

Two threads can each wait for room in the other's full queue, and neither
ever pops, so give at least one of them a timeout.

The queue remembers which thread popped it last, so deadlock detection can
guess who is to blame.

### What it does not do

A `MessageQueue` does not know who else holds a pointer, so nothing stops
two threads reading the same one - `pop()` and `try_pop()` are safe to
call from more than one thread, but whichever call happens to win a race
takes the item, and the loser sees the queue as if it had never been
there. Arrange a single reader yourself if a queue needs one - the
`MessageQueue` doesn't enforce it.
