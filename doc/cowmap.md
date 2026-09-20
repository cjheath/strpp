## COWMap

`#include	<cowmap.h>`

The Copy-on-write Map template is a sorted key/value map with by-value
semantics, like StrVal and Array: copying a CowMap is cheap (O(1), sharing
the same underlying tree with the original), and modifying it transparently
takes a private copy first if it's shared with any other reference.

Internally, CowMap is backed by a persistent (path-copying) red-black tree
(see [redblack.md](redblack.md)), not the STL's std::map. This is what
makes copying O(1): a copy just shares the tree's root; a subsequent
mutation on either copy only replaces the O(log n) nodes on the path to the
change, leaving every other node - and so every other reference still
holding an older version completely untouched. Node colour is packed into
the spare low bits of each child pointer (see taggedref.h) at no extra
memory cost. A node that is uniquely owned is mutated in place; a shared
node is copied first, which is safe because our rules require that a single
reference is confined to one thread; sharing a map across threads should
always copy the reference.

`operator[]` answers a copy of the value; `find()` returns an iterator
instead, giving read-only access (`->first`/`->second`) without copying.
`put()`/`insert()`/`remove()`/`clear()` mutate the map, unsharing first; the
rest are read-only, functional-style traversal helpers.

### Public methods

Defined in [cowmap.h](https://github.com/cjheath/strpp/blob/main/include/cowmap.h).

Making one:

- `CowMap()` - empty.
- `CowMap(const Key* keys, const Value* values, int size)` - from `size`
  parallel key and value arrays.
- `CowMap(const CowMap&)`, `operator=` - share the tree's root, which is O(1)
  however many entries the map holds.

Reading:

- `operator[](const Key&)` - the value stored for the key, or a
  default-constructed one when it is absent. It does not insert.
- `contains(const Key&)` - whether the key is in the map.
- `find(const Key&)` - an iterator at the entry, to compare with `end()`.
- `begin()`, `end()` - the entries, in key order.
- `size()` - how many entries there are.
- `values()` - an array of the values, in key order.
- `each(f)` - call `f` with every key and value.
- `select(f)` - a new map of the entries that satisfy `f`.
- `inject(start, f)` - fold the entries into an accumulator.
- `all(f)`, `any(f)`, `one(f)` - whether all, any, or exactly one entry
  satisfies `f`.

Changing, each unsharing first so that no other map sees the change:

- `insert(const Key, const Value)` - insert the pair.
- `remove(const Key&)` - erase the entry.
- `put(const Key&, Value)` - erase any entry for the key, then insert the new
  value, and answer the key.
- `clear()` - empty the map.

Example:

	#include	<cowmap.h>

	CowMap<int, StrVal>	ages;
	ages.insert("Alice", 30);
	ages.insert("Bob", 25);

	CowMap<int, StrVal>	snapshot(ages);	// O(1): shares the same tree
	ages.insert("Carol", 40);		// Only ages sees "Carol"; snapshot is unaffected
