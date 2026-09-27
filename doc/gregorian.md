## Gregorian: civil dates and ISO 8601

`#include	<gregorian.h>`

A `Gregorian` holds the fields as written on a wall: a year, a month, a day, an
hour, a minute, a second, and a fraction of a second as a count of 10⁻⁸ second
ticks. It knows no time zone, and it is not a point in time - it is what a
clock and a calendar say, which is a different thing in every zone.

To get an instant, tell it the zone: `asDateTime(off)` returns the `DateTime`
that a civil time names in that zone, and `DateTime::asGregorian(off)` returns
the civil time of an instant. That is the whole of the conversion between them,
and nothing here caches a result.

### The years before 1 AD

The year is the count the calendar uses, in which 1 AD is year 1 and the year
before it is 1 BC, so the years before 1 AD count downwards: -1 is 1 BC, -2 is
2 BC. There is no year 0 in that reckoning, and this class uses the year that
never existed to mean something it does need: **a time of day with no date**. So
`year()` returns 0 for a value that is a time alone, `hasDate()` says which kind
it is, and `"12:34"` reads as a time rather than as a century.

ISO 8601, for its part, counts years as the astronomers do, inserting the year
0 that the calendar has not got: 1 BC is written `0000`, and 2 BC as `-0001`.
Both the reader and the writer shift by the one year, so the two counts never
meet except here and in the tests.

A day number counts days from `0/0/0000`, so that 1/1/0001 is day 1 and the day
before it is day 0. Those are the day numbers the library this replaces used,
and they are kept so that a date and a day number still mean the same thing
across the two.

### Reading ISO 8601

The forms accepted are the ones the standard allows without a zone database:

	[sign]YYYY[-MM[-DD]]		a date, 4 or more digits, punctuated
	YYYYMMDD[hhmmss]		a date, and a time if all 14 are there
	hh[:mm[:ss]]			a time of day, punctuated
	hhmmss				a time of day, unpunctuated
	[.ffffff]			a fraction of a second, after the seconds
	Z, [+-]hh[:mm], [+-]hhmm	the zone, when one is given

A date and a time are separated by `T` or a space. A bare run of four digits is
refused rather than read, since it could be a year or an hour and a minute, and
a caller who meant either can punctuate it.

Reading a text with no zone designator does not assume local time: it is read
as UTC, because a result that depends on where it is read is not a result.
The zone that was read is handed back through an out-pointer, so a text read
and written again keeps the zone it came in.

A text that is not a date or a time at all, and a field set that is not a date
that exists - the 31st of February, or an hour of 24 - are reported as
`TIMERR_INVALID_TEXT`. `fromYMD()` reports a field set of its own as
`TIMERR_INVALID_YMDHMS`, and returns the value it was given, so that `isValid()`
can be asked about it.

### Writing ISO 8601

`toString(flags)` writes the extended (punctuated) form by default, and the
flags choose another:

	IsoPunctuate		the - and : of the extended form
	IsoDateOnly		the date, without the time
	IsoSpaceForT		a space where ISO 8601 has its T
	IsoNoZone		leave the zone designator off
	IsoFractionDigits	the low nibble: how many digits of a second, 0 to 8

A civil time has no zone, so `Gregorian::toString` writes none; an instant's
text belongs to `DateTime::toString`, which writes the offset it was rendered
in. The fraction is written to the number of digits asked for, and no more:
the default is none, so a caller who wants a text that reads back as the same
value to the tick asks for all eight.

	Gregorian(2002,1,3,11,12,13).toString()
	// 2002-01-03T11:12:13

	Gregorian(2002,1,3,11,12,13).toString(IsoPunctuate|IsoSpaceForT)
	// 2002-01-03 11:12:13

	Gregorian(-1,1,1).toString(IsoPunctuate|IsoDateOnly)
	// 0000-01-01

### The range

A civil date can be any the fields can hold, but a `DateTime` counts ticks from
2000-01-01, and so can only name 924 BC to 4922 AD. A date outside that is
refused and reported as `TIMERR_OUT_OF_RANGE`, naming the date and both ends of
the range - never wrapped, and never silently a different date. That is the one
place in this layer where a valid calendar date has no instant to be.

`isValid()` checks the fields without converting anything, and `dayOfWeek()`
returns -1 for a value with no date. Both are safe to call in any order and any
number of times: there is one representation, and no cache to fall out of step
with it.

### Public methods

Defined in [gregorian.h](https://github.com/cjheath/strpp/blob/main/include/gregorian.h).

Making one:

- `Gregorian()`, `Gregorian(year, month, day, hour = 0, minute = 0, second = 0,
  fraction = 0)` - a civil date and time. A year of 0 is a time of day with no
  date.
- `Gregorian::fromYMD(...)` - the same fields, checked and reported as
  `TIMERR_INVALID_YMDHMS` when they are not a date that exists.
- `Gregorian::fromDayNumber(day, within_day = 0)` - the date a day number
  names, with that day's time of day. A time of day past the end of its day is
  folded into the day, so that a count of ticks means what it says; a day
  number whose year will not fit the fields is reported as past the range and
  answered with a value that has no date, rather than with a date that never
  was.
- `Gregorian::fromString(text, UtcOffset* offset = 0, ErrNum* err_return = 0)`
  - the ISO 8601 text of a date, a time, or both.

Reading:

- `year()`, `month()`, `day()`, `hour()`, `minute()`, `second()`,
  `fraction()` - the fields. 0 is not a month or a day.
- `hasDate()` - false for a time of day, which is the year 0.
- `isValid()` - whether the fields are a date and time that exist.
- `dayOfWeek()` - 0 for Sunday to 6 for Saturday, or -1 with no date.
- `dayOfYear()` - 1 to 366, or 0 with no date.
- `dayNumber()` - the day, counted from 0/0/0000, or 0 with no date.
- `timeSinceMidnight()` - an `Interval`, which for a time of day is the whole
  value.
- `Gregorian::isLeapYear(year)`, `Gregorian::daysInMonth(year, month)` - the
  calendar's own questions, answered for the count this class uses.
- `asDateTime(off = UtcOffset(), ErrNum* err_return = 0)` - the instant this
  civil time names in that zone. Reported as `TIMERR_NO_DATE` for a time of
  day, `TIMERR_INVALID_YMDHMS` for a field set that is not a date that exists,
  and `TIMERR_OUT_OF_RANGE` beyond either end of what a `DateTime` can hold.
- `toString(flags = IsoPunctuate)` - the ISO 8601 text.
- `operator==`, `operator!=` - whether two values are the same fields.
  Ordering is not offered: two civil times in different zones have no order,
  and an instant is where that question belongs.

See [Errors](error.md) and the `TIM` set in `str_err.h`.
