/*
 * The one part of StrVal that is better not inlined: the report a number reader
 * makes, which cannot be made from the header, because reporting needs the
 * message set and hence a VariantArray.
 *
 * The explicit instantiation below is what puts the symbol in the library for
 * every other translation unit to call.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<strval.h>
#include	<str_msg.h>

/*
 * What a reader could not read, said out loud. One function serves every reader
 * and every width: the message set has one text per condition, and the type's
 * name is a parameter, so another width needs no new message and a new reader
 * needs no new number.
 *
 * `at` is where the trouble is, for the message that names a character, and
 * `stop` is where the number stopped being read, for the two that name a
 * position. A condition that names neither passes either.
 */
template<typename Index>
ErrNum StrValI<Index>::reportNumber(
	ErrNum			why,
	const char*		type_name,
	const StrValI<Index>&	text,
	int			radix,
	Index			at,
	Index			stop
)
{
	Index	len = text.length();
	Index	after = stop;		// Where the text after the number starts

	while (after < len && UCS4IsWhite(text[after]))
		after++;

	switch (why)
	{
	case STRERR_ILLEGAL_RADIX:
		return ErrorSTR_IllegalRadix(radix, text);

	case STRERR_NO_DIGITS:
		return ErrorSTR_NoDigits(text, radix);

	case STRERR_NOT_NUMBER:
		return ErrorSTR_NotNumber(text, radix, StrVal(text[at]), at);

	case STRERR_NEGATIVE_UNSIGNED:
		return ErrorSTR_NegativeUnsigned(text, radix);

	case STRERR_NUMBER_OVERFLOW:
		return ErrorSTR_NumberOverflow(text, radix, stop, type_name);

	default:
		return ErrorSTR_TrailText(text, radix, stop, text.substr(after));
	}
}

// The definition above, for the index width this library is built with, so
// that every other translation unit can call it
template class StrValI<StrValIndex>;
