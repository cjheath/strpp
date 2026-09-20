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
  run of bytes of known length, preallocating if the length will grow. The run
  need not be NUL-terminated: none of it past the length is read, and the
  string's own terminator is added. `allocate` counts the terminator with the
  characters, so a string that will grow to n characters asks for n+1.
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
- `static format(StrVal f, VariantArray args, int depth = 2)` - the text `f`
  with each `{1}`, `{2}` and so on replaced by that parameter, which the marker
  may say how to render, and how far an array or a map among them is expanded.
  See "Substituting parameters into a text" below.
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

### Substituting parameters into a text

`format()` takes a text with markers in it and the parameters those markers
refer to, and answers the text with each parameter interpolated where the text
names it:

	StrVal::format("The {1} weighs {2} grams", VariantArray() << "parcel" << 250)
	// The parcel weighs 250 grams

The text names each parameter by its position in the array - `{1}` is the
first, `{2}` the second - rather than by the order they happen to appear in it,
so a translation may use them in another order, or leave one out. A marker is
`{`, the position, and `}`. A parameter is rendered as text: a number as its
digits, a string as itself. Anything the text wants around a parameter is
written into the text rather than added by the caller, so that a translator can
place it where that language wants it. Error messages are the commonest use of
this - the library's own are built from texts like this one - but the text may
be anything a program has to write:

	StrVal::format("The object `{1}` could not be changed", params)
	// The object `TOP.B` could not be changed

After a colon, a marker may say how its parameter is to be rendered. The
representation comes first, as one character - `b` binary, `o` octal, `d`
decimal, `x` and `X` hexadecimal - and carries no prefix of its own, because a
text that wants `0x` writes it itself, which leaves the prefix where a
translator can put it. A base that is not decimal renders the bit pattern of
the value at the width of its own type, which is what a base is for: an `int`
of -1 is `ffffffff`, and no sign is involved. Decimal renders the sign, and is
where a sign belongs. Then the least number of characters, padded for in front
with spaces, or with zeroes when its first digit is a `0`. Then, after a `<`,
the most characters: text longer than that is cut, and three dots - or a single
`…` - follow what was kept, in place of the end that was lost. Those two are the
only tails there are.

	{1}		as it stands
	{1:X}		as hexadecimal
	{1:8}		in at least eight characters, spaces in front
	{1:08}		...zeroes in front instead
	{1:X2<8}	hexadecimal, between two and eight characters
	{1:<40...}	at most forty: thirty-seven characters and three dots
	{1:<40…}	the same with an ellipsis: thirty-nine characters and …

A tail costs what it occupies, so what the value keeps is the maximum less
the tail's own length: three dots leave thirty-seven characters of the value,
and a single `…` leaves thirty-nine. Sizing everywhere counts characters, not
terminal columns: a wide glyph is one, and an ellipsis is one although it is
three bytes of UTF-8. The maximum is a maximum and not a target, so a value of
exactly forty characters comes through whole and takes no tail; only a longer
one is cut. A minimum is applied after the cut rather than before it, so
`{1:8<6...}` gives two spaces and then `abc...`.

A parameter that is an array or a map is expanded rather than named: an array
in brackets, its elements rendered the same way, and a map as JSON, which is
what a map is for. `format()`'s third argument is how deep that goes, and it
defaults to two levels; an array at that limit answers its type name in angle
brackets instead, as does a type with no rendering at all here. A map goes by
way of `as_json()`, which has no depth of its own and descends as far as the
map does. The limit belongs to the call and not to the text, since it is the
programmer who knows what is being passed and a translator who does not; a
parameter that needs another depth, or another rendering entirely, can be
rendered by the caller and passed as a string. A specification the type has no
use for is ignored, so `{1:x}` of an array is just the array:

	StrVal::format("{1}", VariantArray() << Variant(array))
	// [1, [x, y]]

A specification is applied as far as it is understood, and whatever else the
marker holds is passed over. A text may be translated, and a translation may
come from a catalog that nothing can have checked before the program runs, so a
part of a specification that means nothing here leaves the rest of it working
rather than striking the message out: `{1:08-}` is a zero-padded minimum of
eight with something after it that means nothing, and it renders as `{1:08}`
does. A minimum that *is* understood is still applied, so the marker shows what
it can.

A marker that names no parameter of the array - `{3}` where two were given, or
`{0}` - is left exactly as it stands, and so is anything in braces that is not
a marker at all, such as `{of}` or a lone `{`. A text that does not match its
parameters therefore shows the reader the marker it could not fill rather than
losing the text around it. A brace that is meant literally is doubled, as it is
in Python and .NET: `{{` is a `{`, and `}}` is a `}`. Nothing else needs
escaping, so a text may carry a regular expression, a path or a backslash as it
stands.

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
