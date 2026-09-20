## strpp - StringPlusPlus - pronounced "strip"

A value-oriented C++ library implementing maps, arrays and strings (with
slices), raw Unicode processing, Variant type, pattern-matching and parsing.
Designed to minimise dynamic memory allocation and memory safety issues,
_strpp_ provides efficient but advanced functionality on machines with
restricted memory, such as many embedded systems.

_Strpp_ uses copy-on-write to implement _value semantics_ on shared objects
([Unicode strings](strval.md), generic [arrays](array.md) and
[maps](cowmap.md)) using thread-safe atomic reference-counting. Values may
be passed by copying references, which means you can pass complex structures
cheaply without much fear of aliasing, memory leaks, or object lifetime
violations.

The [Variant class](variant.md) provides type-safe support for passing any
data object, as is common in interpreted languages (Perl, Ruby, Python).

Greedy [PEG expressions](pegexp.md) with look-ahead assertions match text
without backtracking, and all their operators are in the prefix position,
which does not require a compilation step (or memory allocation) for
efficient execution. These _Pegular Expressions_ are also composed into full
(non-regular) [PEG grammars](peg.md). Parser template parameters allow
capturing parse results, with a generic [Abstract Syntax Tree
builder](https://github.com/cjheath/strpp/blob/main/include/peg_ast.h) for
any of those grammars.

The [rx](https://github.com/cjheath/strpp/blob/main/rx/README.md) library
provides a regular expression implementation using `StrVal` that is
ReDoS-safe (using the Thompson algorithm). These regular expressions are
deprecated in favour of the PEG expressions above.

[Px](https://github.com/cjheath/px) is an external parser generator for those
grammars: it compiles a grammar into a compact table-driven parser that
allocates no memory, and generates grammar documentation using Javascript
and SVG.
