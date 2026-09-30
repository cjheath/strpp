## Threads, locks and thread-local storage

This is a cross-platform thread API, allowing multi-threaded code to be
written once for Posix Threads, Windows, or FreeRTOS systems, with efficient
synchronisation primitives and thread-local storage.

`#include	<thread.h>`, which brings in `lockfree.h` (the Latch),
`condition.h` (the Condition) and `thread_local.h` (ThreadSlot, ThreadLocal).

### Locks

A `Latch` is a lock, held by one thread at a time. `enter()` waits for it,
`probe()` takes it only if it is free, `holding()` asks whether this thread
has it, and `leave()` releases it. A `Latch` cannot be copied: the copy would
hold the same lock, and destroying either would destroy it under the other.
Under `NO_THREAD` there is one thread, so all of these are no-ops and
`holding()` is always true.

**A latch that cannot be taken stops the program**, with the reason reported.
There is no result `enter()` could return that the caller could act on - it
would walk into the critical section unprotected - and the usual cause is the
caller's own mistake, a recursive `enter()` that this (error-checking) lock
refuses by design. `leave()` checks in the same way that the calling thread is
the one holding it.

A `Condition` lets a thread wait until another signals it, with an optional
timeout. Under `NO_THREAD` nothing waits, so every method is a no-op.

### Public methods

`Thread` and `ThreadParams`, in
[thread.h](https://github.com/cjheath/strpp/blob/main/include/thread.h):

- `Thread(const ThreadParams* params = 0)` - a thread object, not yet started.
  `ThreadParams::stackBytes` asks for a stack size, 0 taking the platform's
  default.
- `run()` - virtual, and yours to override. The thread's body, whose return
  value is its exit code.
- `resume()`, `suspend()` - start the thread, and suspend or resume it. A host
  that will not create the thread says so, and the thread is then not running:
  its `id()` is 0, joining it is a mistake, and destroying it is harmless.
- `join()` - wait for the thread to end, and return its exit code: what `run()`
  returned, or what `exit()` was given. Not from the thread itself. 0 for a
  thread that never ran, or whose wait failed - the report is what tells those
  apart.
- `id()` - this thread's platform identifier, or 0 before it starts.
- `exit(int)` - end the current thread, with this exit code. The thread ends
  there: the rest of `run()` does not happen, and under pthreads the destructors
  of objects `run()` built on its stack do not run either, so anything that must
  be released is released first.
- `static yield(Milliseconds milliseconds = 0)` - give up the CPU, waiting for
  that long first where the platform can. A zero or negative time, or a null
  `Milliseconds`, is a plain yield.
- `static joinAny()` - wait until one of the registered threads has ended, and
  return it; 0 if there are none.
- `static currentId()`, `static current()` - the calling thread, by identifier
  or by pointer.
- `static currentProcessId()` - the process the calling thread is in, or 0
  where the platform has no such notion.
- `static main()` - the main thread.

`Latch`, in
[lockfree.h](https://github.com/cjheath/strpp/blob/main/include/lockfree.h):

- `enter()` - wait for the latch, then hold it.
- `probe()` - take the latch if it is free, without waiting.
- `holding()` - whether the calling thread holds it.
- `leave()` - release it.

`Condition`, in
[condition.h](https://github.com/cjheath/strpp/blob/main/include/condition.h):

- `wait(Latch*)` - sleep until signalled, releasing the latch while it waits
  and taking it again on waking. The latch is the one the caller holds: a wait
  without one has nothing to release, and the platform needs a lock to wait on.
- `wait(long& delay_ms, Latch*)` - the same, giving up after that many
  milliseconds. `delay_ms` is updated with the time that is left: 0 when the
  wait timed out, and the remainder when it was signalled early, so a caller
  waiting in a loop for something that may never happen waits only as long as
  it asked to. A delay of 0 or less does not wait at all.
- `signal()`, `broadcast()` - wake one thread that is waiting, or all of them.
- `ok()` - whether the underlying primitive was made. A condition variable that
  was not created has nothing to wait on and nothing to signal: the first wait,
  signal or broadcast to find that reports it and returns immediately.
- `waiters()`, where the model offers it - how many threads are waiting.

### When the host refuses

An operating system call can fail for reasons the program cannot control - no
more threads can be created, a lock cannot be taken. Each of those is reported
from the `THR` message set (`include/strpp_err.h`), and then the library carries
on with the safe nothing: a thread that was not created is not running and is
not registered; a wait that failed is over. A caller who expects one and does
not want the buffer filled by it takes a checkpoint first, as with any other
report - see [Errors](error.md).

What is *not* carried on with is the library's own bookkeeping: a reference
count that would wrap or go below zero, or a tag too wide for a pointer's
alignment, stops the program with the reason reported, in every build. Those
can only happen if something in the library is already wrong, and no result it
could return would be right.

`ThreadLocal<T>` and `ThreadSlot`, in
[thread_local.h](https://github.com/cjheath/strpp/blob/main/include/thread_local.h):

- `ThreadLocal<T>::get()` - this thread's object, made on first use.
- `ThreadLocal<T>::peek()` - this thread's object, if it already has one, and
  null otherwise.
- `ThreadLocal<T>::clear()` - destroy this thread's object, if it has one.
- `ThreadSlot::get()`, `ThreadSlot::set(void*)` - this thread's one `void*` in
  the slot.

### Choosing a threading model

Exactly one must be defined, or the build stops with a single error in
`thread.h`:

| define | |
|---|---|
| `HAVE_PTHREADS` | POSIX threads |
| `HAVE_FREERTOS` | FreeRTOS |
| `MSW` | Windows |
| `NO_THREAD` | No threading: one thread, and locks that never block |

The models satisfy the same declarations with *different implementations*,
so **whatever model the library was built with, a program using it must be
compiled with**. Two translation units built with different selections agree
on the declarations but not on the objects, and the mismatch then shows up
as unexplained behaviour at run time rather than as an error - which is why
selecting none, or more than one, is refused at compile time.

`make COPT=-DNO_THREAD lib` builds the library without threads. The object
files are shared between models, so a `make clean` is needed when switching.

### Thread-local storage

`ThreadSlot` holds one `void*` per thread. `ThreadLocal<T>` holds one object
of type T per thread, made on first use:

	static ThreadLocal<VariantArray>	scratch;

	VariantArray&	params = *scratch.get();	// This thread's, made if new

Slots are process-wide and few, because every backend offers only a small
pool of them - a pthread key, a TLS index, or a compile-time index into a
fixed array of pointers per task. Claim them early and rarely: one per
subsystem that needs one, not one per object.

There are deliberately **no thread-exit destructors**. A pthread key can run
one, but `TlsAlloc` and FreeRTOS task-local storage cannot, and behaviour
that differs by backend is worse than uniformly not having it. So whatever a
thread puts in a slot is released by the process, or by whoever owns the
thread, or by `ThreadLocal::clear()` - not by the slot.

This is not a compiler `thread_local`: that works on pthreads and Windows
but not on FreeRTOS, where it needs toolchain support, so it could not cover
every model this library builds for.

Under FreeRTOS, `configNUM_THREAD_LOCAL_STORAGE_POINTERS` must be raised
from its default of 0 to at least `THREAD_LOCAL_MAX_SLOTS`.

### Passing messages between threads

A Thread may receive messages from a message [`Queue`](queue.md), which
is created on its first call to `Queue::mine()`. The queue is made available
to other threads which may `push()` new items onto the queue.
Each Queue involves a `Latch` to protect the critical section, a `Condition`
to notify a waiter, and a VariantArray to contain message data.
