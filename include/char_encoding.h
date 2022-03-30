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
 * A UTF8 character is represented as 1-4 bytes.
 * A first byte with a most significant bit of zero is a single ASCII byte.
 * The bytes after the first always have most significant two bits == "10",
 * which never occurs in the first byte - thus it's possible to jump into
 * the middle of a UTF8 string and reliably find the start of a character.
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<assert.h>

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

inline bool
UTF8Is1st(UTF8 ch)
{
	return (ch & 0xC0) != 0x80;
}

// Get length of UTF8 from UCS4
inline int
UTF8Len(UCS4 ch)
{
	if (ch <= 0x0000007F)	// 7 bits:
		return 1;	// ASCII
	if (ch <= 0x000007FF)	// 11 bits:
		return 2;	// two bytes
	if (ch <= 0x0000FFFF)	// 16 bits
		return 3;	// three bytes
	if (ch <= 0x001FFFFF)	// 21 bits
		return 4;	// four bytes
	assert(ch <= 0x03FFFFFF);
#if defined(UTF8_SIX_BYTE)	// We don't support six-byte UTF-8 by defined, for efficiency reasons
	if (ch <= 0x03FFFFFF)	// 26 bits
		return 5;	// five bytes
	if (ch <= 0x7FFFFFFF)	// 31 bits
		return 6;	// six bytes
	assert(ch <= 0x7FFFFFFF); // max UTF-8 value
#endif
	return 0;		// safety.
}

inline UCS4
UTF8Get(const UTF8*& cp)
{
	UCS4		ch	= UCS4_NONE;

	switch (*cp&0xF0)
	{
	case 0x00: case 0x10: case 0x20: case 0x30:
	case 0x40: case 0x50: case 0x60: case 0x70:	// One UTF8 byte
		ch = *cp++ & 0xFF;
		break;

	case 0x80: case 0x90: case 0xA0: case 0xB0:	// Illegal, not first byte
		break;

	case 0xC0: case 0xD0:				// Two UTF8 bytes
		assert(!UTF8Is1st(cp[1]));
		if (UTF8Is1st(cp[1]))
			break;
		ch = ((UCS4)(cp[0]&0x1F)<<6) | (cp[1]&0x3F);
		cp += 2;
		break;

	case 0xE0:					// Three UTF8 bytes
		assert(!UTF8Is1st(cp[1]) && !UTF8Is1st(cp[2]));
		if (UTF8Is1st(cp[1]) || UTF8Is1st(cp[2]))
			break;
		ch = ((UCS4)(cp[0]&0xF)<<12)
			| ((UCS4)(cp[1]&0x3F)<<6)
			| (cp[2]&0x3F);
		cp += 3;
		break;

	case 0xF0:					// Four UTF8 bytes
		assert(!UTF8Is1st(cp[1]) && !UTF8Is1st(cp[2]) && !UTF8Is1st(cp[3]));
		if (UTF8Is1st(cp[1]) || UTF8Is1st(cp[2]) || UTF8Is1st(cp[3]))
			break;
		if ((cp[0] & 0x0C) == 0)
		{
			ch = ((UCS4)(cp[0]&0x7)<<18)
				| ((UCS4)(cp[1]&0x3F)<<12)
				| ((UCS4)(cp[2]&0x3F)<<6)
				| (cp[3]&0x3F);
			cp += 4;
			break;
		}
#if defined(UTF8_SIX_BYTE)	// We don't support six-byte UTF-8 by default, for efficiency reasons
		assert(!UTF8Is1st(cp[4]));
		if (UTF8Is1st(cp[4]))
			break;
		if ((*cp&0x0C) == 0x80)
		{					// Five UTF8 bytes
			ch = ((UCS4)(cp[0]&0x3)<<24)
				| ((UCS4)(cp[1]&0x7)<<18)
				| ((UCS4)(cp[2]&0x3F)<<12)
				| ((UCS4)(cp[3]&0x3F)<<6)
				| (cp[4]&0x3F);
			cp += 5;
		}
		else
		{					// Six UTF8 bytes
			assert(!UTF8Is1st(cp[5]));
			if (UTF8Is1st(cp[5]))
				break;
			ch = ((UCS4)(cp[0]&0x1)<<30)
				| ((UCS4)(cp[1]&0x3)<<24)
				| ((UCS4)(cp[2]&0x7)<<18)
				| ((UCS4)(cp[3]&0x3F)<<12)
				| ((UCS4)(cp[4]&0x3F)<<6)
				| (cp[5]&0x3F);
			cp += 6;
		}
#endif
	}

	return ch;
}

inline int
UTF8Len(const UTF8* cp)
{
	switch (*cp&0xF0)
	{
	case 0x00: case 0x10: case 0x20: case 0x30:
	case 0x40: case 0x50: case 0x60: case 0x70:	// One UTF8 byte
		return 1;

	case 0x80: case 0x90: case 0xA0: case 0xB0:	// Illegal, not first byte
		goto illegal_utf8;			// Not legal first UTF-8 byte

	case 0xC0: case 0xD0:				// Two UTF8 bytes
		if (UTF8Is1st(cp[1]))
			goto illegal_utf8;		// Illegal trailing UTF-8 byte after 2-byte prefix
		return 2;

	case 0xE0:					// Three UTF8 bytes
		if (UTF8Is1st(cp[1]) || UTF8Is1st(cp[2]))
			goto illegal_utf8;		// Illegal trailing UTF-8 byte after 3-byte prefix
		return 3;

	case 0xF0:					// Four UTF8 bytes
		if (UTF8Is1st(cp[1]) || UTF8Is1st(cp[2]) || UTF8Is1st(cp[3]))
			goto illegal_utf8;		// Illegal trailing UTF-8 byte after 4-byte prefix
		if ((cp[0] & 0x0C) == 0)
			return 4;
#if defined(UTF8_SIX_BYTE)	// We don't support six-byte UTF-8 by default, for efficiency reasons
		if (UTF8Is1st(cp[4]))
			goto illegal_utf8;		// Illegal trailing UTF-8 byte after 5-byte prefix
		if ((*cp&0x0C) == 0x80)
		{					// Five UTF8 bytes
			return 5;
		}
		else
		{					// Six UTF8 bytes
			if (UTF8Is1st(cp[5]))
				goto illegal_utf8;	// Illegal trailing UTF-8 byte after 6-byte prefix
			return 6;
		}
#else
		if ((cp[0] & 0x80) != 0)
			goto illegal_utf8;		// UTF-8 is only accepted up to 4 bytes
#endif

	illegal_utf8:
		assert(!"Illegal or unsupported UTF-8 sequence");
		return 0;

	default:					// Cannot happen, but keep the compiler happy
		return 0;
	}
}

inline const UTF8*
UTF8Backup(const UTF8* cp, const UTF8* limit = 0)
{
	if (limit == 0)
		limit = cp-4;	// No limit
	if (cp-1 >= limit && UTF8Is1st(cp[-1]))
		return cp-1;
	if (cp-2 >= limit && UTF8Is1st(cp[-2]))
		return cp-2;
	if (cp-3 >= limit && UTF8Is1st(cp[-3]))
		return cp-3;
	if (cp-4 >= limit && UTF8Is1st(cp[-4]))
		return cp-4;
#if defined(UTF8_SIX_BYTE)	// We don't support six-byte UTF-8 by default, for efficiency reasons
	if (cp-5 >= limit && UTF8Is1st(cp[-5]))
		return cp-5;
	if (cp-6 >= limit && UTF8Is1st(cp[-6]))
		return cp-6;
#endif
	return (UTF8*)0;
}

inline void
UTF8PutPaddedZero(UTF8*& cp, int length)
{
#if defined(UTF8_SIX_BYTE)	// We don't support six-byte UTF-8 by default, for efficiency reasons
	assert(length >= 0 && length <= 6);
#else
	assert(length >= 0 && length <= 4);
#endif
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
		*cp++ = (UTF8)ch;
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

#if defined(UTF8_SIX_BYTE)	// We don't support six-byte UTF-8 by default, for efficiency reasons
	case 5:		// 2 data bits in 1st byte, 6 in each of 4 more
		*cp++ = 0xF8 | (UTF8)((ch >> 24) & 0x03);
		*cp++ = 0x80 | (UTF8)((ch >> 18) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >> 12) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  6) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
		return;

	case 6:		// 1 data bit in 1st byte, 6 in each of 5 more
		*cp++ = 0xFC | (UTF8)((ch >> 30) & 0x01);
		*cp++ = 0x80 | (UTF8)((ch >> 24) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >> 18) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >> 12) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  6) & 0x3F);
		*cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
		return;
#endif
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
