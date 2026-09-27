#if	!defined(STR_ERR_H)
#define	STR_ERR_H
/*
 * strpp's error numbers: one per message in the set, with the message's default
 * text in a comment. This is the public half of the pair, for code that has to
 * recognise a condition; str_msg.h holds the function that reports each of
 * these, and is what code that raises one includes.
 *
 * One library, one message source file, however many sets it holds: strpp has
 * three, and all are here. Set 1 is STR, the strings and what can go wrong in
 * reading one from a text; set 2 is VAR, the Variant type; set 3 is TIM, the
 * time and date layer. They are written by hand and shaped as a generator would
 * emit them.
 *
 * A number holds a set number and a message number within that set. A number,
 * once used, is never re-used or re-numbered: it appears in logs, in the
 * product manual and in a customer's report, and it has to mean the same thing
 * years later. Neither set has been released, so both may still be renumbered;
 * once one is, no number in it changes again.
 *
 * A message's default text names its parameters by position, {1} being the
 * first, because a translation may use them in another order or leave one out.
 * No message here has a source position - nothing in this library parses a
 * file - so a message about a text carries the text itself, and how far into it
 * the trouble was, as parameters of its own.
 */
#include	<error.h>

#define	STRERR_SET			1	// The message set allocated to the strings
#define	VARERR_SET			2	// The message set allocated to the Variant
#define	TIMERR_SET			3	// The message set allocated to the times and dates

// The assertion, whose report is the one made on the way out:

#define	STRERR_ASSERT			ErrNum(STRERR_SET, 1)	// Assertion failed: `{1}` at {2}:{3}

// Reading a number out of a text:

#define	STRERR_TRAIL_TEXT		ErrNum(STRERR_SET, 2)	// Reading `{1}` in radix {2}: the number ends at {3} and `{4}` is not part of it
#define	STRERR_NO_DIGITS		ErrNum(STRERR_SET, 3)	// There are no digits in `{1}` to read a number from, in radix {2}
#define	STRERR_NUMBER_OVERFLOW		ErrNum(STRERR_SET, 4)	// The number in `{1}` is too large to be read as `{4}` in radix {2}, overflowing at {3}; {4} being the width that was asked for. An overflow, and not the trailing text that a lost precision or an unconsumed character gives: the number is not this number, so a caller should not carry on with it
#define	STRERR_NOT_NUMBER		ErrNum(STRERR_SET, 5)	// `{1}` is not a number in radix {2}: the character `{3}` at {4} is not a digit
#define	STRERR_ILLEGAL_RADIX		ErrNum(STRERR_SET, 6)	// The radix {1} is not one a number can be read in, so `{2}` was not read
#define	STRERR_NEGATIVE_UNSIGNED	ErrNum(STRERR_SET, 7)	// The number in `{1}` is negative, and cannot be read into an unsigned type in radix {2}

// An Array asked for an element it has not got. The index named is the one that
// is out of range, or the first the request needed and did not have:

#define	STRERR_INDEX_OUT_OF_RANGE	ErrNum(STRERR_SET, 8)	// An index of {1} is outside an array of {2} elements, so there is nothing to {3}

// A Variant's type, and a conversion that would lose the value:

#define	VARERR_WRONG_TYPE		ErrNum(VARERR_SET, 1)	// A `{1}` was expected, but this Variant is a `{2}`
#define	VARERR_DOES_NOT_FIT		ErrNum(VARERR_SET, 2)	// Cannot convert to a `{1}` because the value {2} does not fit
#define	VARERR_NO_COERCION		ErrNum(VARERR_SET, 3)	// A `{1}` cannot be converted to a `{2}`: that conversion is not implemented, so the value is left as it is

// The times and dates:

#define	TIMERR_INVALID_YMDHMS		ErrNum(TIMERR_SET, 1)	// The date and time {1}-{2}-{3} {4}:{5}:{6} is not a date that exists
#define	TIMERR_INVALID_TEXT		ErrNum(TIMERR_SET, 2)	// `{1}` is not a time, a date or an interval this library can read
#define	TIMERR_OUT_OF_RANGE		ErrNum(TIMERR_SET, 3)	// The date {1} is outside the range a DateTime can hold, which runs from {2} to {3}
#define	TIMERR_NO_DATE			ErrNum(TIMERR_SET, 4)	// This value is a time of day with no date, so it does not name an instant
#define	TIMERR_NO_CLOCK			ErrNum(TIMERR_SET, 5)	// The current time is not known: this target has no clock, or reading it failed
#define	TIMERR_NULL_VALUE		ErrNum(TIMERR_SET, 6)	// This `{1}` is null, so there is no value for {2}
#define	TIMERR_RESULT_OVERFLOW		ErrNum(TIMERR_SET, 7)	// The result of {2} is past the range of `{1}`
#define	TIMERR_NO_ZONE			ErrNum(TIMERR_SET, 8)	// The host's zone offset is not known at that instant, so UTC is answered

#endif	// STR_ERR_H
