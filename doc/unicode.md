## Unicode character processing

`#include <char_encoding.h>`

	typedef char		UTF8;		// We don't assume un/signed
	typedef uint16_t	UTF16;		// Used in Unicode 2, and 3 with surrogates
	typedef	char32_t	UCS4;		// A UCS4 character, aka UTF-32, aka Rune

Characters are passed and returned as UCS4 whatever their encoding.
UCS4_NONE (0xFFFFFFFF) marks end of input.

### Illegal UTF-8

A byte sequence that is not legal UTF-8 is not an error. Its raw bytes are
returned as characters of the form 0x800000xx, one per byte, so naive code
passes such data through unchanged instead of needing an exception, and
`UTF8Backup` mirrors `UTF8Get` so that stepping back over them works.

### UTF-8

* `bool UTF8Is1st(UTF8)` a valid leading byte

* `bool UTF8Is2nd(UTF8)` a following byte

* `int UTF8CorrectLen(UTF8)` the number of bytes the sequence should have,
given this leading byte, without checking them

* `int UTF8Len(const UTF8* cp)` the actual length, having checked that the
following bytes are correct

* `UCS4 UTF8Get(const UTF8*& cp)` the next character, advancing cp

* `UCS4 UTF8Peek(const UTF8*& cp)` the next character, without advancing cp

* `const UTF8* UTF8Backup(const UTF8* cp, const UTF8* limit)` back one
character

* `UTF8PutPaddedZero(UTF8*& cp, int length)` a zero character occupying
  length
bytes, sometimes wanted as a place-holder

A non-minimum length encoding - a character zero-padded to a longer sequence
than it needs - is accepted when read, and is never produced when written.

### Encoding a character

* `int UTF8Len(UCS4)` the bytes needed to encode the character as UTF-8

* `int UTF16Len(UCS4)` the UTF-16 words needed

### Classifying and converting characters

* `bool UCS4IsAlphabetic(UCS4)` susceptible to case conversion, or in a list
  of
208 characters that are not

* `int UCS4Digit(UCS4)` the decimal value of a digit from any of Unicode's
  20
digit ranges, or -1

* `bool UCS4IsDecimal(UCS4)`

* `int UCS4HexDigit(UCS4)` the decimal or hexadecimal value of a digit, or
  -1

* `UCS4 UCS4ToUpper(UCS4)`, `UCS4ToLower(UCS4)`, `UCS4ToTitle(UCS4)` the
title-case equivalent where one exists, otherwise uppercase

* `bool UCS4IsWhite(UCS4)` ASCII white-space or any of Unicode's four other
white-space groups

* `bool UCS4IsASCII(UCS4)` 0..0x7F

* `bool UCS4IsLatin1(UCS4)` 0..0xFF

* `bool UCS4IsUTF16(UCS4)` 0..0xFFFF, including surrogates

* `bool UCS4IsUnicode(UCS4)` 0..0x10FFFF

* `bool UCS4IsIllegal(UCS4)` an out-of-sequence UTF-8 byte, as described
  above

The classification functions use reduced tables derived from the standard.

### UTF-16

In case you should be unfortunate enough to need to support UTF-16, these
functions are provided.

* `bool UTF16IsSurrogate(UTF16 ch)` a surrogate of either kind

* `bool UTF16Is1st(UTF16 ch)` a high surrogate

* `bool UTF16Is2nd(UTF16 ch)` a low surrogate

* `UTF16 UCS4HighSurrogate(UCS4 ch)`, `UCS4LowSurrogate(UCS4 ch)` the
surrogate values for the character

* `UTF16 UTF16Swab(UTF16 x)` swap the bytes of a word

* `UCS4 UTF16Get(const UTF16*& cp, bool swap = false)` the next character
  from
one or two words, advancing cp

* `int UTF16Len(const UTF16* cp, bool swap = false)` the words making up the
next character

* `void UTF16Put(UTF16*& cp, UCS4 ch, bool swap = false)` store the
  character
as UTF-16, advancing cp
