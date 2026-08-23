## COWMap

`#include	<cowmap.h>`

The Copy-on-write Map template is a sorted key/value map with by-value
semantics, like StrVal and Array: copying a CowMap is cheap (O(1), sharing
the same underlying tree with the original), and modifying it transparently
takes a private copy first if it's shared with any other reference.

Internally, CowMap is backed by a persistent (path-copying) red-black tree
(see redblack.h), not the STL's std::map. This is what makes copying O(1):
a copy just shares the tree's root; a subsequent mutation on either copy
only replaces the O(log n) nodes on the path to the change, leaving every
other node - and so every other reference still holding an older version -
completely untouched. Node colour is packed into the spare low bits of each
child pointer (see taggedref.h) at no extra memory cost. A node that is
uniquely owned is mutated in place; a shared node is copied first - safe
because a single reference is confined to one thread, and sharing a map
across threads always goes through copying it.

`operator[]` and `keys()`/`values()` return copies of the value/keys;
`find()` returns an iterator instead, giving read-only access (`->first`/
`->second`) without copying. `put()`/`insert()`/`remove()`/`clear()` mutate
the map; `select()`/`map()`/`inject()`/`each()`/`all()`/`any()`/`one()` are
read-only, functional-style traversal helpers.

Read the header file for the full API.

Tested in test/cowmap_test.cpp (the public CowMap API, including
copy-on-write behaviour) and test/redblack_test.cpp (the underlying tree:
structural invariants, differential testing against std::map, persistence/
sharing, and leak checking).

Example:

	#include	<cowmap.h>

	CowMap<int, StrVal>	ages;
	ages.insert("Alice", 30);
	ages.insert("Bob", 25);

	CowMap<int, StrVal>	snapshot(ages);	// O(1): shares the same tree
	ages.insert("Carol", 40);		// Only ages sees "Carol"; snapshot is unaffected
