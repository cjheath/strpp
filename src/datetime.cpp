/*
 * The parts of datetime.h that are better not inlined, and the two things here
 * that ask the host what time it is.
 *
 * Reporting needs the message set, which a header cannot include (see
 * str_msg.h), so every member that reports is defined here.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<datetime.h>
#include	<gregorian.h>
#include	<str_msg.h>
#if	!defined(MSW) && !defined(HAVE_NO_CLOCK)
#include	<sys/time.h>			// gettimeofday
#endif

// Seconds from 1/1/1970 to 1/1/2000, which is 10957 days: the bridge to time_t
static const Tick	Time_tAtEpoch = 946684800;

/*
 * Reading a text as a decimal number, with or without a fraction, and whether
 * the digits are a whole number of a unit or a count of seconds. This is what
 * all three fromString()s share: none of them accepts anything but a number in
 * their own unit as text, so none needs a grammar of its own.
 */
typedef struct DecimalScan
{
	bool		ok;			// A number, and nothing but a number
	bool		negative;		// A leading - was read
	uint64_t	whole;			// The digits before the radix
	uint64_t	fraction;		// The digits after it, scaled to 10^-8 seconds
} DecimalScan;

static DecimalScan
scanDecimal(StrVal text, bool allow_fraction)
{
	DecimalScan	answer;
	StrValIndex	i = 0;
	StrValIndex	len = text.length();

	answer.ok = false;
	answer.negative = false;
	answer.whole = 0;
	answer.fraction = 0;

	while (i < len && UCS4IsWhite(text[i]))
		i++;
	if (i < len && (text[i] == '+' || text[i] == '-'))
	{
		answer.negative = text[i] == '-';
		i++;
	}
	if (i >= len || UCS4Digit(text[i]) < 0)
		return answer;			// No digits to read

	while (i < len && UCS4Digit(text[i]) >= 0)
	{
		unsigned	d = (unsigned)UCS4Digit(text[i]);
		if (answer.whole > (UINT64_MAX - d)/10)
			return answer;		// More digits than any count here can hold
		answer.whole = answer.whole*10 + d;
		i++;
	}

	int		digits = 0;
	if (allow_fraction && i < len && (text[i] == '.' || text[i] == ','))
	{
		i++;
		while (i < len && UCS4Digit(text[i]) >= 0)
		{
			if (digits < 8)		// Past 10^-8 seconds is past our resolution
			{
				answer.fraction = answer.fraction*10 + (unsigned)UCS4Digit(text[i]);
				digits++;
			}
			i++;
		}
	}
	while (digits++ < 8)			// Scale the fraction to 10^-8 seconds
		answer.fraction *= 10;

	while (i < len && UCS4IsWhite(text[i]))
		i++;
	if (i != len)
		return answer;			// Something after the number

	answer.ok = true;
	return answer;
}

/*
 * A whole number from a text, which is what Milliseconds and Seconds read. The
 * bound is the same either sign: one below the most negative time that exists
 * is the null tick, and a text cannot spell that out and mean a value.
 */
static bool
scanWhole(StrVal text, Tick* answer)
{
	DecimalScan	scan = scanDecimal(text, false);
	uint64_t	limit = 0x7FFFFFFFFFFFFFFFULL;

	if (!scan.ok || scan.whole > limit)
		return false;

	*answer = scan.negative ? (Tick)(0 - scan.whole) : (Tick)scan.whole;
	return true;
}

/*
 * Whether a text says "null", which is what a null value is written as and so
 * what it reads back from. It is checked before the number scan in all four
 * fromString()s, since it is not a number and could not be read as one.
 */
static bool
scanNull(StrVal text)
{
	StrValIndex	i = 0;
	StrValIndex	len = text.length();

	while (i < len && UCS4IsWhite(text[i]))
		i++;
	if (i+4 > len
	 || text[i] != 'n' || text[i+1] != 'u' || text[i+2] != 'l' || text[i+3] != 'l')
		return false;
	i += 4;
	while (i < len && UCS4IsWhite(text[i]))
		i++;
	return i == len;
}

/*
 * The three operations, checked before they are done rather than after: a
 * sum, a difference or a scaling that would run past the end of a Tick is
 * refused, because a wrapped count of ticks is a duration or a time that never
 * was, and nothing about it looks wrong. The check is on the operands, since
 * signed overflow is undefined and there is nothing to inspect afterwards.
 *
 * The range is DateTime's, which is the range of a Tick: everything above the
 * null tick is a time or a duration this library can hold.
 */
static bool
addFits(Tick addend, Tick augend, Tick* answer)
{
	if (augend > 0 ? addend > DateTime::MaxTicks - augend
		       : addend < DateTime::MinTicks - augend)
		return false;
	*answer = addend + augend;
	return true;
}

static bool
subFits(Tick minuend, Tick subtrahend, Tick* answer)
{
	if (subtrahend > 0 ? minuend < DateTime::MinTicks + subtrahend
			   : minuend > DateTime::MaxTicks + subtrahend)
		return false;
	*answer = minuend - subtrahend;
	return true;
}

static bool
scaleFits(Tick value, Tick factor, Tick* answer)
{
	if (value > 0 ? value > DateTime::MaxTicks/factor
		      : value < floorDiv(DateTime::MinTicks, factor))
		return false;
	*answer = value*factor;
	return true;
}

/*
 * A count of one unit as a count of another, which is what the unit conversions
 * do. Reported when it will not fit, and answered with a null: a caller who
 * asked for seconds as milliseconds is not handed a wrapped count instead.
 */
static Tick
scaledTicks(Tick value, Tick factor, const char* type, const char* operation)
{
	Tick	answer;

	if (!scaleFits(value, factor, &answer))
	{
		ErrorTIM_ResultOverflow(type, operation);
		return NullTick;
	}
	return answer;
}

/*
 * Interval
 */
Interval::Interval(const Milliseconds& ms)
: ticks_(NullTick)
{
	if (!ms.isNull())
		ticks_ = scaledTicks(ms.ms(), 100000, "Interval", "converting from milliseconds");
}

Interval::Interval(const Seconds& sec)
: ticks_(NullTick)
{
	if (!sec.isNull())
		ticks_ = scaledTicks(sec.seconds(), TicksPerSecond, "Interval", "converting from seconds");
}

/*
 * A null where a value was needed. Reported, and not asserted: a null is data -
 * a field never filled in, a record that says "no time" - so a program that
 * meets one has a data problem rather than a fault of its own. The answer is
 * another null, so that a null spreads through a computation instead of
 * becoming a number part-way along it.
 */
Interval
Interval::nullOperand(const char* operation) const
{
	ErrorTIM_NullValue("Interval", operation);
	return Interval(NullTick);
}

/*
 * Adding and subtracting, checked. Each answers a null for a null operand or
 * for a result that ran past the end of a Tick, and reports which of the two it
 * was: the caller is never handed a wrapped count, and needs no report to know
 * that a null is not a value.
 */
Interval
Interval::sum(const Interval& addend) const
{
	Tick	answer;

	if (isNull() || addend.isNull())
		return nullOperand("adding");
	if (!addFits(ticks_, addend.ticks_, &answer))
	{
		ErrorTIM_ResultOverflow("Interval", "adding");
		return Interval(NullTick);
	}
	return Interval(answer);
}

Interval
Interval::difference(const Interval& minuend) const
{
	Tick	answer;

	if (isNull() || minuend.isNull())
		return nullOperand("subtracting");
	if (!subFits(ticks_, minuend.ticks_, &answer))
	{
		ErrorTIM_ResultOverflow("Interval", "subtracting");
		return Interval(NullTick);
	}
	return Interval(answer);
}

time_t
Interval::asTime_t() const
{
	if (isNull())
	{
		ErrorTIM_NullValue("Interval", "read as a time_t");
		return 0;
	}
	return (time_t)(ticks_/TicksPerSecond);	// Truncated, as time_t is
}

Milliseconds
Interval::asMilliseconds() const
{
	if (isNull())
		return Milliseconds(NullTick);	// A null stays null: nothing was invented
	return Milliseconds(ticks_/100000);	// Truncated towards zero
}

Seconds
Interval::asSeconds() const
{
	if (isNull())
		return Seconds(NullTick);
	return Seconds(ticks_/TicksPerSecond);	// Truncated towards zero
}

/*
 * Seconds and a fraction of one, as "-1.50000000", or "null". The fraction is
 * always eight digits, so that the text reads back as the same number of ticks;
 * the sign is written once, in front, and the digits come from the unsigned
 * form because the most negative tick has no positive counterpart.
 */
StrVal
Interval::toString() const
{
	bool		negative;
	uint64_t	ticks;
	StrVal		text;
	StrVal		fraction;

	if (isNull())
		return "null";
	negative = ticks_ < 0;
	ticks = negative ? 0 - (uint64_t)ticks_ : (uint64_t)ticks_;
	text = StrVal::fromUInt64(ticks/TicksPerSecond);
	fraction = StrVal::fromUInt64(ticks%TicksPerSecond);

	fraction = StrVal("00000000").shorter(fraction.length()) + fraction;
	return (negative ? StrVal("-") : StrVal()) + text + StrVal(".") + fraction;
}

Interval
Interval::fromString(StrVal text, ErrNum* err_return)
{
	DecimalScan	scan;
	uint64_t	limit;
	uint64_t	ticks;

	if (err_return)
		*err_return = 0;
	if (scanNull(text))
		return Interval(NullTick);	// "null" is a null, and not a failure

	scan = scanDecimal(text, true);

	/*
	 * The largest count either sign can read. Not one more for the negative
	 * side: one tick below the smallest time that exists is the null tick,
	 * which means "no value", and a text that spells it out must not be read
	 * as a value at all - a number that cannot be held is refused, not turned
	 * into a null behind the caller's back.
	 */
	limit = ((uint64_t)0x7FFFFFFFFFFFFFFFULL - scan.fraction)/TicksPerSecond;
	if (!scan.ok || scan.whole > limit)
	{
		if (err_return)
			*err_return = ErrorTIM_InvalidText(text);
		return Interval(NullTick);	// A text that is not a time is no time
	}

	ticks = scan.whole*TicksPerSecond + scan.fraction;
	return Interval(scan.negative ? (Tick)(0 - ticks) : (Tick)ticks);
}

/*
 * Milliseconds
 */
Milliseconds::Milliseconds(const Interval& interval)
: ms_(interval.isNull() ? NullTick : interval.ticks()/100000)	// Truncated towards zero
{
}

Milliseconds::Milliseconds(const Seconds& sec)
: ms_(NullTick)
{
	if (!sec.isNull())
		ms_ = scaledTicks(sec.seconds(), 1000, "Milliseconds", "converting from seconds");
}

Milliseconds
Milliseconds::nullOperand(const char* operation) const
{
	ErrorTIM_NullValue("Milliseconds", operation);
	return Milliseconds(NullTick);
}

Milliseconds
Milliseconds::sum(const Milliseconds& addend) const
{
	Tick	answer;

	if (isNull() || addend.isNull())
		return nullOperand("adding");
	if (!addFits(ms_, addend.ms_, &answer))
	{
		ErrorTIM_ResultOverflow("Milliseconds", "adding");
		return Milliseconds(NullTick);
	}
	return Milliseconds(answer);
}

Milliseconds
Milliseconds::difference(const Milliseconds& minuend) const
{
	Tick	answer;

	if (isNull() || minuend.isNull())
		return nullOperand("subtracting");
	if (!subFits(ms_, minuend.ms_, &answer))
	{
		ErrorTIM_ResultOverflow("Milliseconds", "subtracting");
		return Milliseconds(NullTick);
	}
	return Milliseconds(answer);
}

time_t
Milliseconds::asTime_t() const
{
	if (isNull())
	{
		ErrorTIM_NullValue("Milliseconds", "read as a time_t");
		return 0;
	}
	return (time_t)(ms_/1000);
}

Interval
Milliseconds::asInterval() const
{
	if (isNull())
		return Interval(NullTick);
	return Interval(scaledTicks(ms_, 100000, "Interval", "converting from milliseconds"));
}

Seconds
Milliseconds::asSeconds() const
{
	if (isNull())
		return Seconds(NullTick);
	return Seconds(ms_/1000);		// Truncated towards zero
}

StrVal
Milliseconds::toString() const
{
	return isNull() ? StrVal("null") : StrVal::fromInt64(ms_);
}

Milliseconds
Milliseconds::fromString(StrVal text, ErrNum* err_return)
{
	Tick		ms;

	if (err_return)
		*err_return = 0;
	if (scanNull(text))
		return Milliseconds(NullTick);

	if (!scanWhole(text, &ms))
	{
		if (err_return)
			*err_return = ErrorTIM_InvalidText(text);
		return Milliseconds(NullTick);	// A text that is not a time is no time
	}
	return Milliseconds(ms);
}

/*
 * Seconds
 */
Seconds::Seconds(const Interval& interval)
: sec_(interval.isNull() ? NullTick : interval.ticks()/TicksPerSecond)	// Truncated
{
}

Seconds::Seconds(const Milliseconds& ms)
: sec_(ms.isNull() ? NullTick : ms.ms()/1000)				// Truncated
{
}

Seconds
Seconds::nullOperand(const char* operation) const
{
	ErrorTIM_NullValue("Seconds", operation);
	return Seconds(NullTick);
}

Seconds
Seconds::sum(const Seconds& addend) const
{
	Tick	answer;

	if (isNull() || addend.isNull())
		return nullOperand("adding");
	if (!addFits(sec_, addend.sec_, &answer))
	{
		ErrorTIM_ResultOverflow("Seconds", "adding");
		return Seconds(NullTick);
	}
	return Seconds(answer);
}

Seconds
Seconds::difference(const Seconds& minuend) const
{
	Tick	answer;

	if (isNull() || minuend.isNull())
		return nullOperand("subtracting");
	if (!subFits(sec_, minuend.sec_, &answer))
	{
		ErrorTIM_ResultOverflow("Seconds", "subtracting");
		return Seconds(NullTick);
	}
	return Seconds(answer);
}

time_t
Seconds::asTime_t() const
{
	if (isNull())
	{
		ErrorTIM_NullValue("Seconds", "read as a time_t");
		return 0;
	}
	return (time_t)sec_;
}

Interval
Seconds::asInterval() const
{
	if (isNull())
		return Interval(NullTick);
	return Interval(scaledTicks(sec_, TicksPerSecond, "Interval", "converting from seconds"));
}

Milliseconds
Seconds::asMilliseconds() const
{
	if (isNull())
		return Milliseconds(NullTick);
	return Milliseconds(scaledTicks(sec_, 1000, "Milliseconds", "converting from seconds"));
}

StrVal
Seconds::toString() const
{
	return isNull() ? StrVal("null") : StrVal::fromInt64(sec_);
}

Seconds
Seconds::fromString(StrVal text, ErrNum* err_return)
{
	Tick		sec;

	if (err_return)
		*err_return = 0;
	if (scanNull(text))
		return Seconds(NullTick);

	if (!scanWhole(text, &sec))
	{
		if (err_return)
			*err_return = ErrorTIM_InvalidText(text);
		return Seconds(NullTick);	// A text that is not a time is no time
	}
	return Seconds(sec);
}

/*
 * UtcOffset
 */
StrVal
UtcOffset::toString() const
{
	int	minutes = minutes_;
	int	m;

	if (minutes == 0)
		return "Z";			// UTC is Z, and is never written as +00:00
	m = minutes < 0 ? -minutes : minutes;
	return StrVal(minutes < 0 ? "-" : "+")
		+ StrVal::format("{1:02}:{2:02}", VariantArray() << m/60 << m%60);
}

/*
 * DateTime
 */
DateTime
DateTime::nullOperand(const char* operation) const
{
	ErrorTIM_NullValue("DateTime", operation);
	return DateTime(NullTick);
}

DateTime
DateTime::sum(const Interval& addend) const
{
	Tick	answer;

	if (isNull() || addend.isNull())
		return nullOperand("adding");
	if (!addFits(ticks_, addend.ticks(), &answer))
	{
		ErrorTIM_ResultOverflow("DateTime", "adding");
		return DateTime(NullTick);
	}
	return DateTime(answer);
}

DateTime
DateTime::difference(const Interval& minuend) const
{
	Tick	answer;

	if (isNull() || minuend.isNull())
		return nullOperand("subtracting");
	if (!subFits(ticks_, minuend.ticks(), &answer))
	{
		ErrorTIM_ResultOverflow("DateTime", "subtracting");
		return DateTime(NullTick);
	}
	return DateTime(answer);
}

Interval
DateTime::between(const DateTime& minuend) const
{
	Tick	answer;

	if (isNull() || minuend.isNull())
	{
		(void)nullOperand("subtracting");
		return Interval(NullTick);
	}
	if (!subFits(ticks_, minuend.ticks_, &answer))
	{
		ErrorTIM_ResultOverflow("Interval", "subtracting");
		return Interval(NullTick);
	}
	return Interval(answer);
}

time_t
DateTime::asTime_t() const
{
	if (isNull())
	{
		ErrorTIM_NullValue("DateTime", "read as a time_t");
		return 0;
	}
	return (time_t)(ticks_/TicksPerSecond + Time_tAtEpoch);
}

/*
 * A time_t is a count of seconds from 1970, and this is a count of ticks from
 * 2000, so the bridge is a subtraction and a scaling - both of which a time_t
 * from a corrupt file or a wrong-width field can take past the range. Reported
 * when they do, and answered with a null rather than a date that never was.
 */
DateTime
DateTime::fromTime_t(time_t t)
{
	Tick	seconds;
	Tick	ticks;

	if (!subFits((Tick)t, Time_tAtEpoch, &seconds)
	 || !scaleFits(seconds, TicksPerSecond, &ticks))
	{
		ErrorTIM_ResultOverflow("DateTime", "reading a time_t");
		return DateTime(NullTick);
	}
	return DateTime(ticks);
}

/*
 * The current time. This and localOffset() are the only two places in the
 * library that ask the host anything, and a target with no clock at all
 * defines HAVE_NO_CLOCK and is told so rather than failing to build.
 */
DateTime
DateTime::now()
{
#if	defined(HAVE_NO_CLOCK)
	ErrorTIM_NoClock();
	return DateTime(NullTick);		// No clock, so no instant: a null, said out loud
#elif	defined(MSW)
	FILETIME	filetime;
	ULARGE_INTEGER	uli;
	Tick		ticks;

	GetSystemTimeAsFileTime(&filetime);
	memcpy(&uli, &filetime, sizeof(uli));	// Avoid alignment problems
	// FILETIMEs count 100ns units from 1/1/1601; ours count 10ns from 1/1/2000
	if (!scaleFits((Tick)(uli.QuadPart - 125911584000000000ULL), 10, &ticks))
	{
		ErrorTIM_NoClock();
		return DateTime(NullTick);
	}
	return DateTime(ticks);
#else
	struct timeval	now;
	DateTime	instant;

	if (gettimeofday(&now, 0) != 0)
	{
		ErrorTIM_NoClock();
		return DateTime(NullTick);	// No clock, so no instant: a null, said out loud
	}

	instant = DateTime::fromTime_t(now.tv_sec);	// Reports a clock past the range
	if (instant.isNull())
		return instant;
	// Microseconds to 10^-8 seconds, and the same checked addition a caller gets
	return instant + Interval((Tick)now.tv_usec*100);
#endif
}

/*
 * How far the host's civil time is from UTC at a given instant, which is the
 * offset in force then rather than the one in force now. The host's own rules
 * are used, so a zone's summer time is accounted for and its history is not:
 * there is no zone database here, and a caller who needs one knows the offset
 * and passes it.
 *
 * A host that cannot answer is reported, and UTC is answered: an UtcOffset
 * cannot be null, so the honest thing is to say that the offset is not known
 * and answer the one the caller can trust to be a whole answer.
 */
UtcOffset
DateTime::localOffset(DateTime when)
{
#if	defined(HAVE_NO_CLOCK)
	ErrorTIM_NoZone();
	return UtcOffset();
#else
	time_t		at = when.asTime_t();
	struct tm	utc;
	time_t		as_local;

#if	defined(MSW)
	if (gmtime_s(&utc, &at) != 0)
	{
		ErrorTIM_NoZone();
		return UtcOffset();
	}
#else
	if (!gmtime_r(&at, &utc))
	{
		ErrorTIM_NoZone();
		return UtcOffset();
	}
#endif
	utc.tm_isdst = -1;		// The UTC reading, taken as a civil one
	as_local = mktime(&utc);
	if (as_local == (time_t)-1)
	{
		ErrorTIM_NoZone();
		return UtcOffset();
	}
	return UtcOffset::minutes((int)((at - as_local)/60));
#endif
}

DateTime
DateTime::nowWithOffset(UtcOffset* offset)
{
	DateTime	utc = now();

	if (utc.isNull())
	{
		if (offset)
			*offset = UtcOffset();	// And no zone was read for it either
		return utc;
	}
	if (offset)
		*offset = localOffset(utc);
	return utc;			// The instant, and the offset to read it in
}

/*
 * The calendar date and time of this instant in the given zone. The zone is
 * applied once, here, and the fields are read from the shifted count: this is
 * the only place where an instant becomes a civil time, and nothing caches the
 * answer.
 *
 * A null instant has no date to answer, and no null a Gregorian could carry, so
 * it is reported and answered with the value that says "no date": a Gregorian
 * that is a time of day alone, at midnight. A caller who checks errors sees the
 * report; one who does not gets nothing rather than a date that never was.
 *
 * So is an instant within eighteen hours of either end of the range, read in a
 * zone that would take it past the end: the shift is checked, and the answer is
 * the same "no date" rather than a wrapped date from the other end of the
 * range, which would read as a perfectly ordinary day and be wrong.
 */
Gregorian
DateTime::asGregorian(UtcOffset off, Interval* time_of_day) const
{
	Tick	ticks;
	Tick	within;

	if (isNull())
	{
		ErrorTIM_NullValue("DateTime", "reading a date");
		if (time_of_day)
			*time_of_day = Interval(NullTick);
		return Gregorian();
	}

	if (!addFits(ticks_, (Tick)off.asSeconds()*TicksPerSecond, &ticks))
	{
		ErrorTIM_ResultOverflow("DateTime", "reading a date in that zone");
		if (time_of_day)
			*time_of_day = Interval(NullTick);
		return Gregorian();
	}
	within = floorMod(ticks, TicksPerDay);		// Since midnight, local

	if (time_of_day)
		*time_of_day = Interval(within);
	// Day numbers count from 0/0/0000 and ticks from the epoch, so the one
	// has to be moved onto the other's origin
	return Gregorian::fromDayNumber(floorDiv(ticks, TicksPerDay) + EpochDay, within);
}

/*
 * A null instant has no text of a time, and says so. It reports nothing here:
 * toString is on the path a *report* is rendered along - Variant::value_text
 * and as_json call it - and a report made while one is being rendered would be
 * made into the buffer that is being read.
 *
 * The zone is written only for a value that has a date: a time of day is not
 * in any zone, and a conversion that could not answer has none to write.
 */
StrVal
DateTime::toString(UtcOffset off, int flags) const
{
	Gregorian	gregorian;
	StrVal		text;

	if (isNull())
		return "null";

	gregorian = asGregorian(off);
	text = gregorian.toString(flags);
	if (gregorian.hasDate() && (flags & (IsoNoZone|IsoDateOnly)) == 0)
		text += off.toString();
	return text;
}

DateTime
DateTime::fromString(StrVal text, UtcOffset* offset, ErrNum* err_return)
{
	ErrNum		e = 0;
	UtcOffset	zone;
	Gregorian	gregorian;
	DateTime	answer;

	if (err_return)
		*err_return = 0;
	if (offset)
		*offset = UtcOffset();
	if (scanNull(text))
		return DateTime(NullTick);	// "null" is a null, and not a failure

	gregorian = Gregorian::fromString(text, &zone, &e);
	if (!e)
		answer = gregorian.asDateTime(zone, &e);	// Reports a time of day, or a date out of range
	if (err_return)
		*err_return = e;
	if (!e && offset)
		*offset = zone;
	return e ? DateTime() : answer;
}
