## Array with slices

`#include	<array.h>`

The `Array<T>` template creates a slice into an array of type T. New slices
(and copies) onto the same ArrayBody are inexpensive (using atomic
reference-counting), but any attempt to modify a slice first creates a copy
of the Body, leaving other slices unaffected. The ArrayBody itself is only
accessible as a constant, and a new Array may be created over a static body.

### Public methods

Defined in [array.h](https://github.com/cjheath/strpp/blob/main/include/array.h).

Making one:

- `Array()` - empty.
- `Array(const Element data)` - an array holding that one element.
- `Array(const Element* data, Index size, Index allocate = 0)` - copies `size`
  elements, with room for `allocate` more before it must grow.
- `Array(const Array&)`, `operator=` - share the body, so both are cheap.
- `Array(Body*)` - a reference to a body that already exists, for statics.

Reading:

- `length()`, `isEmpty()`, `isShared()` - the elements in this slice, whether
  it has any, and whether another array shares the body.
- `operator[](int)`, `elem(n)`, `last()` - copies of the elements.
- `last_ref()`, `elem_ref(n)` - const references, without copying.
- `asElements()` - a pointer to the first element of this slice.
- `operator->()`, `operator*()`, `operator const Body&()` - the body itself.
- `compare(const Array&)` and the comparison operators - element-wise.
- `find(e)`, `rfind(e)`, `find(f)`, `rfind(f)` - the index of an element, by
  value or by a match function, or -1 if it is not there.
- `detect(f)` - the index of the first element satisfying `f`, or -1.
- `bsearch(f)` - binary search of a sorted array by comparator.

Slicing, all O(1) and none of them copying:

- `slice(at, len = -1)`, `head(n)`, `tail(n)`, `shorter(n)`, `drop(n)`.

Changing, each taking a private copy first if the body is shared:

- `push(e)`, `append(e)`, `append(Array)`, `operator+=(e)`, `operator+=(Array)`,
  `operator<<(e)` - add at the end.
- `operator+` - concatenation, making a new array.
- `insert(pos, Array)` - insert another array at `pos`.
- `unshift(e)` - insert at the start; `shift()` removes from the start.
- `pull()`, `last_mut()` - take the last element, or reach it to write.
- `remove(at, len = -1)`, `delete_at(at)`, `delete_if(f)` - remove elements.
- `set(n, e)`, `elem_mut(n)` - write an element, and reach one to write.
- `reverse()` - reverse the elements of this slice.
- `each(f)` - call `f` for every element, leaving the array alone.
- `select(f)` - a new array of the elements that satisfy `f`.
- `map(f)` - a new array of what `f` answers for each element.
- `inject(start, f)` - fold the elements into an accumulator.
- `all(f)`, `any(f)`, `one(f)` - whether all, any, or exactly one element
  satisfies `f`.
- `evacuate()`, `clear()` - empty it, keeping the storage or giving it back.
- `free_if_emptied()` - release the body if this empty slice is its only
  owner.

`each`, `select`, `map`, `inject` and the `all`/`any`/`one` predicates take a
function and leave the array alone.

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
