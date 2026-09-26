/*
 * The calendar: the day number of a date, the date of a day number, and the
 * ISO 8601 text of either.
 *
 * The day-number arithmetic is Tantzen's, translated to C by Clifford Heath
 * from Nat Howard's Fortran translation of the Algol original published as
 * algorithm 199 in the Collected Algorithms of the ACM.
 *
 * One thing about it is not kept: every division in it is a floor division.
 * Tantzen's rearrangement assumes it, and C divides towards zero, which is the
 * same answer for a year from 1 AD onwards and a day out before it - and its
 * inverse, ltodate() below, comes apart completely for a day number before the
 * epoch. This library can name dates back to 924 BC, so the difference matters,
 * and it is settled once here rather than at each use.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<gregorian.h>
#include	<str_msg.h>
#include	<strformat.h>			// StrVal::format, for the ISO 8601 text

/*
 * Chosen so that 1/1/0001 is day 1, which makes the day before it day 0, the
 * "0/0/0000" that a count of days from nothing is counted from.
 */
static const Tick	DayOffset = -306;

/*
 * Tantzen's algorithm counts astronomical years, in which the year before 1 AD
 * is year 0. A Gregorian counts 1 BC as -1 and keeps year 0 for a time of day,
 * so the two differ by one for every year before 1 AD. ISO 8601's expanded form
 * counts as the astronomers do, so a text takes the same shift, and the two
 * functions here are the only place it is applied.
 */
static int
astronomical(int year)
{
	return year < 0 ? year+1 : year;
}

static int
civil(int year)
{
	return year <= 0 ? year-1 : year;
}

/*
 * The day number of a Y/M/D, in astronomical years. A date is valid when
 * ltodate() of this answer yields the same Y/M/D, which is how the calendar
 * checks a field set it did not construct itself.
 */
static Tick
datol(int year, int mon, int day)
{
	if (mon < 3)			/* Adjust for February */
	{
		mon += 9;
		--year;
	}
	else
		mon -= 3;

	return floorDiv(146097*floorDiv(year, 100), 4)	/* Century days */
	     + floorDiv(1461*floorMod(year, 100), 4)	/* Year days */
	     + (153*mon + 2)/5				/* And the month's */
	     + day					/* And the day */
	     + DayOffset;
}

/*
 * The date a day number names, and the day of the week it fell on (0 = Sunday),
 * which is its inverse.
 */
static int
ltodate(Tick j, int ymd[3])
{
	Tick	d, m, y;

	Tick	wd = floorMod(j, 7);

	j -= DayOffset;
	y = floorDiv(4*j - 1, 146097);
	j = 4*j - 1 - 146097*y;
	d = floorDiv(j, 4);
	j = floorDiv(4*d + 3, 1461);
	d = 4*d + 3 - 1461*j;
	d = floorDiv(d + 4, 4);
	m = floorDiv(5*d - 3, 153);
	d = 5*d - 3 - 153*m;
	d = floorDiv(d + 5, 5);
	y = 100*y + j;
	if (m < 10)
		m += 3;
	else
	{
		m -= 9;
		++y;
	}
	ymd[0] = (int)y;
	ymd[1] = (int)m;
	ymd[2] = (int)d;

	return (int)wd;
}

// A date outside the range a DateTime can hold, named with both boundaries
static ErrNum
errorOutOfRange(const Gregorian& when)
{
	return ErrorTIM_OutOfRange(
		when.toString(IsoDateOnly|IsoPunctuate),
		Gregorian::fromDayNumber(DateTime::MinDay).toString(IsoDateOnly|IsoPunctuate),
		Gregorian::fromDayNumber(DateTime::MaxDay).toString(IsoDateOnly|IsoPunctuate));
}

/*
 * Reading ISO 8601. The forms accepted are the ones the standard allows without
 * a zone database:
 *
 *	[sign]YYYY[-MM[-DD]]		a date, 4 or more digits, punctuated
 *	YYYYMMDD[hhmmss]		a date, and a time if all 14 are there
 *	hh[:mm[:ss]]			a time of day, punctuated
 *	hhmmss				a time of day, unpunctuated
 *	[.ffffff]			a fraction of a second, after the seconds
 *	Z, [+-]hh[:mm], [+-]hhmm	the zone, when one is given
 *
 * A date and a time are separated by T or a space. A text with no zone is read
 * as UTC and never as local time: an answer that depends on where it is read is
 * not an answer.
 *
 * A bare run of four digits is refused rather than read, since it could be a
 * year or an hour and a minute, and a caller who meant either can punctuate it.
 */
/*
 * How many digits stand at `i`, leaving `i` on the first character after them:
 * what decides which form a text is written in is what follows its first run
 * of digits, and a separator is only a separator in that position.
 */
static int
digitRun(StrVal text, StrValIndex& i)
{
	StrValIndex	len = text.length();
	int		count = 0;

	while (i + count < len && UCS4Digit(text[i+count]) >= 0)
		count++;
	i += count;
	return count;
}

static bool
scanDigits(StrVal text, StrValIndex& i, int count, int* value)
{
	StrValIndex	len = text.length();
	int		answer = 0;

	if (i + count > len)
		return false;
	for (int n = 0; n < count; n++)
	{
		int	d = UCS4Digit(text[i+n]);
		if (d < 0)
			return false;
		answer = answer*10 + d;
	}
	i += count;
	*value = answer;
	return true;
}

Gregorian::Gregorian()
: year_(0), month_(0), day_(0)
, hour_(0), minute_(0), second_(0), fraction_(0)
{
}

Gregorian::Gregorian(int year, int month, int day, int hour, int minute, int second, int32_t fraction)
: year_((short)year), month_((short)month), day_((short)day)
, hour_((short)hour), minute_((short)minute), second_((short)second)
, fraction_(fraction)
{
}

Gregorian
Gregorian::fromYMD(int year, int month, int day, int hour, int minute, int second, int32_t fraction, ErrNum* err_return)
{
	Gregorian	answer(year, month, day, hour, minute, second, fraction);

	if (err_return)
		*err_return = 0;
	if (!answer.isValid() && err_return)
		*err_return = ErrorTIM_InvalidYMDHMS(year, month, day, hour, minute, second);
	return answer;
}

bool
Gregorian::isValid() const
{
	if (fraction_ < 0 || fraction_ >= TicksPerSecond)
		return false;
	if (hour_ < 0 || hour_ > 23
	 || minute_ < 0 || minute_ > 59
	 || second_ < 0 || second_ > 59)		// Second 60 is a leap second: not represented
		return false;
	if (!hasDate())
		return month_ == 0 && day_ == 0;	// A time of day, and nothing more
	return month_ >= 1 && month_ <= 12
	    && day_ >= 1 && day_ <= daysInMonth(year_, month_);
}

bool
Gregorian::isLeapYear(int year)
{
	Tick	at = astronomical(year);

	return floorMod(at, 4) == 0
	    && (floorMod(at, 100) != 0 || floorMod(at, 400) == 0);
}

static const short	days_in_month[13] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

int
Gregorian::daysInMonth(int year, int month)
{
	if (month < 1 || month > 12)
		return 0;
	if (month == 2 && isLeapYear(year))
		return 29;
	return days_in_month[month];
}

Tick
Gregorian::dayNumber() const
{
	if (!hasDate())
		return 0;			// A time of day is not on any day
	return datol(astronomical(year_), month_, day_);
}

int
Gregorian::dayOfWeek() const
{
	int	ymd[3];

	if (!hasDate())
		return -1;
	return ltodate(dayNumber(), ymd);
}

int
Gregorian::dayOfYear() const
{
	if (!hasDate())
		return 0;
	return (int)(dayNumber() - datol(astronomical(year_), 1, 1)) + 1;
}

Interval
Gregorian::timeSinceMidnight() const
{
	return Interval(((Tick)hour_*3600 + (Tick)minute_*60 + second_)*TicksPerSecond + fraction_);
}

Gregorian
Gregorian::fromDayNumber(Tick day, Tick within_day)
{
	int	ymd[3];
	Tick	seconds;
	Tick	fraction;

	ltodate(day, ymd);
	seconds = floorDiv(within_day, TicksPerSecond);
	fraction = floorMod(within_day, TicksPerSecond);

	return Gregorian(civil(ymd[0]), ymd[1], ymd[2],
			(int)(seconds/3600), (int)((seconds/60)%60), (int)(seconds%60),
			(int32_t)fraction);
}

DateTime
Gregorian::asDateTime(UtcOffset off, ErrNum* err_return) const
{
	Tick	day, days, within, ticks;

	if (err_return)
		*err_return = 0;

	if (!hasDate())
	{
		if (err_return)
			*err_return = ErrorTIM_NoDate();
		return DateTime();
	}
	if (!isValid())
	{
		if (err_return)
			*err_return = ErrorTIM_InvalidYMDHMS(year_, month_, day_, hour_, minute_, second_);
		return DateTime();
	}

	day = datol(astronomical(year_), month_, day_);
	if (day < DateTime::MinDay || day > DateTime::MaxDay)
	{
		if (err_return)
			*err_return = errorOutOfRange(*this);
		return DateTime();
	}

	// The zone applies to the civil time, and moves it by at most half a day
	days = day - EpochDay;
	within = timeSinceMidnight().ticks() - (Tick)off.asSeconds()*TicksPerSecond;
	days += floorDiv(within, TicksPerDay);
	within = floorMod(within, TicksPerDay);

	/*
	 * The product below is what overflows, so the day is bounded before the
	 * multiply rather than after it. The days at either end of the range are
	 * not whole days, so a time of day that runs off the end of the last one
	 * is refused the same way a date past it is.
	 */
	if (days > floorDiv(DateTime::MaxTicks - within, TicksPerDay)
	 || days < floorDiv(DateTime::MinTicks + TicksPerDay - 1 - within, TicksPerDay))
	{
		if (err_return)
			*err_return = errorOutOfRange(*this);
		return DateTime();
	}

	ticks = days*TicksPerDay + within;
	return DateTime::fromTicks(ticks);
}

StrVal
Gregorian::toString(int flags) const
{
	int	digits = flags & IsoFractionDigits;
	bool	punctuated = (flags & IsoPunctuate) != 0;
	StrVal	text;

	if (hasDate())
	{
		/*
		 * ISO 8601 writes a year before 1 AD in its expanded form, which
		 * counts as the astronomers do: 1 BC is written 0000 and 2 BC as
		 * -0001. Four digits, and the sign is not one of them, so a year
		 * that has a sign has to be given one place more.
		 */
		int	year = astronomical(year_);
		StrVal	year_text = year < 0
				? StrVal::format("{1:05}", VariantArray() << year)
				: StrVal::format("{1:04}", VariantArray() << year);
		StrVal	month_text = StrVal::format("{1:02}", VariantArray() << (int)month_);
		StrVal	day_text = StrVal::format("{1:02}", VariantArray() << (int)day_);

		text = punctuated
			? year_text + StrVal("-") + month_text + StrVal("-") + day_text
			: year_text + month_text + day_text;
		if (flags & IsoDateOnly)
			return text;
		text += (flags & IsoSpaceForT) ? " " : "T";
	}

	text += punctuated
		? StrVal::format("{1:02}:{2:02}:{3:02}", VariantArray() << (int)hour_ << (int)minute_ << (int)second_)
		: StrVal::format("{1:02}{2:02}{3:02}", VariantArray() << (int)hour_ << (int)minute_ << (int)second_);
	if (digits > 0)
	{
		// Built at all eight digits and cut back, so that none is invented
		StrVal	fraction = StrVal::format("{1:08}", VariantArray() << fraction_);

		text += StrVal(".") + fraction.shorter(8 - digits);
	}
	return text;
}

Gregorian
Gregorian::fromString(StrVal text, UtcOffset* offset, ErrNum* err_return)
{
	StrValIndex	i = 0;
	StrValIndex	len = text.length();
	StrValIndex	start;
	int		count;
	bool		have_sign = false;
	int		sign = 1;
	int		year = 0, month = 1, day = 1;
	int		hour = 0, minute = 0, second = 0;
	int32_t		fraction = 0;
	int		zone = 0;
	bool		have_date = false;
	bool		have_time = false;

	if (err_return)
		*err_return = 0;
	if (offset)
		*offset = UtcOffset();

	while (i < len && UCS4IsWhite(text[i]))
		i++;
	if (i < len && (text[i] == '+' || text[i] == '-'))
	{
		have_sign = true;
		sign = text[i] == '-' ? -1 : 1;
		i++;
	}

	start = i;
	count = digitRun(text, i);
	if (count == 0)
		goto bad;

	if (have_sign || (i < len && text[i] == '-'))
	{				// [sign]YYYY[-MM[-DD]]
		if (count < 4)
			goto bad;
		i = start;
		if (!scanDigits(text, i, count, &year))
			goto bad;
		year *= sign;
		if (year < -32768 || year > 32767)
			goto bad;		// More than a year this calendar can hold
		year = civil(year);		// The text counts years as the astronomers do
		have_date = true;
		if (i < len && text[i] == '-')
		{
			i++;
			if (!scanDigits(text, i, 2, &month))
				goto bad;
			if (i < len && text[i] == '-')
			{
				i++;
				if (!scanDigits(text, i, 2, &day))
					goto bad;
			}
		}
	}
	else if (i < len && text[i] == ':')
	{				// hh:mm[:ss]
		if (count != 2)
			goto bad;
		i = start;
		if (!scanDigits(text, i, 2, &hour))
			goto bad;
		i++;			// The ':' that must be there
		if (!scanDigits(text, i, 2, &minute))
			goto bad;
		have_time = true;
		if (i < len && text[i] == ':')
		{
			i++;
			if (!scanDigits(text, i, 2, &second))
				goto bad;
		}
	}
	else if (count == 6)
	{				// hhmmss
		i = start;
		if (!scanDigits(text, i, 2, &hour)
		 || !scanDigits(text, i, 2, &minute)
		 || !scanDigits(text, i, 2, &second))
			goto bad;
		have_time = true;
	}
	else if (count == 8 || count == 14)
	{				// yyyymmdd[hhmmss]
		i = start;
		if (!scanDigits(text, i, 4, &year)
		 || !scanDigits(text, i, 2, &month)
		 || !scanDigits(text, i, 2, &day))
			goto bad;
		year = civil(year);
		have_date = true;
		have_time = count == 14;
		if (have_time
		 && (!scanDigits(text, i, 2, &hour)
		  || !scanDigits(text, i, 2, &minute)
		  || !scanDigits(text, i, 2, &second)))
			goto bad;
	}
	else
		goto bad;		// Four digits alone are a year or a time, and we cannot tell

	if (have_date && i < len && (text[i] == 'T' || text[i] == ' '))
	{				// The time of a date, punctuated or not
		i++;
		if (!scanDigits(text, i, 2, &hour))
			goto bad;
		have_time = true;
		if (i < len && text[i] == ':')
		{
			i++;
			if (!scanDigits(text, i, 2, &minute))
				goto bad;
			if (i < len && text[i] == ':')
			{
				i++;
				if (!scanDigits(text, i, 2, &second))
					goto bad;
			}
		}
		else if (i+2 <= len && UCS4Digit(text[i]) >= 0)
		{
			if (!scanDigits(text, i, 2, &minute))
				goto bad;
			if (i+2 <= len && UCS4Digit(text[i]) >= 0
			 && !scanDigits(text, i, 2, &second))
				goto bad;
		}
	}

	if (have_time && i < len && (text[i] == '.' || text[i] == ','))
	{				// The fraction of a second
		int	digits = 0;

		i++;
		while (i < len && UCS4Digit(text[i]) >= 0)
		{
			if (digits < 8)	// Past 10^-8 seconds is past our resolution
			{
				fraction = fraction*10 + UCS4Digit(text[i]);
				digits++;
			}
			i++;
		}
		if (digits == 0)
			goto bad;
		while (digits++ < 8)
			fraction *= 10;
	}

	if (i < len)
	{
		if (text[i] == 'Z' || text[i] == 'z')
			i++;
		else if (text[i] == '+' || text[i] == '-')
		{
			int	zone_sign = text[i] == '-' ? -1 : 1;
			int	zone_hour, zone_minute = 0;

			i++;
			if (!scanDigits(text, i, 2, &zone_hour))
				goto bad;
			if (i < len && text[i] == ':')
			{
				i++;
				if (!scanDigits(text, i, 2, &zone_minute))
					goto bad;
			}
			else if (i+2 <= len && UCS4Digit(text[i]) >= 0
			      && !scanDigits(text, i, 2, &zone_minute))
				goto bad;
			if (zone_hour > 18 || zone_minute > 59)
				goto bad;
			zone = zone_sign*(zone_hour*60 + zone_minute);
		}
	}

	while (i < len && UCS4IsWhite(text[i]))
		i++;
	if (i != len)
		goto bad;

	if (hour > 23 || minute > 59 || second > 59)
		goto bad;
	if (have_date)
	{
		if (month < 1 || month > 12 || day < 1 || day > daysInMonth(year, month))
			goto bad;
	}
	else
		month = day = 0;	// A time of day, and no date

	if (offset)
		*offset = UtcOffset::minutes(zone);
	return Gregorian(year, month, day, hour, minute, second, fraction);

bad:
	/*
	 * Nothing was read, or what was read is not a date or a time. The fields
	 * read so far are answered with the error, as reading a number answers
	 * the digits it read: the error is what says they are not the value.
	 */
	if (err_return)
		*err_return = ErrorTIM_InvalidText(text);
	if (!have_date)
		month = day = 0;
	return Gregorian(year, month, day, hour, minute, second, fraction);
}
