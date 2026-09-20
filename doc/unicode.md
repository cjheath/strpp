## Unicode character processing

This library of global methods provides efficient low-level support for
32-bit Unicode characters, encoded as UTF-8 for preference, UTF-16 where
necessary, and able to handle incorrect encodings without corrupting the
data further. The UTF-8 methods handle the entire 32-bit range by extending
UTF-8 up to six bytes (this is sometimes called WTF-8).

`#include	<char_encoding.h>`

Defined in
[char_encoding.h](https://github.com/cjheath/strpp/blob/main/include/char_encoding.h).

	typedef char		UTF8;		// We don't assume un/signed
	typedef uint16_t	UTF16;		// Used in Unicode 2, and 3 with surrogates
	typedef	char32_t	UCS4;		// A UCS4 character, aka UTF-32, aka Rune

Characters are passed and returned as UCS4 whatever their encoding.
UCS4_NONE (0xFFFFFFFF) marks end of input.

### UTF-8

- `bool UTF8Is1st(UTF8)` - a valid leading byte.
- `bool UTF8Is2nd(UTF8)` - a following byte.
- `int UTF8CorrectLen(UTF8)` - the number of bytes the sequence should have,
  given this leading byte, without checking them.
- `int UTF8Len(const UTF8* cp)` - the actual length, having checked that the
  following bytes are correct.
- `int UTF8Len(UCS4)` - how many bytes needed to encode the character as UTF-8.
- `UCS4 UTF8Get(const UTF8*& cp)` - the next character, advancing cp.
- `UCS4 UTF8Peek(const UTF8*& cp)` - the next character, without advancing cp.
- `const UTF8* UTF8Backup(const UTF8* cp, const UTF8* limit)` - back one
  character.
- `void UTF8Put(UTF8*& cp, UCS4 ch)` - store a character as UTF-8, advancing
  cp.
- `void UTF8PutPaddedZero(UTF8*& cp, int length)` - a zero character occupying
  `length` bytes, sometimes wanted as a place-holder.
- `UCS4 UTF8EncodeIllegal(UTF8)` - the UCS4 value that stands for an illegal
  UTF-8 byte.

A non-minimum length encoding - a character zero-padded to a longer sequence
than it needs - is accepted when read, but is never produced when written.

### Classifying and converting characters

Classification here is restricted to what can be done without carrying
Unicode's full code tables: these functions use reduced tables derived from
the standard.

- `bool UCS4IsAlphabetic(UCS4)` - susceptible to case conversion, or in a list
  of 208 characters that are not.
- `int UCS4Digit(UCS4)` - the decimal value of a digit from any of Unicode's
  20 digit ranges, or -1.
- `bool UCS4IsDecimal(UCS4)` - a decimal digit.
- `int UCS4HexDigit(UCS4)` - the decimal or hexadecimal value of a digit, or
  -1.
- `int ASCIIDigit(UCS4)` - the value of an ASCII digit only, or -1.
- `UCS4 UCS4ToUpper(UCS4)`, `UCS4ToLower(UCS4)`, `UCS4ToTitle(UCS4)` - the
  upper, lower or title-case equivalent, title falling back to upper.
- `bool UCS4IsWhite(UCS4)` - ASCII white-space, or any of Unicode's four other
  white-space groups.
- `bool UCS4IsASCII(UCS4)` - 0..0x7F.
- `bool UCS4IsASCIIPrintable(UCS4)` - ASCII that is not a control character.
- `bool UCS4IsLatin1(UCS4)` - 0..0xFF.
- `bool UCS4IsUTF16(UCS4)` - 0..0xFFFF, including surrogates.
- `bool UCS4IsUnicode(UCS4)` - 0..0x10FFFF.
- `bool UCS4IsIllegal(UCS4)` - an out-of-sequence UTF-8 byte, as described
  above.

ctype-style overloads of `isalpha`, `isdigit`, `isalnum`, `isspace`,
`isupper`, `islower`, `toupper` and `tolower` take a UCS4 character too.

### Illegal UTF-8

A byte sequence that is not legal UTF-8 does not produce panics. Its raw
bytes are returned as separate UCS4 codes of the form 0x800000xx, one per
byte, so naive code passes such data through unchanged instead of needing an
exception, and `UTF8Backup` mirrors `UTF8Get` so that stepping back over
them works. See `UCS4IsIllegal`, `UTF8EncodeIllegal`.

### UTF-16

In case you should be unfortunate enough to need to support UTF-16 (for
example, for JSON), these functions are provided.

- `int UTF16Len(UCS4)` - how many UTF-16 words needed.
- `bool UTF16IsSurrogate(UTF16 ch)` - a surrogate of either kind.
- `bool UTF16Is1st(UTF16 ch)` - a high surrogate.
- `bool UTF16Is2nd(UTF16 ch)` - a low surrogate.
- `UTF16 UCS4HighSurrogate(UCS4 ch)`, `UCS4LowSurrogate(UCS4 ch)` - the
  surrogate values for the character.
- `UTF16 UTF16Swab(UTF16 x)` - swap the bytes of a word.
- `UCS4 UTF16Get(const UTF16*& cp, bool swap = false)` - the next character
  from one or two words, advancing cp.
- `int UTF16Len(const UTF16* cp, bool swap = false)` - the words making up the
  next character.
- `void UTF16Put(UTF16*& cp, UCS4 ch, bool swap = false)` - store the
  character as UTF-16, advancing cp.

`UCS4_REPLACEMENT` is the character written where one is unknown, and
`UCS4_NO_GLYPH` the UTF-8 sequence used to display it.
