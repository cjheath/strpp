## strpp - StringPlusPlus - pronounced "strip"

A value-oriented C++ library implementing maps, arrays and strings (with
slices), raw Unicode processing, Variant type, pattern-matching and parsing.
Designed to minimise dynamic memory allocation and memory safety issues,
_strpp_ provides efficient but advanced functionality on machines with
restricted memory, such as many embedded systems.

_Strpp_ uses copy-on-write to implement _value semantics_ on shared objects
([Unicode strings](include/strval.h), generic [arrays](include/array.h)
and [maps](include/cowmap.h)) using thread-safe atomic reference-counting.
Values may be passed by copying references, which means you can pass
complex structures cheaply without much fear of aliasing, memory leaks, or
object lifetime violations.

The [Variant class](include/variant.h) provides type-safe support for passing
any data object, as is common in interpreted languages (Perl, Ruby, Python).

The <[rx](rx/README.md)> library provides a regular expression implementation
using `StrVal` that is ReDOS-safe (using the Thompson algorithm).

These regular expressions are however deprecated in favour of greedy
[PEG expressions](include/pegexp.h) with look-ahead assertions. All PEG
operators are in the prefix position, which does not require a compilation step
(or memory allocation) for efficient execution. These _Pegular Expressions_
are also composed into full (non-regular) [PEG grammars](doc/peg.md) with a
[parser generator](https://github.com/cjheath/px) to produce compact table-driven parsers that allocate no
memory. The _Px_ compiler also generates grammar documentation using Javascript
and SVG, and will produce Textmate syntax highlighting patterns for IDEs and
text generators to assist implementation of pretty-printer output.
Parser template parameters allow capturing parse results, with a generic
[Abstract Syntax Tree builder](include/peg_ast.h) for any grammar specified in [Px](https://github.com/cjheath/px).

### Raw Unicode character processing

#include	<[char_encoding.h](include/char_encoding.h)>

See [Unicode](doc/unicode.md)

Rapid inlined encode/decode for 32-bit character values in 1..6 bytes of UTF8.
Gracefully handles errors in UTF-8 coding.

#include	<[charpointer.h](include/char_ptr.h)>

#include	<[char_ptr.h](include/char_ptr.h)>

#include	<[utf8_ptr.h](include/utf8_ptr.h)>

CharPointer selects either 8-bit (char_ptr) or UTF-8 (utf8_ptr) boxed character pointers.
The two kinds support unguarded pointers, pointers that will not advance past a NUL,
and pointers which will not retreat before the start either. All are used to facilitate
correct coding by restricting the unguarded behaviour of the C/C++ languages.

### Error and ErrNum type

#include	<[error.h](include/error.h)>.

See [Error](doc/error.md)

### Unicode strings: the StrVal class

#include	<[strval.h](include/strval.h)>

See [StrVal](doc/strval.md)

### Array with slices

#include	<[array.h](include/array.h)>

See [Array](doc/array.md)

### COWMap, copy-on-write map template

#include	<[cowmap.h](include/cowmap.h)>

See [CowMap](doc/cowmap.md)

### Variant Data Type

#include	<[variant.h](include/variant.h)>

See [Variant](doc/variant.md)

### Prefix Regular Expression pattern matching

#include	<[pegexp.h](include/pegexp.h)>

See [Pegexp](doc/pegexp.md)

### PEG parsing

#include	<[peg.h](include/peg.h)>

See [Peg](doc/peg.md)

### Px, a PEG parser generator

See [Px](https://github.com/cjheath/px)

### Cross-platform C++ Threading support

#include	<[thread.h](include/thread.h)>
#include	<[lockfree.h](include/lockfree.h)>
#include	<[condition.h](include/condition.h)>

_Thread_ is a base class which requires a virtual _run_() method and provides
_suspend_, _resume_, _join_, _joinAny_, _yield_(ms), etc.
_lockfree.h_ implements atomic an Latch class allowing construction of lock-free code,
and _condition,h_ provides an cross-platform implementation of condition variables.

### Regular Expressions

A ReDOS-resistant Thompson-style regexp compiler/interpreter using StrVal.

#include	<[strregex.h](rx/strregex.h)>

See [Rx](rx/README.md)

### LICENSE

The MIT License. See the LICENSE file for details.
