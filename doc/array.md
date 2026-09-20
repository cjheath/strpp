## Array with slices

`#include	<array.h>`

The Array<T> template creates a slice into an array of type T. New slices
(and copies) onto the same ArrayBody are inexpensive (using atomic
reference-counting), but any attempt to modify a slice first creates a copy
of the Body, leaving other slices unaffected. The ArrayBody itself is only
accessible as a constant, and a new Array may be created over a static body.

### The API

	Array<T>	a;			// Empty
	a.push(x), a.append(x), a += x;	// Add at the end
	a.remove(i);			// Delete from i to the end
	a.remove(i, n);			// Delete n elements from i
	a.insert(i, other);		// Insert another array at i
	a[i], a.last(), a.length(), a.isEmpty()
	a.slice(at, len), a.head(n), a.tail(n), a.shorter(n)
	a.find(x), a.each(f), a.select(f), a.map(f), a.all/any/one(f)
	a.evacuate(), a.clear()

`a[i]` and `a.last()` answer copies. `each`, `select`, `map` and the
`all`/`any`/`one` predicates take a function and leave the array alone.

### What a slice costs

A slice holds a reference to the Body, so while any slice is outstanding the
body is shared (`isShared()`), and the next mutation copies it. That is what
makes an array safe to pass around by value - and it is why an array that is
meant to be appended to repeatedly should not be handed out as a slice.
Every outstanding slice costs one copy on the next append, however the slice
is used.

### Shrinking a slice

`remove()`, `pull()`, `drop()`, `shift()` and `delete_at()` take sole
ownership before they release anything, so an element they drop is destroyed
there and then. Shrinking a slice from either end therefore moves the
survivors down: a copy when the body is shared, and a move of the live
elements when it is not. Draining an array one element at a time is
quadratic in its length; at the sizes these are used at that is nothing, but
a large array is better drained in one call.

### Emptying, and keeping the storage

`evacuate()` empties an array, releasing its elements but **keeping its
storage**, so that refilling it costs no allocation. `clear()` is the same
except that it also gives the storage back; an array that is emptied and
refilled repeatedly wants `evacuate()`.

Neither can release elements a shared body still shows to another array; the
elements stay alive until the last reference goes.

The StrVal class uses a specialisation of this template to provide its
storage and reference counting.
