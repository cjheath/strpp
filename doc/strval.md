## Unicode strings: the StrVal class

Unicode strings stored as UTF-8, with value semantics and fast compact
per-character indexing (light-weight copy-on-write implementation using
slices, and bookmarks to speed up character indexing).

Can also handle whatever character set is implied by raw 8-bit bytes in the
program's locale, (not yet always) converting to UTF-8 where processing
involves Unicode.

`#include	<strval.h>`

- By-value semantics (use StrVal like int, no explicit allocation, pass by
  reference/pointer, etc)
- Copies and substrings are slices (they do not copy the data)
- All strings are stored as UTF-8
- All references to individual characters are UCS4 (UTF-32, aka Runes)
- All string indexing is by character position, not byte offsets
- String scanning and indexing is efficient, with internal use of bookmarks
- Content sharing is SMP and thread-safe using atomic reference counting and
  garbage collection
- Any StrVal may be mutated - it will safely make a private copy of any
  shared data

### Public methods

Defined in [strval.h](https://github.com/cjheath/strpp/blob/main/include/strval.h).

Making one:

- `StrVal()` - the empty string. Every empty string shares one static body.
- `StrVal(const char* data, StrDataType dt = StrUTF8)` - copies NUL-terminated
  data.
- `StrVal(const char* data, Index length, size_t allocate = 0)` - copies a
  run of bytes of known length, preallocating if the length will grow.
- `StrVal(UCS4 character)` - one character, encoded to UTF-8.
- `StrVal(const StrRef&)` - from the body reference a Variant carries.
- `StrVal(Body*)` - a reference to a body that already exists, for statics.
- `StrVal(const StrVal&)`, `operator=` - both share the body, and the next
  write to either copies it.

Reading:

- `length()` - the number of characters, not bytes.
- `numBytes()` - the number of bytes of UTF-8 (or raw binary) data.
- `isEmpty()`, `explicit operator bool()` - whether anything is held.
- `operator[](int charNum)` - the character at an index, as UCS4; `'\0'` one
  past the end, and `UCS4_NONE` for an offset that is not there.
- `asUTF8()` - the bytes, NUL-terminated, copying the body first if this is a
  substring whose terminator was elided.
- `asUTF8(Index& bytes)` - the bytes without that guarantee, answering the
  byte count.
- `operator->()`, `operator*()`, `operator const Body&()` - the body itself.

Cutting one up:

- `substr(Index at, int len = -1)`, `head(n)`, `tail(n)`, `shorter(n)` - a
  slice, or everything but the last `n` characters. All are O(1).
- `find`/`rfind(UCS4 ch, int after/before)` - where a character is, or -1.
- `find`/`rfind(const StrVal&, int after/before)` - where a substring is.
- `findAny`/`rfindAny(const StrVal& s, int after/before)` - where any of the
  characters in `s` is.
- `findNot`/`rfindNot(const StrVal& s, int after/before)` - where a character
  not in `s` is.
- `compare(const StrVal&, CompareStyle = CompareRaw)`, `equalCI`, and the
  comparison operators - ordering, optionally case independent. `compare`
  with a style other than `CompareRaw` is not implemented yet.
- `static compare(a, b)`, `static equiv(a, b)` - the ordering and equality
  predicates a std::map needs of its key.
- `isStatic()` - true when the data is not owned by this string's body.

Building one:

- `operator+` with a string, a `const char*`, or a `UCS4` - a new string.
- `operator+=` with a string or a `UCS4` - appended in place.
- `operator*(int repeats)` - this string, repeated.
- `insert(Index pos, const StrVal&)`, `append`, `prepend` - insert into the
  copy this string owns.
- `asLower()`, `asUpper()`, `toLower()`, `toUpper()` - the first two answer a
  copy, the second two change this string and its length.
- `transform(std::function<StrVal(const char*& cp, const char* ep)>, int after = -1)`
  - rewrite each character from `after` on, with whatever the function
    answers.
- `asJSON()`, `toJSON()` - escaped for JSON; `toJSON` escapes in place and
  does not add the enclosing quotes.
- `asInt32(ErrNum* err, int radix = 0, Index* scanned = 0)` - the number this
  string reads as, in any radix from 2 to 36 and auto-detected when 0,
  reporting what it could not use.
- `static format(StrVal f, VariantArray args)` - the template string `f` with
  its substitutions taken from `args`.
- `StringArray::join(StrVal joiner)` - the elements of a `StringArray`,
  concatenated with `joiner` between them.

`Index` is `StrValIndex`, which is 32 bits unless the build says otherwise, so
a string is limited to 2^32 characters.

Example:

	// This example creates precisely three strings, but with four references
	#include        <stdio.h>
	#include        <strval.h>

	void greet(StrVal greeting)
	{
		StrVal  decorated = greeting + "! 🎉🍾\n";
		fputs(decorated.asUTF8(), stdout);
	}

	int main()
	{
		StrVal  hello("Hello, world");

		greet(hello);
	}

### Sharing, slices and NUL termination

A StrVal holds a reference to a reference-counted body, and a substring is a
slice of that body - it does not copy the characters. Two consequences worth
knowing:

- **While any slice of a body is alive, the body is shared**, so writing to
  any of them copies it first. That is what makes StrVal safe to pass by
  value, and it is also why a long-lived slice of a large string costs a
  copy when the string is next written to.

- **`asUTF8()` guarantees NUL termination; `asUTF8(Index& length)` does
  not.** The second form is for a byte range you already have a length for,
  and it will not copy the string to add a terminator. Where the bytes go to
  something that expects a C string, use the first form.

The empty string is the same for everyone: every `StrVal("")` refers to one
static empty body, which is why they can be built and copied freely.
