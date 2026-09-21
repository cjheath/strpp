#if	!defined(STR_MSG_H)
#define	STR_MSG_H
/*
 * strpp's error reporting functions: one per message, gathering the parameters
 * its default text calls for and handing them to the error buffer's Error().
 * This is the private half of the pair, for code that raises a message;
 * str_err.h holds the numbers, and is for code that recognises a condition.
 *
 * Each function answers the ErrNum it has just reported, so reporting an error
 * and returning it are one act, and the ordinary use is a return:
 *
 *	return ErrorSTR_NoDigits(text, radix);
 *
 * Nothing is formatted here, and nothing is decided about language, style or
 * severity: the buffer holds the number, the default text and the parameters
 * until they are read out, which may be in another thread or another process.
 *
 * The parameters are gathered into a VariantArray built in place, so a report
 * cannot be disturbed by another being made while its own parameters are still
 * being gathered. Variant's constructors from StrVal, int, long, long long and
 * const char* are not explicit, so most parameters need no Variant(...) cast.
 *
 * A *header* cannot report through these, and neither can anything it includes:
 * Error() needs a VariantArray, which needs variant.h, which needs strval.h, so
 * strval.h cannot include this file without a cycle. That is why
 * StrVal::asInt32, the one member of the bottom header that has to report, is
 * defined out of line in src/strval.cpp. Anything else that comes to report
 * from a header needs the same treatment.
 */
#include	<str_err.h>
#include	<errbuf.h>
#include	<strval.h>
#include	<variant.h>

// The assertion, whose report is the one made on the way out:

inline ErrNum
ErrorSTR_Assert(const char* condition, const char* file, int line)
{
	return Error(STRERR_ASSERT,
		"Assertion failed: `{1}` at {2}:{3}",
		VariantArray() << condition << file << line);
}

// Reading a number out of a text:

inline ErrNum
ErrorSTR_TrailText(StrVal text, int radix, StrValIndex offset, StrVal trailing)
{
	return Error(STRERR_TRAIL_TEXT,
		"Reading `{1}` in radix {2}: the number ends at {3} and `{4}` is not part of it",
		VariantArray() << text << radix << offset << trailing);
}

inline ErrNum
ErrorSTR_NoDigits(StrVal text, int radix)
{
	return Error(STRERR_NO_DIGITS,
		"There are no digits in `{1}` to read a number from, in radix {2}",
		VariantArray() << text << radix);
}

inline ErrNum
ErrorSTR_NumberOverflow(StrVal text, int radix, StrValIndex offset)
{
	return Error(STRERR_NUMBER_OVERFLOW,
		"The number in `{1}` is too large to be read as an `int32_t` in radix {2}, overflowing at {3}",
		VariantArray() << text << radix << offset);
}

inline ErrNum
ErrorSTR_NotNumber(StrVal text, int radix, StrVal character, StrValIndex offset)
{
	return Error(STRERR_NOT_NUMBER,
		"`{1}` is not a number in radix {2}: the character `{3}` at {4} is not a digit",
		VariantArray() << text << radix << character << offset);
}

inline ErrNum
ErrorSTR_IllegalRadix(int radix, StrVal text)
{
	return Error(STRERR_ILLEGAL_RADIX,
		"The radix {1} is not one a number can be read in, so `{2}` was not read",
		VariantArray() << radix << text);
}

// A Variant's type, and a conversion that would lose the value:

inline ErrNum
ErrorVAR_WrongType(const char* wanted, const char* held)
{
	return Error(VARERR_WRONG_TYPE,
		"A `{1}` was expected, but this Variant is a `{2}`",
		VariantArray() << wanted << held);
}

inline ErrNum
ErrorVAR_DoesNotFit(const char* wanted, StrVal value)
{
	return Error(VARERR_DOES_NOT_FIT,
		"Cannot convert to a `{1}` because the value {2} does not fit",
		VariantArray() << wanted << value);
}

#endif	// STR_MSG_H
