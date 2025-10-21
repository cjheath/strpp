## strpp - StringPlusPlus - pronounced "strip"

A value-oriented C++ library implementing maps, arrays and strings, with
slices, Unicode processing, a Variant type, pattern-matching and parsing.

This lightweight library implements *value semantics* on shared objects
and sliced arrays using atomic reference-counting with copy-on-write.

Values may be passed by (atomically) copying references, which means you
can pass or "copy" complex structures cheaply without fear of aliasing,
memory leaks, or object lifetime violations.

A ReDOS-safe (Thompson algorithm) implementation of regular expressions
is provided, but is deprecated in favour of greedy PEG expressions
with look-ahead assertions, which do not require a compilation step for
efficient execution. These _Pegular expressions_ are composed into full
PEG grammars with a parser generator, and can use the Variant class
to construct Abstract Syntax Trees for any grammar specified in the
Px language.

Designed to minimise dynamic memory allocation, _strpp_ provides efficient
but advanced functionality on machines with restricted memory, such as
many embedded systems.

## Raw Unicode character processing

#include	<[char_encoding.h](char_encoding.h)>

See [Unicode](doc/unicode.md)

Rapid inlined encode/decode for 32-bit character values in 1..6 bytes of UTF8.
Gracefully handles errors in UTF-8 coding.

## Error and ErrNum type

#include	<[error.h](include/error.h)>.

See [Error](doc/error.md)

## Unicode strings: the StrVal class

#include	<[strval.h](include/strval.h)>

See [StrVal](doc/strval.md)

## Array with slices

#include	<[array.h](include/array.h)>

See [Array](doc/array.md)

## COWMap, copy-on-write map template

#include	<[cowmap.h](include/cowmap.h)>

See [CowMap](doc/cowmap.md)

## Variant Data Type

#include	<[variant.h](include/variant.h)>

See [Variant](doc/variant.md)

## Prefix Regular Expression pattern matching

#include	<[pegexp.h](include/pegexp.h)>

See [Pegexp](doc/pegexp.md)

## PEG parsing

#include	<[peg.h](include/peg.h)>

See [Peg](doc/peg.md)

## Px, a PEG parser generator

See [Px](doc/px.md)

## Regular Expressions

A ReDOS-resistant Thompson-style regexp compiler/interpreter using StrVal.

#include	<[strregex.h](rx/strregex.h)>

See [Rx](rx/README.md)

## LICENSE

The MIT License. See the LICENSE file for details.
