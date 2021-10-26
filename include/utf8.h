/*
 * UTF8 encode/decode.
 *
 * A UTF8 character is represented as 1-4 bytes.
 * A first byte with a most significant bit of zero is a single ASCII byte.
 * The bytes after the first always have most significant two bits == "10",
 * which never occurs in the first byte - thus it's possible to jump into
 * the middle of a UTF8 string and reliably find the start of a character.
 */
#include	<assert.h>

typedef char		UTF8;
typedef	uint32_t	UCS4;		// A UCS4 character, aka Rune
#define	UCS4_NONE	0xFFFFFFFF	// Marker indicating no UCS4 character
#define	UCS4_NO_GLYPH	"\xEF\xBF\xBD"	// UTF8 character used to display where the correct glyph is not available

inline bool
UTF8Is1st(UTF8 ch)
{
	return (ch & 0xC0) != 0x80;
}

// Get length of UTF8 from UCS4
inline int
UTF8Len(UCS4 ch)
{
	if (ch <= 0x0000007F)   // 7 bits:
		return 1;       // ASCII
	if (ch <= 0x000007FF)   // 11 bits:
		return 2;       // two bytes
	if (ch <= 0x0000FFFF)   // 16 bits
		return 3;       // three bytes
	if (ch <= 0x001FFFFF)   // 21 bits
		return 4;       // four bytes
	assert(ch <= 0x03FFFFFF);
#if 0
	if (ch <= 0x03FFFFFF)   // 26 bits
		return 5;       // five bytes
	if (ch <= 0x7FFFFFFF)   // 31 bits
		return 6;       // six bytes
	assert(ch <= 0x7FFFFFFF); // max UTF-8 value
#endif
	return 0;               // safety.
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
		if (UTF8Is1st(cp[1]) || UTF8Is1st(cp[2]) || UTF8Is1st(cp[3])
		 || (cp[0] & 0x08) != 0)		// Else trying for more than four bytes
			break;
		ch = ((UCS4)(cp[0]&0x7)<<18)
			| ((UCS4)(cp[1]&0x3F)<<12)
			| ((UCS4)(cp[2]&0x3F)<<6)
			| (cp[3]&0x3F);
		cp += 4;
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
		assert(!"UTF-8 starts with trailing byte");
		return 0;

	case 0xC0: case 0xD0:				// Two UTF8 bytes
		if (UTF8Is1st(cp[1]))
		{
			assert(!"Illegal trailing UTF-8 byte");
			return 0;
		}
		return 2;

	case 0xE0:					// Three UTF8 bytes
		if (UTF8Is1st(cp[1]) || UTF8Is1st(cp[2]))
		{
			assert(!"Illegal trailing UTF-8 byte");
			return 0;
		}
		return 3;

	case 0xF0:					// Four UTF8 bytes
		if (UTF8Is1st(cp[1]) || UTF8Is1st(cp[2]) || UTF8Is1st(cp[3]) || (cp[0] & 0x40))
		{
			assert(!"Illegal 4-byte UTF-8");
			return 0;
		}
		return 4;

	default:
		return 0;	// Cannot happen
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
	return (UTF8*)0;
}

// Store UTF8 from UCS4
inline void
UTF8Put(UTF8*& cp, UCS4 ch)
{
        switch (UTF8Len(ch))
        {
        default:
                return;         // Error caught in UTF8Len()

        case 1:                 // Single byte
                *cp++ = (UTF8)ch;
                return;

        case 2:         // 5 data bits in 1st byte, 6 in next
                *cp++ = 0xC0 | (UTF8)((ch >>  6) & 0x1F);
                *cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
                return;

        case 3:         // 4 data bits in 1st byte, 6 in each of 2 more
                *cp++ = 0xE0 | (UTF8)((ch >> 12) & 0x0F);
                *cp++ = 0x80 | (UTF8)((ch >>  6) & 0x3F);
                *cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
                return;

        case 4:         // 3 data bits in 1st byte, 6 in each of 3 more
                *cp++ = 0xF0 | (UTF8)((ch >> 18) & 0x07);
                *cp++ = 0x80 | (UTF8)((ch >> 12) & 0x3F);
                *cp++ = 0x80 | (UTF8)((ch >>  6) & 0x3F);
                *cp++ = 0x80 | (UTF8)((ch >>  0) & 0x3F);
                return;
	}
}
