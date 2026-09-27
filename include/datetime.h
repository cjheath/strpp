#if	!defined(DATETIME_H)
#define	DATETIME_H
/*
 * Time and date: the interval, the instant, and the zone offset.
 *
 * Time comes in two kinds that are forever being confused with each other, so
 * they are two types here. An Interval is a length of time - how long something
 * takes; a DateTime is a point in time - when it happens. Both count the
 * same tick, so an interval adds to an instant with no conversion, and one
 * instant less another is an interval.
 *
 * The tick is 10^-8 seconds in a 64-bit signed integer: exactly the unit of a
 * Windows FILETIME, so one converts without rounding. A count of ticks spans
 * 5845 years, which is 2922 either side of the epoch. The epoch is
 * 2000-01-01T00:00:00Z, so that instant is tick 0, and the range runs from
 * 924 BC to 4922 AD.
 *
 * That range is a boundary and not a warning: a calendar date outside it is
 * refused and reported when it is asked to become a DateTime, never wrapped.
 * See gregorian.h for which dates can be named at all, and what year 0 means.
 *
 * A DateTime is counted from the epoch and knows no time zone. The zone is a
 * parameter of each operation that needs one, and is never stored, so there is
 * no local/UTC flag to fall out of step with the value it describes, and no
 * adjustment that can be applied twice or left applied by accident.
 *
 * Leap seconds are not represented: a minute here has 60 seconds, always, so
 * the count agrees with POSIX time and with every other count of seconds from
 * an epoch. A text naming second 60 is refused rather than quietly turned into
 * something else. The cost of that is stated in doc/datetime.md: the civil
 * readings of a tick are always right, and a difference between two of them is
 * a count of UTC seconds, which is not the physical time between them.
 *
 * A time can also be *null*, which is not a time at all: a value that was never
 * set. Null is the lowest tick, and a null used where a value is needed reports
 * rather than returning something that looks real. See NullTick below.
 *
 * The calendar - Y/M/D, h:m:s, ISO 8601 - is in gregorian.h, which is separate
 * because the two representations of one time, which an earlier library kept
 * in a single class with a cache between them, belong in two classes with no
 * cache at all.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdint>
#include	<time.h>

#include	<strval.h>
#include	<str_err.h>			// The error numbers this header returns
#include	<strassert.h>			// An offset that will not fit a short stops

class	Gregorian;
class	Milliseconds;
class	Seconds;

/*
 * The tick: 10^-8 seconds, which is the unit of a FILETIME and of nothing else
 * in particular. Everything time-shaped in this library counts these.
 */
typedef	int64_t		Tick;

static const Tick	TicksPerSecond	= 100000000;
static const Tick	TicksPerDay	= 8640000000000LL;	// 86400 * 10^8
static const Tick	EpochDay	= 730120;		// Days from 0/0/0000 to 2000-01-01

/*
 * The null tick: a Tick that means "no value at all", for a time that was
 * never set or is not known. It is the most negative tick, the one value with
 * no positive equivalent, since negating it would overflow - so no real time
 * can be turned into a null by being negated, and no arithmetic that negates a
 * value can invent one.
 *
 * It costs exactly one representable instant, the first tick of the range,
 * which is a ten-millionth of a second at 02:07:11 on the 25th of March, 924
 * BC. No clock reports it and no calendar needs it; see MinTicks below for
 * what the range becomes.
 *
 * A null is not zero and not the epoch: zero is a real instant. A value that
 * is null says so - isNull() - and reading one where a value is needed reports
 * TIMERR_NULL_VALUE rather than returning something that looks real.
 */
static const Tick	NullTick	= (Tick)-9223372036854775807LL - 1;

/*
 * How a time or a date is written as ISO 8601 text. The low nibble is how many
 * digits of a second to write, 0 to 8; the rest are flags. The macros this
 * replaces kept three bits for the digit count, which cannot hold the 8 that a
 * 10^-8 second tick needs.
 */
typedef enum {
	IsoFractionDigits	= 0x0F,	// Digits after the seconds, 0..8
	IsoPunctuate		= 0x10,	// Write the - and : of a punctuated form
	IsoDateOnly		= 0x20,	// The date, and not the time
	IsoSpaceForT		= 0x40,	// A space where ISO 8601 has its T
	IsoNoZone		= 0x80,	// Leave the zone designator off
} IsoFormat;

/*
 * Division that rounds towards minus infinity, and the modulus that goes with
 * it. A count of ticks before the epoch is negative, and a date is read out of
 * one by dividing it into days; with C's division, which truncates towards
 * zero, that lands on the wrong day for every count before the epoch, and off
 * by a day in the month and year arithmetic of a negative year. Every division
 * of a tick count in this library is one of these, and a count that is not
 * negative divides exactly as C's division already did, so only a date before
 * the epoch pays anything for it.
 */
inline Tick	floorDiv(Tick dividend, Tick divisor)
		{
			Tick	quotient = dividend/divisor;
			if (dividend%divisor != 0 && ((dividend < 0) != (divisor < 0)))
				quotient--;
			return quotient;
		}

inline Tick	floorMod(Tick dividend, Tick divisor)
		{
			Tick	remainder = dividend%divisor;
			if (remainder != 0 && ((remainder < 0) != (divisor < 0)))
				remainder += divisor;
			return remainder;
		}

/*
 * A length of time, in ticks. Signed, so an interval can be negative and
 * subtraction needs no special case.
 *
 * The three duration types convert to each other, each counted in its own
 * unit, so a conversion to a coarser unit truncates towards zero and says so
 * by doing it: there is no hidden rounding anywhere.
 */
class	Interval
{
public:
	Interval() : ticks_(0) {}
	Interval(Tick p_ticks) : ticks_(p_ticks) {}
	Interval(const Milliseconds& ms);
	Interval(const Seconds& sec);

	Tick		ticks() const { return ticks_; }
	bool		isNull() const { return ticks_ == NullTick; }
	time_t		asTime_t() const;			// Reports a null, and returns 0
	Milliseconds	asMilliseconds() const;			// A null stays null
	Seconds		asSeconds() const;

	StrVal		toString() const;			// Seconds, as "-1.50000000", or "null"
	static Interval	fromString(StrVal text, ErrNum* err_return = 0);

	/*
	 * Arithmetic on a null reports, and returns another null, so that a null
	 * spreads through a computation instead of becoming a number. So does a
	 * result that ran past the end of what a Tick can hold: that is a null too,
	 * and never a wrapped number that looks like a time. A comparison is not
	 * arithmetic - it needs no value - so a null is equal to a null, and before
	 * every real interval, the null tick being the lowest there is.
	 */
	Interval	operator-() const
			{ return isNull() ? nullOperand("negating") : Interval(-ticks_); }
	Interval	operator+(const Interval& addend) const
			{ return sum(addend); }
	Interval	operator-(const Interval& minuend) const
			{ return difference(minuend); }
	Interval&	operator+=(const Interval& addend)
			{ *this = sum(addend); return *this; }
	Interval&	operator-=(const Interval& minuend)
			{ *this = difference(minuend); return *this; }

	bool		operator==(const Interval& other) const
			{ return ticks_ == other.ticks_; }
	bool		operator!=(const Interval& other) const
			{ return ticks_ != other.ticks_; }
	bool		operator<(const Interval& other) const
			{ return ticks_ < other.ticks_; }
	bool		operator<=(const Interval& other) const
			{ return ticks_ <= other.ticks_; }
	bool		operator>(const Interval& other) const
			{ return ticks_ > other.ticks_; }
	bool		operator>=(const Interval& other) const
			{ return ticks_ >= other.ticks_; }

protected:
	/*
	 * A null where a value was needed: reports that, naming the operation it
	 * was needed for, and returns another null. Reported and not asserted, a
	 * null being data - a field never filled in, a record that says "no
	 * time" - and not a fault in the program. Defined in src/datetime.cpp,
	 * where Error() is at hand, like every reporting member.
	 */
	Interval	nullOperand(const char* operation) const;

	// The two operations, checked: a null operand or a result that ran past
	// the end of a Tick is reported and answered with a null, and the caller
	// gets neither a wrapped number nor a crash. Defined in src/datetime.cpp.
	Interval	sum(const Interval& addend) const;
	Interval	difference(const Interval& minuend) const;

	Tick		ticks_;
};

/*
 * A length of time in milliseconds, counted in milliseconds and not in ticks,
 * so that a thread scheduling call wanting milliseconds is given the number
 * the caller wrote. Counted in a 64-bit signed integer, where the class this
 * replaces counted an unsigned long.
 */
class	Milliseconds
{
public:
	Milliseconds() : ms_(0) {}
	Milliseconds(Tick p_ms) : ms_(p_ms) {}
	Milliseconds(const Interval& interval);
	Milliseconds(const Seconds& sec);

	Tick		ms() const { return ms_; }
	bool		isNull() const { return ms_ == NullTick; }
	time_t		asTime_t() const;			// Reports a null, and returns 0
	Interval	asInterval() const;			// A null stays null
	Seconds		asSeconds() const;

	StrVal		toString() const;			// Milliseconds, as "1500", or "null"
	static Milliseconds fromString(StrVal text, ErrNum* err_return = 0);

	Milliseconds	operator-() const
			{ return isNull() ? nullOperand("negating") : Milliseconds(-ms_); }
	Milliseconds	operator+(const Milliseconds& addend) const
			{ return sum(addend); }
	Milliseconds	operator-(const Milliseconds& minuend) const
			{ return difference(minuend); }
	Milliseconds&	operator+=(const Milliseconds& addend)
			{ *this = sum(addend); return *this; }
	Milliseconds&	operator-=(const Milliseconds& minuend)
			{ *this = difference(minuend); return *this; }

	bool		operator==(const Milliseconds& other) const
			{ return ms_ == other.ms_; }
	bool		operator!=(const Milliseconds& other) const
			{ return ms_ != other.ms_; }
	bool		operator<(const Milliseconds& other) const
			{ return ms_ < other.ms_; }
	bool		operator<=(const Milliseconds& other) const
			{ return ms_ <= other.ms_; }
	bool		operator>(const Milliseconds& other) const
			{ return ms_ > other.ms_; }
	bool		operator>=(const Milliseconds& other) const
			{ return ms_ >= other.ms_; }

protected:
	Milliseconds	nullOperand(const char* operation) const;	// Reports, returns null
	Milliseconds	sum(const Milliseconds& addend) const;		// The checked operations
	Milliseconds	difference(const Milliseconds& minuend) const;

	Tick		ms_;
};

/*
 * A length of time in seconds, counted in seconds. The class this replaces
 * counted these in an unsigned long, which cannot hold the second before the
 * epoch; this one can.
 */
class	Seconds
{
public:
	Seconds() : sec_(0) {}
	Seconds(Tick p_sec) : sec_(p_sec) {}
	Seconds(const Interval& interval);
	Seconds(const Milliseconds& ms);

	Tick		seconds() const { return sec_; }
	bool		isNull() const { return sec_ == NullTick; }
	time_t		asTime_t() const;			// Reports a null, and returns 0
	Interval	asInterval() const;			// A null stays null
	Milliseconds	asMilliseconds() const;

	StrVal		toString() const;			// Seconds, as "5", or "null"
	static Seconds	fromString(StrVal text, ErrNum* err_return = 0);

	Seconds		operator-() const
			{ return isNull() ? nullOperand("negating") : Seconds(-sec_); }
	Seconds		operator+(const Seconds& addend) const
			{ return sum(addend); }
	Seconds		operator-(const Seconds& minuend) const
			{ return difference(minuend); }
	Seconds&	operator+=(const Seconds& addend)
			{ *this = sum(addend); return *this; }
	Seconds&	operator-=(const Seconds& minuend)
			{ *this = difference(minuend); return *this; }

	bool		operator==(const Seconds& other) const
			{ return sec_ == other.sec_; }
	bool		operator!=(const Seconds& other) const
			{ return sec_ != other.sec_; }
	bool		operator<(const Seconds& other) const
			{ return sec_ < other.sec_; }
	bool		operator<=(const Seconds& other) const
			{ return sec_ <= other.sec_; }
	bool		operator>(const Seconds& other) const
			{ return sec_ > other.sec_; }
	bool		operator>=(const Seconds& other) const
			{ return sec_ >= other.sec_; }

protected:
	Seconds		nullOperand(const char* operation) const;	// Reports, returns null
	Seconds		sum(const Seconds& addend) const;		// The checked operations
	Seconds		difference(const Seconds& minuend) const;

	Tick		sec_;
};

/*
 * How far a civil time is from UTC, counted in minutes east of Greenwich, so
 * that a local reading is the UTC reading plus the offset. Zero is UTC, which
 * is also what a reader that never asked for a zone gets.
 *
 * Minutes and not seconds, because there is no zone database here and so no
 * historical local mean time to represent: every zone the world keeps is a
 * whole number of minutes from UTC, and a zone this class cannot name is one
 * the caller can still write as a number of minutes.
 */
class	UtcOffset
{
public:
	UtcOffset() : minutes_(0) {}			// UTC

	static UtcOffset	hours(int hours)
				{ return UtcOffset(hours*60); }
	static UtcOffset	minutes(int minutes)
				{ return UtcOffset(minutes); }

	short		asMinutes() const { return minutes_; }
	int32_t		asSeconds() const { return (int32_t)minutes_*60; }
	bool		isUTC() const { return minutes_ == 0; }

	StrVal		toString() const;			// "Z", or "+10:00"

	bool		operator==(const UtcOffset& other) const
			{ return minutes_ == other.minutes_; }
	bool		operator!=(const UtcOffset& other) const
			{ return minutes_ != other.minutes_; }

protected:
	/*
	 * An offset too large to be a whole number of minutes in a short is not
	 * an offset: it is truncated to one rather than quietly becoming a
	 * different zone.
	 */
	UtcOffset(int p_minutes) : minutes_((short)p_minutes)
		{ StrppAssert(p_minutes == minutes_); }

	short		minutes_;
};

/*
 * A point in time: a count of ticks from the epoch, knowing no zone. The zone
 * is given to the operations that need one, which are the ones that read or
 * write a calendar date, and it is never stored.
 *
 * The date and time of one instant is therefore a question with as many
 * returns as there are zones, which is why asGregorian() takes the zone and
 * returns a Gregorian, and why nothing here caches one.
 */
class	DateTime
{
public:
	DateTime() : ticks_(0) {}			// The epoch
	explicit DateTime(Tick p_ticks) : ticks_(p_ticks) {}

	static DateTime	fromTicks(Tick ticks) { return DateTime(ticks); }
	Tick		ticks() const { return ticks_; }
	bool		isNull() const { return ticks_ == NullTick; }

	// The only two things here that ask the host what time it is:
	static DateTime	now();					// UTC, from the host's clock
	static UtcOffset localOffset(DateTime when);		// The host's offset at that instant
	static DateTime	nowWithOffset(UtcOffset* offset = 0);	// now(), and the offset to read it in

	time_t		asTime_t() const;			// Reports a null, and returns 0
	static DateTime	fromTime_t(time_t t);

	/*
	 * The ISO 8601 text of this instant in a zone. The zone is written as Z
	 * for UTC and as +hh:mm otherwise, unless the flags say to leave it off:
	 * an instant whose zone is not written is not the same instant to a
	 * reader. Punctuation is written by default, which is the extended form
	 * the standard prefers; flags = 0 is the basic form. A null instant has
	 * no text of a time, and says "null".
	 */
	Gregorian	asGregorian(UtcOffset off = UtcOffset(), Interval* time_of_day = 0) const;
	StrVal		toString(UtcOffset off = UtcOffset(), int flags = IsoPunctuate) const;
	static DateTime	fromString(StrVal text, UtcOffset* offset = 0, ErrNum* err_return = 0);

	/*
	 * Arithmetic on a null reports, and returns another null, so that a null
	 * spreads through a computation instead of becoming an instant. So does a
	 * result that ran past either end of the range: that is a null too, and
	 * never an instant that never was. A comparison is not arithmetic - it
	 * needs no value - so a null is equal to a null, and before every real
	 * instant, the null tick being the lowest there is.
	 */
	Interval	operator-(const DateTime& minuend) const
			{ return between(minuend); }
	DateTime	operator+(const Interval& addend) const
			{ return sum(addend); }
	DateTime	operator-(const Interval& minuend) const
			{ return difference(minuend); }
	DateTime&	operator+=(const Interval& addend)
			{ *this = sum(addend); return *this; }
	DateTime&	operator-=(const Interval& minuend)
			{ *this = difference(minuend); return *this; }

	bool		operator==(const DateTime& other) const
			{ return ticks_ == other.ticks_; }
	bool		operator!=(const DateTime& other) const
			{ return ticks_ != other.ticks_; }
	bool		operator<(const DateTime& other) const
			{ return ticks_ < other.ticks_; }
	bool		operator<=(const DateTime& other) const
			{ return ticks_ <= other.ticks_; }
	bool		operator>(const DateTime& other) const
			{ return ticks_ > other.ticks_; }
	bool		operator>=(const DateTime& other) const
			{ return ticks_ >= other.ticks_; }

	/*
	 * The range a DateTime can name. The ends are not whole days, so the
	 * days they fall on are named too: they are the boundaries the calendar
	 * refuses to cross, and the dates it refuses to name.
	 *
	 * MinTicks is one past the null tick, which is what makes the first
	 * instant of the range the second tick of the day: null is not an
	 * instant, so the range starts above it, and a calendar conversion can
	 * never produce it (the guard in Gregorian::asDateTime is stated in
	 * these terms).
	 */
	static const Tick	MinTicks	= NullTick + 1;
	static const Tick	MaxTicks	= (Tick)0x7FFFFFFFFFFFFFFFULL;
	static const Tick	MinDay		= -337400;	// 924 BC, March 25
	static const Tick	MaxDay		= 1797639;	// 4922 AD, October 8

protected:
	DateTime	nullOperand(const char* operation) const;	// Reports, returns null
	DateTime	sum(const Interval& addend) const;		// The checked operations
	DateTime	difference(const Interval& minuend) const;
	Interval	between(const DateTime& minuend) const;

	Tick		ticks_;
};

#endif	// DATETIME_H
