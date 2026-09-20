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

### Building the library

The `Makefile` that comes with the library is a plain one, and a few settings
may be given to `make` to fit a build to its target. Each has a default that
suits a general purpose machine, and none of them changes what the library
does, only how much it may hold and how much room its own bookkeeping takes.

- `STRVALINDEXBITS` (32) - the width in bits of the index a string counts its
  characters with, from 8 up to the width of a pointer. The longest a string
  may be follows from it, and is enforced: a 16-bit index holds 65 534
  characters, and asking for one more [stops the program](error.md) rather
  than wrapping round. The narrower it is, the less memory the count itself
  takes.

- `ARRAYINDEXBITS` (32) - the same for the index an array body counts its
  elements with. The two are independent: a string body counts with the string
  index whatever this one says, so a 32-bit array may hold more elements than a
  16-bit string may hold characters.

- `DEPTH` (16) - the most levels of array or map that
  [formatting](strval.md) and JSON rendering descend into. A structure nested
  deeper than that answers its type name instead, so that one far deeper than
  any text needs cannot run away with the stack.

- `PEG_TRACE` (off) - traces the [PEG parsers](peg.md) as they run, writing to
  standard output.

A small target might be built with an index of two bytes and less depth than a
message needs:

	make STRVALINDEXBITS=16 ARRAYINDEXBITS=16 DEPTH=8

A threaded build chooses its model with `HAVE_PTHREADS`, `HAVE_FREERTOS`, `MSW`
or `NO_THREAD`, and a FreeRTOS build also sets `THREAD_DEFAULT_STACK_BYTES`,
`THREAD_DEFAULT_PRIORITY` and `MAX_THREAD`. See [Threads, locks and
thread-local storage](threading.md), and `include/thread_local.h` for the pool
of thread-local slots that a FreeRTOS build must make room for.

An ESP-IDF build registers the library with `CMakeLists.txt` and sets the
thread settings from `Kconfig` rather than from this Makefile.
