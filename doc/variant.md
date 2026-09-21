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
    UInteger,
    ULong,
    ULongLong,
    String (a StrRef),
	StrArray,
    VarArray,
    StrVarMap

String means a compact StrRef rather than a StrVal: it does not need the
bookmark that makes StrVal's scanning and indexing fast, and does not pay
for it.

The three unsigned types are worth having because a Variant must be able to
hold what the program has: a count, a hash, an address, a bit pattern from a
register. Read as a signed type, half of those values are negative, and one
that must not be read any other way has nowhere to go. They cost nothing to
carry - each is stored in the same union word as the signed type of its width,
so the three of them add no size at all to a Variant, which is the same 24
bytes it was.

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
- `Variant(int)`, `Variant(long)`, `Variant(long long)` - a signed number.
- `Variant(unsigned)`, `Variant(unsigned long)`, `Variant(unsigned long long)`
  - an unsigned one, held as `UInteger`, `ULong` or `ULongLong`. An integer
  literal that is too large for the signed type beside it picks one of these
  on its own, and so does any value of an unsigned type.
- `Variant(StrVal)`, `Variant(const char*)` - a string.
- `Variant(StringArray)`, `Variant(StrVal* v, StringArray::Index count)` - an
  array of strings.
- `Variant(VariantArray)`, `Variant(Variant* v, VariantArray::Index count)` -
  an array of Variants.
- `Variant(StrVal* keys, Variant* values, StringArray::Index count)`,
  `Variant(StrVariantMap)` - a string-keyed map to Variant.
- `Variant(VariantType t)` - an empty Variant of the given type. A Variant of
  that type already made - `Variant v(0)` for an `Integer`, `Variant
  m(Variant::StrVarMap)` for a map - is usually the better way, because no
  coercion comes *from* `None`: `Variant v; v.as_int() = 5;` reports a
  mismatched type rather than becoming an `Integer`.
- `Variant(const Variant&)`, `operator=` - a copy, discarding what the target
  held before.

Reading:

- `type()` - which kind of value is held.
- `is_null()` - true when it holds nothing.
- `type_name()` - the name of the held type, for messages about it.
- `as_int()`, `as_long()`, `as_longlong()` - the number held. The mutable
  forms coerce to that type first, and report and then assert when the value
  cannot be read as it.
- `as_uint()`, `as_ulong()`, `as_ulonglong()` - the same three, unsigned, for a
  Variant whose type is one of the unsigned ones. Unlike the three above they
  answer a value rather than a reference to the union word, which is signed: a
  reference to it read as unsigned would be an aliasing violation. There is
  therefore no mutable form of these three - nothing coerces *to* an unsigned
  type - so a Variant that holds a number signed must be asked for it that way.
- `as_signed()` - the number held as a signed one, in the closest signed type
  that holds it, answering a `long long`. This is the read for a number of
  unknown origin, and the answer to an unsigned value too large for its own
  signed twin: a `UInteger` beyond `INT_MAX` becomes a `Long`, and one beyond
  `LONG_MAX` a `LongLong`. A value already signed is answered as it stands and
  not narrowed, since its width is what its holder chose; a string that reads
  as a number is converted; and an unsigned value no signed type can hold - a
  `ULongLong` beyond `LLONG_MAX`, and on a 64-bit target a `ULong` beyond
  `LONG_MAX` too - is refused like any other lossy coercion. It answers a value
  rather than a reference, the width of a reference being whatever the value
  turned out to need, so it is a read and not a way to write in.
- `as_strval()`, `as_string_array()`, `as_variant_array()`, `as_variant_map()`
  - the string or container held. The mutable `as_strval()` coerces a number
  to a string, which is what renders an unsigned value unsigned. All four
  answer a handle to a body the Variant still shares, so writing through one -
  `v.as_strval() += "x"`, `v.as_variant_map().insert(k, v)` - copies the body
  and leaves the Variant as it was. A string or container is changed by taking
  it, changing that, and putting the result back.
- `as_json(int indent = -1)` - the value as JSON. `-1` adds single spaces,
  `-2` is maximally compact, and `n >= 0` indents two spaces per level from
  `n`.

### Reading and coercing an unsigned value

The three unsigned types exist so that a value the program holds unsigned is
not read as a negative number. What a Variant does with one follows from that:

- **To the signed type of its own width, and no wider**, an unsigned value
  coerces exactly as its signed twin does, so two Variants holding the same
  bits behave the same way however they were built: `UInteger` as `Integer`,
  `ULong` as `Long`, `ULongLong` as `LongLong`. A `UInteger` of 4000000000
  has the sign bit set in that width, so it does not coerce to an `Integer` at
  all: the coercion is refused, as it is for any value the target could not
  hold, rather than answering -294967296.

- **Widening one goes by value, not by bits**, where the target is wide enough
  to hold it: a `UInteger` of 4000000000 read as a `LongLong` is 4000000000
  and not -294967296. That is the case the types exist for - a count that is
  large but not that large, and a type that can hold it.

A coercion that succeeds is a change of type and not only of reading: the
Variant is of the type asked for afterwards, so the next read may take it as
that type without coercing again.

A coercion is never lossy: one that would change the number is refused instead.
A refusal reports into the thread's error buffer, naming the type that was
asked for and the value that does not fit it:

	Cannot convert to a `Integer` because the value 4000000000 does not fit

Where assertions are on, that report is followed by an assertion and the
program stops, so a program under development cannot carry the wrong number
forward. Where assertions are compiled out there is no one to stop for, and
losing the value would be worse than a wrong answer: the Variant is left of the
closest type that holds the value instead - a `UInteger` beyond `INT_MAX` is
left a `Long` - so nothing is lost and any later read answers it correctly. The
caller that asked for too narrow a type still gets what it asked for, and the
buffer says why.

That is what `as_signed()` is for where the width is not known: it asks for the
closest signed type that holds the value rather than being told one, so a
caller with a number of unknown origin has a single call that either answers
the number or says that no signed type holds it.

- **Coercing one to a `String` renders it unsigned.** The digits of 4000000000
  are not what that number reads as a signed one, so the string the value
  becomes is the unsigned one. `as_strval()`, `format()` and `as_json()` all
  arrive there, which is why a marker for an unsigned parameter needs no
  specification to print it right.

Building:

- `operator<<(const Variant& n)` - a new `VariantArray` of this Variant
  followed by `n`, so arrays can be built by chaining.
