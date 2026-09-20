## Variant Data Type

`#include	<variant.h>`

The Variant class offers a type-safe way to manage numeric and StrVal types
in a compact union, along with Array<StrVal>, Array<Variant>, and a
StrValkeyed COWMap to Variant. This allows building arbitrary data
structures, which support asJSON() to render the whole structure as a
string.

A Variant is a type tag and a union holding one of

	None, Integer, Long, LongLong, String (a StrRef),
	StrArray, VarArray, StrVarMap

A String holds a StrRef rather than a StrVal: it does not need the bookmark
that makes StrVal's scanning and indexing fast, and does not pay for it.

A Variant is as large as its largest member plus the tag - 24 bytes where
pointers are 8 bytes, and less where they are smaller. A parameter list of
them is therefore compact, but it is not a fixed size: do not assume one.

The three container types are reference-counted and copy-on-write, like
StrVal and Array, so a Variant that holds one is cheap to copy - but *how*
cheap differs, and the difference matters when a map is large:

- `StrArray` and `VarArray` copies share the array body, so a copy is O(1)
  until either copy is written to.
- `StrVarMap` is backed by a pure functional (path-copying) red-black tree,
  so **copying one copies only the root** - a copy is O(1) however many
  entries it holds, and a later change replaces only the nodes on the path
  to it, leaving every other node shared. See [cowmap.md](cowmap.md).

### VariantArray

`VariantArray` is `Array<Variant>`: a type-safe array of values of mixed
type, each carrying its own type. It is what the error system carries
message parameters in - see [errbuf.md](errbuf.md).

A value is read back by asking for the type it should be, which coerces
where the coercion loses nothing (an integer to its digits, say) and
otherwise reports the mismatch. That is what makes a parameter list typesafe
where printf-style arguments are not: printf, handed the wrong argument,
reinterprets its bytes.
