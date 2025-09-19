#if !defined(CHAR_ENCODING_H)
#define CHAR_ENCODING_H
/*
 * Encode/decode characters between UTF-8, Latin1, UTF-16 and UCS4.
 *
 * UCS4 (AKA UTF-32, Rune) is the ISO/IEC 10646 32-bit character encoding.
 * Truncation of 16 zero bits yields Unicode 2 (excluding surrogates)
 * Truncation of 24 zero bits yields Latin 1 (ISO-8859-1)
 * Truncation of 25 zero bits yields ASCII
 *
 * UTF-16 is used in Unicode 2, and in a mostly one-word encoding in Unicode 3.
 * We don't like it, so there's not much support here, but some APIs might want it.
 *
 * We don't support wchar_t, which is compiler-dependent, but required for some APIs.
 *
 * A UTF8 character is represented as 1-6 bytes.
 * The Unicode standard only defines 1-4 bytes, so we have a define to disable 6.
 * We also have a define to control the behaviour of an illegal UTF-8 sequence.
 * A first byte with a most significant bit of zero is a single ASCII byte.
 * The bytes after the first always have most significant two bits == "10",
 * which never occurs in the first byte - thus it's possible to jump into
 * the middle of a UTF8 string and reliably find the start of a character.
 *
 * Applications which need to signal EOF should use the UCS-4 character 0xFFFFFFFF,
 * which in char32_t notation is equivalent to the integer value -1. The definition
 * UCS4_NONE is used for this value.
 *
 * Illegal UTF-8 handing:
 * A byte which does not start a UTF-8 character is returned as a 32-bit value
 * with the high order bit set, as in, 0x800000yy. These are legal Unicode
 * replacement characters as defined in <https://unicode.org/faq/utf_bom.html>.
 *
 * 6-byte UTF-8 as implemented here can encode a full 32-bit value, not just 31
 * as is more commonly done. Note that both UCS4_NONE and the UCS4 replacement
 * character values can also be encoded as six-byte UTF-8, but not by most other
 * 6-byte UTF8 implementations, so in practise this is unlikely to occur.
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<assert.h>
#include	<cstdint>

typedef char		UTF8;		// We don't assume un/signed
typedef uint16_t	UTF16;		// Used in Unicode 2, and 3 with surrogates
typedef	char32_t	UCS4;		// A UCS4 character, aka UTF-32, aka Rune

/*
 * Access to some character property tables is declared below.
 * Many more are possible, see ftp://ftp.unicode.org/Public/UNIDATA/PropList.txt
 */
#define	UCS4_NONE	0xFFFFFFFF	// Marker indicating no UCS4 character
#define	UCS4_REPLACEMENT ((UCS4)0x0000FFFD)	// substitute for an unknown char
#define	UCS4_NO_GLYPH	"\xEF\xBF\xBD"	// UTF8 character used to display where the correct glyph is not available

/*
 * UCS4 classification and conversion
 */
bool		UCS4IsAlphabetic(UCS4 ch);	// Letters used to form words
bool		UCS4IsIdeographic(UCS4 ch);	// Asian pictograms, mostly
bool		UCS4IsDecimal(UCS4 ch);		// Decimal digits only
int		UCS4Digit(UCS4 ch);		// Digit value 0-9 or -1 if not digit
int		UCS4HexDigit(UCS4 ch);		// Digit value 0-9, a-f/A-F or -1 if not digit
UCS4		UCS4ToUpper(UCS4 ch);		// To upper case
UCS4		UCS4ToLower(UCS4 ch);		// To lower case
UCS4		UCS4ToTitle(UCS4 ch);		// To Title or upper case
inline bool	UCS4IsWhite(UCS4 ch)
		{
			return ch == ' '
				|| ch == '\t' || ch == '\n' || ch == '\r'
				|| ch == 0x00A0
				|| (ch >= 0x2000 && ch <= 0x200B)
				|| (ch >= 0x2028 && ch <= 0x2029)
				|| ch == 0x3000;
		}
inline bool	UCS4IsASCII(UCS4 ch) { return ch < 0x00000080; }
inline bool	UCS4IsLatin1(UCS4 ch) { return ch < 0x00000100; }
inline bool	UCS4IsUTF16(UCS4 ch) { return ch < 0x00010000; }
inline bool	UCS4IsUnicode(UCS4 ch) { return ch < 0x00110000; }

// Overloads of ctype functions. Those better not be macros!
inline bool	isalpha(UCS4 c) { return UCS4IsAlphabetic(c); }
inline bool	isdigit(UCS4 c) { return UCS4IsDecimal(c); }
inline bool	isalnum(UCS4 c) { return UCS4IsAlphabetic(c) || UCS4IsDecimal(c); }
inline UCS4	toupper(UCS4 c) { return UCS4ToUpper(c); }
inline UCS4	tolower(UCS4 c) { return UCS4ToLower(c); }
inline bool	isspace(UCS4 c) { return UCS4IsWhite(c); }

inline bool
UCS4IsIllegal(UCS4 ucs4)	// Does this UCS4 character encode an illegal utf-8 byte?
{
	return (ucs4 & 0xFFFFFF00) == 0x80000000 || ucs4 == 0xFFFFFFFF;
}

inline UCS4
UTF8EncodeIllegal(UTF8 illegal)	// Encode an illegal UTF8 byte as a UCS4 replacement
{
	return 0x80000000 | (illegal&0xFF);
}

inline bool
UTF8Is1st(UTF8 ch)
{
	return (ch & 0xC0) != 0x80;	// A non-1st byte is always 0b10xx_xxxx
}

inline bool
UTF8Is2nd(UTF8 ch)
{
	return (ch & 0xC0) == 0x80;
}

// Get length of UTF8 from UCS4
inline int
UTF8Len(UCS4 ch)
{
	if (ch < 0)
	{
		if (UCS4IsIllegal(ch))	// MSB is sign bit
			return 1;
		return 6;
	}
	if (ch < (1<<7))	// 7 bits:
		return 1;	// ASCII
	if (ch < (1<<11))	// 11 bits:
		return 2;	// two bytes
	if (ch < (1<<16))	// 16 bits
		return 3;	// three bytes
	if (ch < (1<<21))	// 21 bits
		return 4;	// four bytes
	if (UCS4IsIllegal(ch))	// In case UCS4 > 4 bytes or is unsigned
		return 1;
	if (ch < (1<<26))	// 26 bits
		return 5;	// five bytes
	return 6;		// six bytes
}

// From a candidate UTF8 first byte, return the correct length of the UTF8 sequence it introduces:
inline int
UTF8CorrectLen(UTF8 c)
{
	if ((unsigned char)c < 0x80) return 1;		// 0b0xxx_xxxx (7 literal bits, no extension characters)
	if ((unsigned char)c < 0xC0) return 0;		// 0b10xx_xxxx (6 literal extension bits, not valid 1st character)
	if ((unsigned char)c < 0xE0) return 2;		// 0b110x_xxxx (5 literal and 1x6 extension character = 11 bits)
	if ((unsigned char)c < 0xF0) return 3;		// 0b1110_xxxx (4 literal and 2x6 extension character = 16 bits)
	if ((unsigned char)c < 0xF8) return 4;		// 0b1111_0xxx (3 literal and 3x6 extension character = 21 bits)
	if ((unsigned char)c < 0xFC) return 5;		// 0b1111_10xx (2 literal and 4x6 extension character = 26 bits)
	// Some implementations only implement 1 literal bit for the six-byte form, so only generate 31 bits.
	/*if (c < 0xFE)*/ return 6;	// 0b1111_11xx (2 literal and 5x6 extension character = 32 bits)
}

// Return the actual length of a correct UTF8 sequence, or its replacement if not correct
inline int
UTF8Len(const UTF8* cp)	// REVISIT: Add an error callback pointer here?
{
	switch (int len = UTF8CorrectLen(*cp))
	{	// This reminds me of Duff's Device :)
	case 6: if (UTF8Is1st(cp[5])) goto illegal;
		// Fall through
	case 5: if (UTF8Is1st(cp[4])) goto illegal;
		// Fall through
	case 4: if (UTF8Is1st(cp[3])) goto illegal;
		// Fall through
	case 3: if (UTF8Is1st(cp[2])) goto illegal;
		// Fall through
	case 2: if (UTF8Is1st(cp[1])) goto illegal;
		// Fall through
	case 1:
		return len;
	case 0:
	default:					// Cannot happen, but keep the compiler happy
	illegal:
#if defined(UTF8_ASSERT)
		assert(!"Illegal or unsupported UTF-8 sequence");
		return 0;
#else
		return 1;		// Use a replacement character
#endif
	}
}

inline UCS4
UTF8Get(const UTF8*& cp)	// REVISIT: Add an error callback pointer here?
{
	const	UTF8*	sp = cp;
	UCS4		ch = *cp;
	static	unsigned char	masks[] = { 0xFF, 0x7F, 0x1F, 0x0F, 0x07, 0x03, 0x03 };

	int		len = UTF8CorrectLen(ch = *cp);
	ch &= masks[len];
	switch (len)
	{
	case 6:	cp++;
		if (UTF8Is1st(*cp)) goto illegal;
		ch = (ch << 6) | (*cp&0x3F);
		// Fall through
	case 5:	cp++;
		if (UTF8Is1st(*cp)) goto illegal;
		ch = (ch << 6) | (*cp&0x3F);
		// Fall through
	case 4:	cp++;
		if (UTF8Is1st(*cp)) goto illegal;
		ch = (ch << 6) | (*cp&0x3F);
		// Fall through
	case 3:	cp++;
		if (UTF8Is1st(*cp)) goto illegal;
		ch = (ch << 6) | (*cp&0x3F);
		// Fall through
	case 2:	cp++;
		if (UTF8Is1st(*cp)) goto illegal;
		ch = (ch << 6) | (*cp&0x3F);
		// Fall through
	case 1:	cp++;
		return ch;

	case 0:
	default:
	illegal:	
		cp = sp+1;
#if defined(UTF8_ASSERT)
		assert(!"Illegal or unsupported UTF-8 sequence");
		return 0;
#else
		return UTF8EncodeIllegal(*sp);
#endif
	}
}

inline UCS4
UTF8Peek(const UTF8*& cp)
{
	const UTF8*	tp = cp;
	UCS4		ch = UTF8Get(tp);
	return ch;
}

inline const UTF8*
UTF8Backup(const UTF8* cp, const UTF8* limit = 0)
{
	if (limit == 0			// No limit provided
	 || cp-limit > 6)		// or in any case limit to 6 bytes
		limit = cp-6;
	if (limit >= cp)
		return 0;

	// The checking here handles illegal characters analogously to going forward
	for (const UTF8* sp = cp-1; sp >= limit; --sp)	// Backup to the limit
		if (UTF8Is1st(*sp))			// Found the start char
		{
			if (UTF8CorrectLen(*sp) >= cp-sp)
				return sp;		// Legal prefix (at least) of a UTF8 sequence
			break;	// Illegal, backup 1 byte if this backup won't consume all bytes scanned
		}
	return cp-1;	// No start char (valid 1st byte) found
}

inline void
UTF8PutPaddedZero(UTF8*& cp, int length)
{
	assert(length >= 0 && length <= 6);
	if (length > 1)
	{
		*cp++ = "\x0\xC0\xE0\xF0\xF8\xFC"[length];	// Put lead byte with zero payload
		while (--length > 0)
			*cp++ = 0x80;				// Put trailing bytes with zero payload
	}
	else
		*cp++ = 0;
}

// Store UTF8 from UCS4
inline void
UTF8Put(UTF8*& cp, UCS4 ch)
{
	switch (UTF8Len(ch))
	{
	default:
		return;		// Error caught in UTF8Len()

	case 1:			// Single byte
		*cp++ = (UTF8)ch;	// If UCS4IsIllegal(ch) encode it anyway
		return;

	case 2:		// 5 data bits in 1st byte, 6 in next
		*cp++ = 0xC0 | (UTF8)((ch >>  6) & 0x1F);
		*cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
		return;

	case 3:		// 4 data bits in 1st byte, 6 in each of 2 more
		*cp++ = 0xE0 | (UTF8)((ch >> 12) & 0x0F);
		*cp++ = 0x80 | (UTF8)((ch >>  6) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
		return;

	case 4:		// 3 data bits in 1st byte, 6 in each of 3 more
		*cp++ = 0xF0 | (UTF8)((ch >> 18) & 0x07);
		*cp++ = 0x80 | (UTF8)((ch >> 12) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  6) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
		return;

	case 5:		// 2 data bits in 1st byte, 6 in each of 4 more
		*cp++ = 0xF8 | (UTF8)((ch >> 24) & 0x03);
		*cp++ = 0x80 | (UTF8)((ch >> 18) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >> 12) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  6) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
		return;

	case 6:		// 2 data bits in 1st byte, 6 in each of 5 more
		*cp++ = 0xFC | (UTF8)((ch >> 30) & 0x03);
		*cp++ = 0x80 | (UTF8)((ch >> 24) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >> 18) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >> 12) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  6) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
		return;
	}
}

/*
 * UTF-16
 *
 * A UTF16 value isn't always a whole character, but may instead contain
 * a surrogate. Surrogates come in pairs, the first from a set of 1024
 * values which is disjoint from the second set, also of 1024.	This
 * extends UTF16 to a 20bit character set.  If you are breaking strings
 * up, you must not break in the middle of a surrogate pair.  Most code
 * doesn't need to worry as it don't break strings in a way that can
 * cause this, others can call isSurrogate(), is1st() or is2nd() to act
 * correctly.
 */
inline bool
UTF16IsSurrogate(UTF16 ch)
{
	return (ch & 0xF800) == 0xD800;
}

inline bool
UTF16Is1st(UTF16 ch)
{
	return (ch & 0xFC00) == 0xD800;
}

inline bool
UTF16Is2nd(UTF16 ch)
{
	return (ch & 0xFC00) == 0xDC00;
}

inline UTF16
UTF16Swab(UTF16 x)
{
	return (UTF16)(
		((x & (UTF16)0x00ffU) << 8)
	      | ((x & (UTF16)0xff00U) >> 8)
	);
}

inline UCS4
UTF16Get(const UTF16*& cp, const bool swap = false)
{
	UTF16	c1 = swap ? UTF16Swab(cp[0]) : cp[0];
 
	if (!UTF16IsSurrogate(c1))
	{ 
		cp++;
		return c1;
	}

	// Check for correct surrogate pair:
	assert(UTF16Is1st(c1));
	if (UTF16Is1st(c1))
	{	
		UTF16 c2 = swap ? UTF16Swab(cp[1]) : cp[1];

		assert(UTF16Is2nd(c2));
		if (UTF16Is2nd(c2))
		{
			cp += 2;
			return (((UCS4) c1 & ~0xD800) << 10)
				+ ((UCS4) c2 & ~0xDC00)
				+ 0x10000;
		}
	}
 
	// Lone surrogate or reversed pair. 
	cp++;
	return UCS4_REPLACEMENT; 
}

inline int
UTF16Len(UCS4 ch)				// Get UTF16 length from UCS4
{
	assert(UCS4IsUnicode(ch));
	if (UCS4IsUnicode(ch))
		return ch > 0x0000FFFF ? 2 : 1;
	else 
		return 1;	// Replacement char is a single UTF-16 codepoint
}

inline int
UTF16Len(const UTF16* cp, bool swap = false)	// Get len from 1st chr
{
	return UTF16Is1st(swap ? UTF16Swab(*cp) : *cp) ? 2 : 1;
}

inline void
UTF16Put(UTF16*& cp, UCS4 ch, bool swap = false)	// Store UTF16 from UCS4
{
	if (ch <= 0xFFFF)
	{
		*cp++ = (swap ? UTF16Swab((UTF16)ch) : (UTF16)ch);
		return;
	}
	assert(UCS4IsUnicode(ch));
	if (UCS4IsUnicode(ch))
	{
		UTF16 v1 = 0xD800 + (UTF16)((ch - 0x10000) >> 10);
		UTF16 v2 = 0xDC00 + (UTF16)(ch & 0x3FF);

		*cp++ = (swap ? UTF16Swab(v1) : v1);
		*cp++ = (swap ? UTF16Swab(v2) : v2);
	}
	else
	{
		// Bad value
		*cp++ = (swap ? UTF16Swab(UCS4_REPLACEMENT) : UCS4_REPLACEMENT);
	}
}

#endif
