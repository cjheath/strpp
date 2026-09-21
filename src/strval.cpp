/*
 * Parts of StrVal that are better non inlined.
 *
 * The explicit instantiation below is what puts the symbol in the library
 * for every other translation unit to call.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<strval.h>
#include	<str_msg.h>

template<typename Index>
int32_t StrValI<Index>::asInt32(
	ErrNum*	err_return,	// error return
	int	radix,		// base for conversion
	Index*	scanned		// characters scanned
) const
{
	Index		len = length();		// length of string
	Index		i = 0;			// position of next character
	Index		end_of_digits = 0;	// where the digits ended, for a report
	UCS4		ch = 0;			// current character
	int		d;			// current digit value
	bool		negative = false;	// Was a '-' sign seen?
	unsigned long	l = 0;			// Number being converted
	unsigned long	last;
	unsigned long	max;

	if (err_return)
		*err_return = 0;

	// Check legal radix
	if (radix < 0 || radix > 36)
	{
		ErrNum	e = ErrorSTR_IllegalRadix(radix, *this);
		if (err_return)
			*err_return = e;
		if (scanned)
			*scanned = 0;
		return 0;
	}

	// Skip leading white-space
	while (i < len && UCS4IsWhite(ch = (*this)[i]))
		i++;
	if (i == len)
		goto no_digits;

	// Check for sign character
	if (ch == '+' || ch == '-')
	{
		i++;
		negative = ch == '-';
		while (i < len && UCS4IsWhite(ch = (*this)[i]))
			i++;
		if (i == len)
			goto no_digits;
	}

	if (radix == 0)		// Auto-detect radix (octal, decimal, binary)
	{
		if (UCS4Digit(ch) == 0 && i+1 < len)
		{
			// ch is the digit zero, look ahead
			switch ((*this)[i+1])
			{
			case 'b': case 'B':
				if (radix == 0 || radix == 2)
				{
					radix = 2;
					ch = (*this)[i += 2];
					if (i == len)
						goto no_digits;
				}
				break;
			case 'x': case 'X':
				if (radix == 0 || radix == 16)
				{
					radix = 16;
					ch = (*this)[i += 2];
					if (i == len)
						goto no_digits;
				}
				break;
			default:
				if (radix == 0)
					radix = 8;
				break;
			}
		}
		else
			radix = 10;
	}

	// Check there's at least one digit:
	if ((d = Digit(ch, radix)) < 0)
		goto not_number;

	max = (ULONG_MAX-1)/radix + 1;
	// Convert digits
	do {
		i++;			// We're definitely using this char
		last = l;
		if (l > max		// Detect *unsigned* long overflow
		 || (l = l*radix + d) < last)
		{
			// Overflowed unsigned long!
			ErrNum	e = ErrorSTR_NumberOverflow(*this, radix, i);
			if (err_return)
				*err_return = e;
			if (scanned)
				*scanned = i;
			return 0;
		}
	} while (i < len && (d = Digit((*this)[i], radix)) >= 0);

	end_of_digits = i;

	if (err_return)
		*err_return = 0;

	// Check for trailing non-white characters
	while (i < len && UCS4IsWhite((*this)[i]))
		i++;
	if (i != len)
	{
		ErrNum	e = ErrorSTR_TrailText(*this, radix, end_of_digits, substr(i));
		if (err_return)
			*err_return = e;
	}

	// Return number of digits scanned
	if (scanned)
		*scanned = i;

	/*
	 * The answer is an int32_t, so the bound is that of an int32_t and not of
	 * a long. The two were the same width where this was written, and on a
	 * 64-bit target they are not: a long's bound lets four billion through to
	 * be wrapped into a negative number. INT32_MIN has no positive
	 * counterpart, so a negative number is allowed one more than a positive.
	 */
	if (l > (unsigned long)INT32_MAX+(negative ? 1 : 0))
	{
		ErrNum	e = ErrorSTR_NumberOverflow(*this, radix, end_of_digits);
		if (err_return)
			*err_return = e;
		// The low word is answered anyway, as a number with trailing text is:
		// a caller reading a 32-bit bit pattern wants it, and the error is
		// what says these digits do not spell the number it is.
	}

	/*
	 * Casting unsigned long down to long doesn't clear the high bit
	 * on a twos-complement architecture:
	 */
	return negative ? -(long)l : (long)l;

no_digits:
	{
		ErrNum	e = ErrorSTR_NoDigits(*this, radix);
		if (err_return)
			*err_return = e;
	}
	if (scanned)
		*scanned = i;
	return 0;

not_number:
	{
		ErrNum	e = ErrorSTR_NotNumber(*this, radix, StrVal(ch), i);
		if (err_return)
			*err_return = e;
	}
	if (scanned)
		*scanned = i;
	return 0;
}

// The definition above, for the index width this library is built with, so
// that every other translation unit can call it
template class StrValI<StrValIndex>;
