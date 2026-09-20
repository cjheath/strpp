## Variant Data Type

`#include	<variant.h>`

The Variant class offers a type-safe way to manage atomic (numeric) and
composite types in a compact union, including `StrVal`, `Array<StrVal>`,
`Array<Variant>`, and a StrVal-keyed COWMap to Variant. You can build
data structures of any depth, and even convert to a single JSON StrVal
using as_json().

A Variant is a type tag and a union holding one of

	None,
    Integer,
    Long,
    LongLong,
    String (a StrRef),
	StrArray,
    VarArray,
    StrVarMap

String means a compact StrRef rather than a StrVal: it does not need the
bookmark that makes StrVal's scanning and indexing fast, and does not pay
for it.

This header also provides `VariantArray`, which is just `Array<Variant>`,
and `StrVariantMap`, which is a `CowMap<StrVal, Variant>` used by StrVarMap.
The three container types are reference-counted and copy-on-write, like
StrVal and Array, so all Variants are cheap to copy:

- `StrVal` is just the reference to a string body,
- `StrArray` and `VarArray` copies share the array body, so a copy is O(1),
- `StrVarMap` is backed by a CowMap (pure functional red-black tree),
  so copying is O(1) however many entries it holds. See [cowmap.md](cowmap.md).

A Variant is as large as its largest member plus the tag - 24 bytes on
64-bit machines, and less where pointers are smaller. A VariantArray is
therefore compact and fairly efficient to manipulate.

### VariantArray

`VariantArray` is `Array<Variant>`: a type-safe array of values of mixed
type, each carrying its own type.  Ask for the type before reading the
value, to avoid type coercions or assertion failures. A value read as the
wrong type is reported before it is asserted: the report names the type that
was wanted and the type that is held, and goes to the thread's error buffer
like any other message, so a program built with assertions left out still has
a record of what went wrong - and one a translation can carry.

Error message parameters are carried this way, for example.  That is what
makes a parameter list typesafe where printf-style arguments are not:
printf, handed the wrong argument, reinterprets its bytes.

### Public methods

Defined in [variant.h](https://github.com/cjheath/strpp/blob/main/include/variant.h).

Making one:

- `Variant()` - nothing at all, of type `None`.
- `Variant(int)`, `Variant(long)`, `Variant(long long)` - a number.
- `Variant(StrVal)`, `Variant(const char*)` - a string.
- `Variant(StringArray)`, `Variant(StrVal* v, StringArray::Index count)` - an
  array of strings.
- `Variant(VariantArray)`, `Variant(Variant* v, VariantArray::Index count)` -
  an array of Variants.
- `Variant(StrVal* keys, Variant* values, StringArray::Index count)`,
  `Variant(StrVariantMap)` - a string-keyed map to Variant.
- `Variant(VariantType t)` - an empty Variant of the given type.
- `Variant(const Variant&)`, `operator=` - a copy, discarding what the target
  held before.

Reading:

- `type()` - which kind of value is held.
- `is_null()` - true when it holds nothing.
- `type_name()` - the name of the held type, for messages about it.
- `as_int()`, `as_long()`, `as_longlong()` - the number held. The mutable
  forms coerce to that type first, and report and then assert when the value
  cannot be read as it.
- `as_strval()`, `as_string_array()`, `as_variant_array()`, `as_variant_map()`
  - the string or container held.
- `as_json(int indent = -1)` - the value as JSON. `-1` adds single spaces,
  `-2` is maximally compact, and `n >= 0` indents two spaces per level from
  `n`.

Building:

- `operator<<(const Variant& n)` - a new `VariantArray` of this Variant
  followed by `n`, so arrays can be built by chaining.
