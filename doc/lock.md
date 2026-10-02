## Lock and SIXLock: shared, intent and exclusive locks

`#include <lock.h>`, which brings in `lockfree.h`, `condition.h`,
`thread.h` and `datetime.h`.

A `Latch` holds one thread at a time. A `Lock` does the same for longer: you
keep an `ExclLock` object in scope for as long as you want to exclude
everyone else. A `SIXLock` adds two lesser locks, so that many threads can
read while one thread decides whether to write.

### The locks

| you hold | others can hold | when it is granted |
|---|---|---|
| `SharedLock` | any number of `SharedLock`s, and one `IntentLock` | no one holds an `IntentLock` or `ExclLock` |
| `IntentLock` | `SharedLock`s that were granted before it | no one holds an `IntentLock` or `ExclLock` |
| `ExclLock` | nothing | no one holds any lock |

An `IntentLock` says "I will write soon". The `SharedLock`s already granted
carry on, but nobody new gets a lock until you release it, so readers cannot
keep you waiting for ever. You can `upgrade` an `IntentLock` to an `ExclLock`,
which waits for the `SharedLock`s to finish.

You hold a lock by keeping its object in scope. Destroying the object
releases it. You can move a lock object, but not copy it.

	SIXLock		lock;

	{
		SharedLock	reading(lock);	// Waits as long as it takes
		look_at(data);
	}					// Released here

	IntentLock	intent(lock);
	prepare();				// Readers continue, no new ones start
	ExclLock	writing(static_cast<IntentLock&&>(intent));
	change(data);				// No one else holds any lock

### Waiting, and not waiting

Each lock takes a `Milliseconds` timeout, and `Milliseconds(0)` polls. A
lock that was not granted is empty, and `holding()` tells you which you got:

	SharedLock	reading(lock, Milliseconds(50));
	if (!reading.holding())
		return;				// Busy; try again later

`ExclLock(intent, timeout)` upgrades an `IntentLock` and gives up after the
timeout. When it gives up, you still hold the `IntentLock`.

### Waiting for yourself

A thread that holds an `IntentLock` or `ExclLock` can never be granted another
lock on the same `SIXLock`, because it would wait for itself. If you ask for
one without a timeout, the program stops with the reason reported. With a
timeout you get an empty lock back after the wait.

### Public methods

`Lock`, in [lock.h](https://github.com/cjheath/strpp/blob/main/include/lock.h):

- `writing()` - whether any thread holds an `IntentLock` or `ExclLock`.
- `sharing()` - how many `SharedLock`s are held.

A `Lock` grants only an `ExclLock`. A `SIXLock` is a `Lock` that grants all
three. Destroying a lock that is still held stops the program.

`SharedLock(SIXLock&)`, `SharedLock(SIXLock&, Milliseconds)`,
`IntentLock(SIXLock&)`, `IntentLock(SIXLock&, Milliseconds)`,
`ExclLock(Lock&)`, `ExclLock(Lock&, Milliseconds)`:

- Take the lock, waiting as long as it takes, or up to the timeout.

`ExclLock(IntentLock&&)`, `ExclLock(IntentLock&, Milliseconds)`:

- Upgrade your `IntentLock`. The first leaves it empty, and the second leaves
  it unchanged if the timeout passes.

Every lock object has:

- `holding()` - whether you hold the lock.
- `release()` - release it now, instead of at the end of the scope.
