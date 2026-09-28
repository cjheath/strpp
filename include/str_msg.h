#if	!defined(STR_MSG_H)
#define	STR_MSG_H
/*
 * strpp's error reporting functions: one per message, gathering the parameters
 * its default text calls for and handing them to the error buffer's Error().
 * This is the private half of the pair, for code that raises a message;
 * str_err.h holds the numbers, and is for code that recognises a condition.
 *
 * Each function returns the ErrNum it has just reported, so reporting an error
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
ErrorSTR_Assert(const char* file, int line, const char* condition)
{
	return Error(STRERR_ASSERT,
		"At {1}:{2}, assertion failed: `{3}`",
		VariantArray() << file << line << condition);
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
ErrorSTR_NumberOverflow(StrVal text, int radix, StrValIndex offset, const char* type_name)
{
	return Error(STRERR_NUMBER_OVERFLOW,
		"The number in `{1}` is too large to be read as `{4}` in radix {2}, overflowing at {3}",
		VariantArray() << text << radix << offset << type_name);
}

inline ErrNum
ErrorSTR_NotNumber(StrVal text, int radix, StrVal character, StrValIndex offset)
{
	return Error(STRERR_NOT_NUMBER,
		"`{1}` is not a number in radix {2}: the character `{3}` at {4} is not a digit",
		VariantArray() << text << radix << character << offset);
}

/*
 * A number with a minus sign, read into a type that has no sign: the sign has
 * no meaning there, so the number cannot be read at all. A bit pattern is asked
 * for by reading it in a base - "0xffffffff" and not "-1".
 */
inline ErrNum
ErrorSTR_NegativeUnsigned(StrVal text, int radix)
{
	return Error(STRERR_NEGATIVE_UNSIGNED,
		"The number in `{1}` is negative, and cannot be read into an unsigned type in radix {2}",
		VariantArray() << text << radix);
}

inline ErrNum
ErrorSTR_IllegalRadix(int radix, StrVal text)
{
	return Error(STRERR_ILLEGAL_RADIX,
		"The radix {1} is not one a number can be read in, so `{2}` was not read",
		VariantArray() << radix << text);
}

/*
 * An Array asked for an element it has not got: an index past the end, or a
 * length that runs off it. `operation` is the plain verb that names what was
 * wanted - "remove", "drop", "take", "slice" - so one message serves them all.
 */
inline ErrNum
ErrorSTR_IndexOutOfRange(size_t index, size_t length, const char* operation)
{
	return Error(STRERR_INDEX_OUT_OF_RANGE,
		"An index of {1} is outside an array of {2} elements, so there is nothing to {3}",
		VariantArray() << (unsigned long long)index << (unsigned long long)length << operation);
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

inline ErrNum
ErrorVAR_NoCoercion(const char* from, const char* to)
{
	return Error(VARERR_NO_COERCION,
		"A `{1}` cannot be converted to a `{2}`: that conversion is not implemented, so the value is left as it is",
		VariantArray() << from << to);
}

// The times and dates:

inline ErrNum
ErrorTIM_InvalidYMDHMS(int year, int month, int day, int hour, int minute, int second)
{
	return Error(TIMERR_INVALID_YMDHMS,
		"The date and time {1}-{2}-{3} {4}:{5}:{6} is not a date that exists",
		VariantArray() << year << month << day << hour << minute << second);
}

inline ErrNum
ErrorTIM_InvalidText(StrVal text)
{
	return Error(TIMERR_INVALID_TEXT,
		"`{1}` is not a time, a date or an interval this library can read",
		VariantArray() << text);
}

inline ErrNum
ErrorTIM_OutOfRange(StrVal when, StrVal first, StrVal last)
{
	return Error(TIMERR_OUT_OF_RANGE,
		"The date {1} is outside the range a DateTime can hold, which runs from {2} to {3}",
		VariantArray() << when << first << last);
}

inline ErrNum
ErrorTIM_NoDate()
{
	return Error(TIMERR_NO_DATE,
		"This value is a time of day with no date, so it does not name an instant",
		VariantArray());
}

inline ErrNum
ErrorTIM_NoClock()
{
	return Error(TIMERR_NO_CLOCK,
		"The current time is not known: this target has no clock, or reading it failed",
		VariantArray());
}

/*
 * A time whose value is null, used where a value was needed: adding it, or
 * reading it as a number of seconds or as a date. Reported rather than
 * asserted, because a null is data - it comes from a field that was never
 * filled in, or a record that says "no time"- and not a fault in the program.
 *
 * The operation is named by a gerund, as it is in the message below, so that a
 * call site needs one word and not two.
 */
inline ErrNum
ErrorTIM_NullValue(const char* type, const char* operation)
{
	return Error(TIMERR_NULL_VALUE,
		"This `{1}` is null, so there is no value for {2}",
		VariantArray() << type << operation);
}

/*
 * An operation whose result will not fit: a count of ticks that ran past the
 * end of what a 64-bit count holds, or a date that ran past what the fields
 * hold. The result is a null or a value with no date - never a wrapped
 * number that looks like a time, which is what this reports instead of.
 */
inline ErrNum
ErrorTIM_ResultOverflow(const char* type, const char* operation)
{
	return Error(TIMERR_RESULT_OVERFLOW,
		"The result of {2} is past the range of `{1}`",
		VariantArray() << type << operation);
}

/*
 * The host could not say how far its civil time is from UTC at that instant -
 * its zone rules do not reach the date, or its clock failed. UTC is answered,
 * and said out loud, since an offset cannot be null.
 */
inline ErrNum
ErrorTIM_NoZone()
{
	return Error(TIMERR_NO_ZONE,
		"The host's zone offset is not known at that instant, so UTC is returned",
		VariantArray());
}

// The threads, the locks and the waits:

/*
 * A thread that the host would not create. It is not running, it is not in the
 * registry, and `join` on it is a mistake - so the caller has to know, which is
 * what this is for. `call` is the host call's own name and `code` what it
 * returned, an errno-style code or the platform's own.
 */
inline ErrNum
ErrorTHR_CreateFailed(const char* call, int code)
{
	return Error(THRERR_CREATE_FAILED,
		"The thread was not created: `{1}` returned {2}, so it is not running",
		VariantArray() << call << code);
}

/*
 * The thread was created, but not with the stack the caller asked for: the
 * platform refused the size, so the host's default applies.
 */
inline ErrNum
ErrorTHR_StackRefused(unsigned long long bytes, const char* call, int code)
{
	return Error(THRERR_STACK_REFUSED,
		"The host refused a stack of {1} bytes: `{2}` returned {3}, so the thread runs with the default",
		VariantArray() << bytes << call << code);
}

/*
 * Waiting for a thread failed, so whether it has ended - and its exit code -
 * are not known. Its exit code is returned as 0, which a thread that ended with
 * 0 also returns: this is what tells the two apart.
 */
inline ErrNum
ErrorTHR_JoinFailed(const char* call, int code)
{
	return Error(THRERR_JOIN_FAILED,
		"The thread could not be waited for: `{1}` returned {2}, so its exit code is not known",
		VariantArray() << call << code);
}

/*
 * A condition variable whose underlying primitive was never made - a failed
 * pthread_cond_init, xEventGroupCreate or CreateEvent. Nothing waits on it, and
 * every wait or signal is a no-op. `operation` is the verb: "wait on",
 * "signal", "broadcast".
 */
inline ErrNum
ErrorTHR_NoCondition(const char* operation)
{
	return Error(THRERR_NO_CONDITION,
		"This condition variable was not created, so there is nothing to {1}",
		VariantArray() << operation);
}

/*
 * A wait that the host refused, or that failed while it was happening. The wait
 * is over, and is treated as having timed out.
 */
inline ErrNum
ErrorTHR_WaitFailed(const char* call, int code)
{
	return Error(THRERR_WAIT_FAILED,
		"The wait failed: `{1}` returned {2}",
		VariantArray() << call << code);
}

/*
 * A delay that did not happen: the host refused the duration, so the thread
 * carried on without waiting.
 */
inline ErrNum
ErrorTHR_DelayFailed(const char* call, int code)
{
	return Error(THRERR_DELAY_FAILED,
		"The delay was not performed: `{1}` returned {2}",
		VariantArray() << call << code);
}

#endif	// STR_MSG_H
