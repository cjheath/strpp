## Threads, locks and thread-local storage

`#include	<thread.h>`, which brings in `lockfree.h` (the Latch),
`condition.h` (the Condition) and `thread_local.h` (ThreadSlot, ThreadLocal).

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

### Locks

A `Latch` is a lock, held by one thread at a time. `enter()` waits for it,
`probe()` takes it only if it is free, `holding()` asks whether this thread
has it, and `leave()` releases it. Under `NO_THREAD` there is one thread, so
all of these are no-ops and `holding()` is always true.

A `Condition` lets a thread wait until another signals it, with an optional
timeout. Under `NO_THREAD` nothing waits, so every method is a no-op.
