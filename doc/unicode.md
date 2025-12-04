## Raw Unicode character processing

`#include <char_encoding.h>`

<pre>
typedef char		UTF8;		// We don't assume un/signed
typedef uint16_t	UTF16;		// Used in Unicode 2, and 3 with surrogates
typedef	char32_t	UCS4;		// A UCS4 character, aka UTF-32, aka Rune
</pre>

### UTF8 processing

UTF8 consists of a leading byte which indicates how many bytes follow,
from zero to five. In practise, a UTF8 character may be encoded with a
longer sequence than required by zero-padding. This implementation will
not unintentionally generate non-minimum length encodings.

* `bool UTF8Is1st(UTF8)` indicates a valid leading byte,

* `bool UTF8Is2nd(UTF8)` the following bytes

* `int UTF8CorrectLen(UTF8)` returns the number of bytes expected in the
sequence starting with this byte, but does not check them

* `int UTF8Len(const UTF8* cp)` looks at the data and checks that the required
following bytes are all correct

* `UCS4 UTF8Get(const UTF8*& cp)` returns the next UCS4 character,
advancing cp, and handling illegal encodings by the method described
above

* `UCS4 UTF8Peek(const UTF8*& cp)` returns the next UCS4 character without
advancing cp

* `const UTF8* UTF8Backup(const UTF8* cp, const UTF8* limit)` backs up one
character, correctly handling illegal UTF8 to mirror UTF8Get

* `UTF8PutPaddedZero(UTF8*& cp, int length)` puts a zero character of
length bytes, which is sometimes useful to create a place-holder

### UCS4 processing

The full 32-bit range of UCS4 (aka UTF-32) may be encoded using six-byte
UTF-8 (not just 31 bits as in some implementations).  The raw bytes of
an illegal UTF-8 sequence is encoded here as a series of replacement
characters as the `xx` bits in 0x800000xx.  In this way there is no need
for an exception to be thrown on an illegal sequence, and naive programs
will pass such data unchanged.  The value 0xFFFFFFFF defined as UCS4_NONE
is used to mark EOF and in similar situations.

* `int UTF8Len(UCS4)` returns the required number of bytes to encode
a UCS4 character as UTF8

* `bool UCS4IsAlphabetic(UCS4)` A character is deemed alphabetic
if it is susceptible to case conversion, of if it is in a list of
208 non-case-convertible characters

* `int UCS4Digit(UCS4)` Unicode contains 20 digit ranges. This
returns the decimal value for a digit, or -1 for non-digits

* `bool UCS4IsDecimal(UCS4)` Return true if the character is decimal

* `int UCS4HexDigit(UCS4)` Return the decimal or hexadecimal (a-f,
A-F) value or -1

* `UCS4 UCS4ToUpper(UCS4)` Return the uppercase equivalent

* `UCS4 UCS4ToLower(UCS4)` Return the lowercase equivalent

* `UCS4 UCS4ToTitle(UCS4)` Return the title-case equivalent if one
exists, otherwise uppercase

* `bool UCS4IsWhite(UCS4)` In addition to ASCII white-space, there
are four other Unicode groups of whitespace

* `bool UCS4IsASCII(UCS4)` Return true if the UCS4 character is in
the ASCII range 0..0x7F

* `bool UCS4IsLatin1(UCS4)` Return true if the UCS4 character is
in the ISO-8859-1 range 0..0x7F

* `bool UCS4IsUTF16(UCS4)` Return true if the UCS4 character is in
the UTF16 range 0..0xFFFF (which includes surrogates)

* `bool UCS4IsUnicode(UCS4)` Return true if the UCS4 character is
in Unicode UTF16 range 0..10FFFF

* `bool UCS4IsIllegal(UCS4)` Return true if the character encodes
an out-of-sequence UTF8 byte

The actual classification functions use reduced tables derived from
the official standard. You might wish to expand these for fully
legal Unicode processing (or a future implementation may provide
them as a compile-time option).

### UTF-16

In case you should be unfortunate enough to need to support UTF-16,
these functions are provided.

* `bool UTF16IsSurrogate(UTF16 ch)` Returns true if the UTF16 word
is a surrogate (low or high)

* `bool UTF16Is1st(UTF16 ch)` Returns true if the UTF16 word is a
high surrogate

* `bool UTF16Is2nd(UTF16 ch)` Returns true if the UTF16 word is a
low surrogate

* `UTF16 UCS4HighSurrogate(UCS4 ch)` Return the high surrogate value
for the Unicode character

* `UTF16 UCS4LowSurrogate(UCS4 ch)` Return the low surrogate value
for the Unicode character

* `UTF16 UTF16Swab(UTF16 x)` Swap the bytes of a UTF16 word

* `UCS4 UTF16Get(const UTF16*& cp, const bool swap = false)` Return
a UCS4 character from one or two UTF16 words, advancing cp and
swapping bytes if requested

* `int UTF16Len(UCS4 ch)` Return the number of UTF16 words required
to encode the character

* `int UTF16Len(const UTF16* cp, bool swap = false)` Return the
number of UTF16 words that make up the next character

* `void UTF16Put(UTF16*& cp, UCS4 ch, bool swap = false)` Convert
and store a UCS4 character as UTF16, advancing cp
