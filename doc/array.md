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
- `Array(const Element* data, Index size, Index allocate, ArrayOwnership ownership)`
  - as above, but `ownership` says how `data` was provided, so this need not
    copy it: `ArrayCopy` copies as the constructor above does; `ArrayBorrow`
    keeps `data` as the caller's, never copying or freeing it (the caller
    must keep it alive for as long as any Array or slice of it exists);
    `ArrayTakeOver` takes over a `new[]`'d buffer of the caller's, freeing it
    (`delete[]`) exactly as if this Array had allocated it itself. `allocate`
    is how many elements the caller's buffer actually holds, if more than
    `size` - the room this Array can grow into before it must reallocate.
    `ArrayOwnership` is defined here but not specific to `Array<T>`: [StrVal](strval.md)'s
    Body is one of these too, and takes the same enum for the same reason.
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

- `slice(at, len = -1)`, `head(n)`, `tail(n)`, `shorter(n)`, `drop(n)`. The
  length defaults to the rest of the array, and a length that runs past the end
  is clamped to what is there - see "Asking for what is not there" below.

Changing, each taking a private copy first if the body is shared:

- `push(e)`, `append(e)`, `append(Array)`, `operator+=(e)`, `operator+=(Array)`,
  `operator<<(e)` - add at the end.
- `operator+` - concatenation, making a new array.
- `insert(pos, Array)` - insert another array at `pos`.
- `unshift(e)` - insert at the start; `shift()` removes from the start.
- `pull()`, `last_mut()` - take the last element, or reach it to write.
- `remove(at, len = -1)`, `delete_at(at)`, `delete_if(f)` - remove elements.
  `delete_if(f)` removes every element for which `f` returns true, and leaves
  the rest in order.
- `set(n, e)`, `elem_mut(n)` - write an element, and reach one to write.
- `reverse()` - reverse the elements of this slice.
- `each(f)` - call `f` for every element, leaving the array alone.
- `select(f)` - a new array of the elements that satisfy `f`.
- `map(f)` - a new array of what `f` returns for each element.
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

### Asking for what is not there

There are two out-of-bounds cases, handled differently.

Asking for **more elements than there are** is a clamp, not an error: `head(n)`
and `tail(n)` return the whole array when `n` is past the end, `shorter(n)`
gets the empty array, and a `len` that runs past the end of a slice is
shortened to fit. None of those reports - this is the clamp that every slice in
the class does, and the same one `StrVal`'s `substr` does.

Asking for an **index the array has not got** is the caller's error, and is
reported as `STRERR_INDEX_OUT_OF_RANGE`, naming the index, the length of the
array and what was wanted. An empty slice is returned:

| The call | What it returns |
|---|---|
| `slice(at)`, `at` past the end | the empty slice |
| `remove(at, len)`, `at` past the end or `len` running off it | *this*, unchanged: nothing is removed |
| `drop(n)`, `n` past the end | the array, unchanged |
| `delete_at(at)`, `at` past the end | a default-constructed element, and nothing is removed |

`slice(length())` and `remove(length())` are not errors - the empty slice and
the empty removal at the end - and neither reports.

Refusing rather than clamping is deliberate for the removal calls: the caller
named elements they believed were there, and removing a different set from the
one they named would be worse than removing none. A caller who expects an index
to be out of range, and does not want the buffer filled by each one, takes a
checkpoint first - see [Errors](error.md).

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

A body never gives its storage back on a **shrink**: asking for less memory than
it holds is ignored, so an array that grows and shrinks keeps the room it grew
into. `clear()` on the last owner is the way to hand it back, and an empty slice
that is the only owner gives it up too.

The StrVal class uses a specialisation of this template to provide its
storage and reference counting.
