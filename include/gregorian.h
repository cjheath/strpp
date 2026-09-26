#if	!defined(GREGORIAN_H)
#define	GREGORIAN_H
/*
 * The civil calendar: a date and a time of day, in the proleptic Gregorian
 * calendar, and the ISO 8601 text of one.
 *
 * A Gregorian holds the fields as written on a wall: a year, a month, a day, an
 * hour, a minute, a second and a fraction of a second. It knows no zone, and it
 * is not a point in time - it is what a clock and a calendar say, which is a
 * different thing in every zone. asDateTime() turns one into an instant by
 * being told the zone; DateTime::asGregorian() turns one back.
 *
 * The year is the count the calendar uses, in which 1 AD is year 1 and the year
 * before it is 1 BC, so the years before 1 AD count downwards: -1 is 1 BC, -2
 * is 2 BC. There is no year 0 in that reckoning, and this class uses the year
 * that never existed to mean something it does need: a time of day with no
 * date. So year() answers 0 for a value that is a time alone, hasDate() says
 * which kind it is, and "12:34" reads as a time rather than as a century.
 *
 * A text is read and written as ISO 8601, in the forms the standard allows
 * without a zone database: a date, a time, or both, punctuated or not, with
 * the fraction of a second written to as many digits as are wanted. A text
 * with a zone is read in that zone, and a text with no zone is read as UTC -
 * never as local time, which would make the answer depend on where it is read.
 * A date before 1 AD is written in ISO 8601's expanded form, which numbers the
 * same years astronomically: 1 BC is written 0000, and 2 BC as -0001.
 *
 * The arithmetic here counts days from 0/0/0000, so that 1/1/0001 is day 1 and
 * the day before it is 0. Those are the day numbers of the calendar this class
 * replaces, and are kept so that a date and a day number still mean the same
 * thing across the two.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdint>

#include	<datetime.h>

class	Gregorian
{
public:
	Gregorian();
	Gregorian(int year, int month, int day,
			int hour = 0, int minute = 0, int second = 0, int32_t fraction = 0);

	/*
	 * The same, checked: a field set that is not a date that exists is
	 * reported and refused, rather than quietly normalised into a different
	 * date. The constructor cannot do that - a constructor that fails has
	 * nowhere to say so - so the check is here, and isValid() asks it.
	 */
	static Gregorian	fromYMD(int year, int month, int day,
					int hour = 0, int minute = 0, int second = 0,
					int32_t fraction = 0, ErrNum* err_return = 0);

	// The date a day number names, with that day's time of day
	static Gregorian	fromDayNumber(Tick day, Tick within_day = 0);

	// The ISO 8601 text of a date, a time, or both. The zone read is handed
	// back, and a text with none is read as UTC
	static Gregorian	fromString(StrVal text, UtcOffset* offset = 0,
					ErrNum* err_return = 0);

	// Reading a value:
	int		year() const { return year_; }
	int		month() const { return month_; }
	int		day() const { return day_; }
	int		hour() const { return hour_; }
	int		minute() const { return minute_; }
	int		second() const { return second_; }
	int32_t		fraction() const { return fraction_; }	// 10^-8 seconds
	bool		hasDate() const { return year_ != 0; }

	bool		isValid() const;
	int		dayOfWeek() const;		// 0 = Sunday, -1 with no date
	int		dayOfYear() const;		// 1..366, 0 with no date
	Tick		dayNumber() const;		// Days from 0/0/0000, 0 with no date
	Interval	timeSinceMidnight() const;

	static bool	isLeapYear(int year);
	static int	daysInMonth(int year, int month);

	/*
	 * The instant this civil time names, in the given zone. Refused, and
	 * reported, for a time of day with no date, for a field set that is not
	 * a date that exists, and for a date outside the range a DateTime can
	 * hold - which is a boundary, not a wrap.
	 */
	DateTime	asDateTime(UtcOffset off = UtcOffset(), ErrNum* err_return = 0) const;

	StrVal		toString(int flags = IsoPunctuate) const;

	bool		operator==(const Gregorian& other) const
			{
				return year_ == other.year_ && month_ == other.month_
				    && day_ == other.day_ && hour_ == other.hour_
				    && minute_ == other.minute_ && second_ == other.second_
				    && fraction_ == other.fraction_;
			}
	bool		operator!=(const Gregorian& other) const
			{ return !(*this == other); }

protected:
	short		year_, month_, day_;
	short		hour_, minute_, second_;
	int32_t		fraction_;		// 10^-8 seconds, 0 to 99999999
};

#endif	// GREGORIAN_H
