/*
 * Encode/decode characters between UTF-8, Latin1, UTF-16 and UCS4.
 *
 * UCS4 (aka UTF-32, Rune) functions
 *
 * UCS4 is the ISO/IEC 10646 32-bit character encoding.
 * Truncation of 16 zero bits yields UTF-16
 * Truncation of 24 zero bits yields Latin-1 (ISO-8859-1)
 * Truncation of 25 zero bits yields ASCII
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<assert.h>

#include	<atomic>
#include	<char_encoding.h>
#include	"case_conversions.c"

bool
UCS4IsAlphabetic(UCS4 ch)
{
	/* Simplistic first cut: if we can convert case, it's alphabetic */
	if (UCS4ToUpper(ch) != ch || UCS4ToLower(ch) != ch)
		return true;

	/* Sigh, there are a couple of hundred non-case convertable letters */
	int	hi, lo, mid;

	lo = 0;
	hi = UCS4NumNoCase-1;
	while (hi >= lo)
	{
		mid = (lo+hi)/2;
		if (ch < UnicodeNoCaseConvLetters[mid])
			hi = mid-1;
		else if (ch > UnicodeNoCaseConvLetters[mid])
			lo = mid+1;
		else
			return true;
	}

	return false;
}

bool
UCS4IsIdeographic(UCS4 ch)
{
	return (ch >= 0x3006 && ch <= 0x3007)
	    || (ch >= 0x3021 && ch <= 0x3029)	/* HANGZHOU numerals */
	    || (ch >= 0x3038 && ch <= 0x303A)
	    || (ch >= 0x3400 && ch <= 0x4DB5)	/* CJK Ideographs Extension A */
	    || (ch >= 0x4E00 && ch <= 0x9FA5)	/* CJK Ideographs */
	    || (ch >= 0xF900 && ch <= 0xFA2D);
}

/*
 * Unicode has multiple ranges of digits from 0 to 9 (or in one case, 1 to 9).
 * In this sorted table, low is the code for 0 or 1, and high is the code for 9.
 */
static const struct digit_range
{
	unsigned short	low;
	unsigned short	high;
} UCS4_DigitRanges[] =
{				// must keep sorted:
	{ 0x0030, 0x0039 }, 
	{ 0x0660, 0x0669 }, 
	{ 0x06F0, 0x06F9 }, 
	{ 0x0966, 0x096F }, 
	{ 0x09E6, 0x09EF }, 
	{ 0x0A66, 0x0A6F }, 
	{ 0x0AE6, 0x0AEF }, 
	{ 0x0B66, 0x0B6F }, 
	{ 0x0BE7, 0x0BEF }, 
	{ 0x0C66, 0x0C6F }, 
	{ 0x0CE6, 0x0CEF }, 
	{ 0x0D66, 0x0D6F }, 
	{ 0x0E50, 0x0E59 }, 
	{ 0x0ED0, 0x0ED9 }, 
	{ 0x0F20, 0x0F29 }, 
	{ 0x1040, 0x1049 }, 
	{ 0x1369, 0x1371 }, 
	{ 0x17E0, 0x17E9 }, 
	{ 0x1810, 0x1819 }, 
	{ 0xFF10, 0xFF19 }
};
#define	UCS4NumDigitRanges	(sizeof(UCS4_DigitRanges)/sizeof(struct digit_range))

bool
UCS4IsDecimal(UCS4 ch)
{
	return UCS4Digit(ch) != -1;
}

// Digit value 0-9, -1 if not digit
int
UCS4Digit(UCS4 ch)
{
	// Adjacent digits will often be from the same set. Memoize that set.
	// This memo is thread- and SMP-safe as long as we read it only once:
	static	std::atomic<int> last_memo;
	int	last = last_memo;

	const struct digit_range* rp = UCS4_DigitRanges+last;
	if (ch >= rp->low && ch <= rp->high)
		return 9-(rp->high - ch);

	for (		// Not worth setting up binary search
		rp = UCS4_DigitRanges;
		rp < UCS4_DigitRanges+UCS4NumDigitRanges;
		rp++
	)
	{
		if (ch < rp->low)
			return -1;		/* Short circuit */
		if (ch <= rp->high)
		{
			last_memo = rp-UCS4_DigitRanges;
			return 9-(rp->high - ch);
		}
	}
	/* Special cases that didn't suit the table: */
	if (ch == 0x3007)
		return 0;			/* Ideographic number zero */
	if (ch >= 0x3021 && ch <= 0x3029)	/* HANGZHOU numerals */
		return ch-0x3020;
	return -1;
}

/*
 * Convert to upper case
 */
UCS4
UCS4ToUpper(UCS4 ch)
{
	// Adjacent characters will often be from the same set. Memoize that set.
	// This memo is thread- and SMP-safe as long as we read it only once:
	static	std::atomic<int> last_memo;
	int	last = last_memo;

	if (ch >= UnicodeToUpper[last].firstchar
	 && ch <= UnicodeToUpper[last].lastchar)
		return ch + UnicodeToUpper[last].delta;

	int	hi, lo, mid;
	lo = 0;
	hi = UCS4NumToUpperChars-1;
	while (hi >= lo)
	{
		mid = (lo+hi)/2;
		if (ch < UnicodeToUpper[mid].firstchar)
			hi = mid-1;
		else if (ch > UnicodeToUpper[mid].firstchar)
			lo = mid+1;
		else
		{
			last_memo = mid;
			return ch + UnicodeToUpper[mid].delta;
		}
	}

	if (hi >= 0
	 && ch >= UnicodeToUpper[hi].firstchar
	 && ch <= UnicodeToUpper[hi].lastchar)
	{
		last_memo = hi;
	 	return ch + UnicodeToUpper[hi].delta;
	}

	return ch;
}

/*
 * Convert to lower case
 */
UCS4
UCS4ToLower(UCS4 ch)
{
	// Adjacent characters will often be from the same set. Memoize that set.
	// This memo is thread- and SMP-safe as long as we read it only once:
	static	std::atomic<int> last_memo;
	int	last = last_memo;

	if (ch >= UnicodeToLower[last].firstchar
	 && ch <= UnicodeToLower[last].lastchar)
	 	return ch + UnicodeToLower[last].delta;

	int	hi, lo, mid;
	lo = 0;
	hi = UCS4NumToLowerChars-1;
	while (hi >= lo)
	{
		mid = (lo+hi)/2;
		if (ch < UnicodeToLower[mid].firstchar)
			hi = mid-1;
		else if (ch > UnicodeToLower[mid].firstchar)
			lo = mid+1;
		else
		{
			last_memo = mid;
			return ch + UnicodeToLower[mid].delta;
		}
	}

	if (hi >= 0
	 && ch >= UnicodeToLower[hi].firstchar
	 && ch <= UnicodeToLower[hi].lastchar)
	{
		last_memo = hi;
	 	return ch + UnicodeToLower[hi].delta;
	}

	return ch;
}

/*
 * Convert to title case
 */
UCS4
UCS4ToTitle(UCS4 ch)
{
	// Special check for the very few special title-case characters:
	for (unsigned i = 0; i < UCS4NumTitleChars; i++)
		if (ch == UnicodeToTitle[i].ch)
			return ch+UnicodeToTitle[i].delta;

	// Otherwise just use upper-case:
	return UCS4ToUpper(ch);
}
