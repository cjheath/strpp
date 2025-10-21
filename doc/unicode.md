## Raw Unicode character processing

`#include <char_encoding.h>`

<pre>
typedef char		UTF8;		// We don't assume un/signed
typedef uint16_t	UTF16;		// Used in Unicode 2, and 3 with surrogates
typedef	char32_t	UCS4;		// A UCS4 character, aka UTF-32, aka Rune
</pre>

The full 32-bit range of UCS4 may be encoded using six-byte UTF-8
(not just 31 bits as in some implementations).  But beware, because an
illegal UTF-8 sequence is processed as a series of replacement characters,
encoded as the `xx` bits in 0x800000xx. In this way there is no need for
an exception to be thrown on an illegal sequence.

A number of functions deal with classifying UCS4 values. A short list
is here, but read the header file for details:

UCS4IsAlphabetic, UCS4IsIdeographic, UCS4IsDecimal, UCS4Digit, UCS4ToUpper,
UCS4ToLower, UCS4ToTitle, UCS4IsWhite, UCS4IsASCII, UCS4IsLatin1,
UCS4IsUTF16, UCS4IsUnicode, and others.

The actual classification functions use reduced tables. You might wish
to expand these for fully legal Unicode processing (or a future implementation
may provide them as a compile-time option).

There are three useful inline functions for dealing with UTF8 pointers:
<pre>
UCS4		UTF8Get(const UTF8*& cp);	// Get next UCS4 or replacement, advancing cp
const UTF8*	UTF8Backup(const UTF8* cp, const UTF8* limit = 0); // Backup cp to start of previous char
void		UTF8Put(UTF8*& cp, UCS4 ch);	// Put ch as 1-6 bytes of UTF8, advancing cp
</pre>

Similar methods exist for UTF16, but we don't like using those (generated C code may require them)
