## Red-black trees: the RbTree class

`#include	<redblack.h>`

`RbTree<K, V>` is a map from a key to a value, one that keeps every older
version of the map usable. Changing the map makes a new tree that shares
almost all of its nodes with the old one: only the nodes along the path to
the change are new. So a map can be copied and edited freely - one version
per thread, one per parse, one kept as a snapshot - without the cost of
copying it, and without disturbing the versions already held. An old version
stays exactly as it was.

This is the tree that CowMap is built on, and it can be used directly where
the versions themselves are what you want to keep.

### The API

	using	Tree = RbTree<StrVal, int>;	// Any key and value types you have

	Tree::Link	root;			// The tree itself is just its root link

	Tree::insert(root, key, value);		// Add, or update the value of a key
	Tree::erase(root, key);			// Remove; nothing if the key is absent
	Tree::find(root, key);			// An Iter at the key, or end()
	Tree::begin(root), Tree::end();		// In-order iteration

	for (Tree::Iter it = Tree::begin(root); it != Tree::end(); ++it)
		... (*it).first, (*it).second ...

The iterator carries no parent pointer, because a shared node can have
several "parents" across versions and no single one is well defined. It
holds the ancestors reached by a left turn as an explicit path stack;
`find` builds the same stack by a key-guided walk, so `++` works the same
however the iterator was made. `(*it).first`/`(*it).second` and
`it->first`/`it->second` read as std::map's iterator does.

A key that is already present keeps its own key and takes the new value, and
erasing a key that is absent does nothing - map-style expectations, both.

### Public methods

Defined in [redblack.h](https://github.com/cjheath/strpp/blob/main/include/redblack.h).

- `RbTree<K,V>::insert(Link& root, const K& key, const V& val)` - adds the
  key, or updates the value if the key is already there.
- `RbTree<K,V>::erase(Link& root, const K& key)` - removes the key; nothing
  happens if it is absent.
- `RbTree<K,V>::find(Link root, const K& key)` - an Iter at the key, or
  `end()` if it is not in the tree.
- `RbTree<K,V>::begin(Link root)` - an Iter at the smallest key.
- `RbTree<K,V>::end()` - the past-the-end Iter, and what `find` answers when
  the key is missing.
- `RbTree<K,V>::Iter::locate(Link root, const K& key)` - an iterator built by
  a key-guided walk; what `find` returns.
- `RbTree<K,V>::Iter::current()` - the node the iterator is on, or null at
  the end.
- `RbTree<K,V>::Iter::operator++` - step to the next key, in order.
- `RbTree<K,V>::Iter::operator*`, `operator->` - the key and value at the
  iterator, as `first` and `second`.
- `RbNode<K,V>::key`, `.value` - the pair the node holds.
- `RbNode<K,V>::left`, `.right` - the child links, each carrying its colour
  in its tag.
- `RbNode<K,V>::live_count` - how many nodes are alive, for tests and
  diagnostics.

### Threading assumptions

A single reference is only ever visible to one thread at a time, and sharing
across threads always goes through copying it, so a reference count of 1
means that no one else, anywhere, can be observing that node. On that basis
a node is mutated in place while its count is 1, and cloned first when it is
not.

Tested in test/redblack_test.cpp (the structural invariants, differential
testing against std::map, snapshot persistence and sharing, and leak
counting by `RbNode::live_count`, which counts the nodes alive).

## Technical details

### Algorithm

This is a *persistent* (path-copying) left-leaning red-black tree.
Persistent means an edit yields a new version and leaves the old one intact;
path-copying means that only the O(log n) nodes on the path from the root to
the change are replaced, every other node being shared. Nodes are
reference-counted, so a node that is uniquely owned is mutated in place - an
unshared tree costs no more than an ordinary red-black tree, allocating only
the node being inserted - while a node shared with another version is copied
before it is modified.

The algorithm is left-leaning red-black (Sedgewick and Wayne, as used in
"Algorithms, 4th Edition"), which collapses red-black deletion's usual many
rebalancing cases into three primitives - rotateLeft, rotateRight and
flipColors - composed into moveRedLeft, moveRedRight and balance. The header
gives the original pseudocode of each above its translation.

### Colours belong to the links, not the nodes

A node's colour is a property of the edge that reaches it, packed into the
spare low-order bits of the child pointer (see taggedref.h) at no extra
space cost. It has to be the edge rather than the node: a shared node can be
reached by a red link from one version and a black link from another at the
same time, which happens as soon as a rotation copies a parent but reuses
its child.
