## Time and date: intervals and instants

`#include	<datetime.h>`

Time comes in two kinds that are forever being confused with each other, so
this header has two types. An `Interval` is a duration - how long something
takes; a `DateTime` is a point in time - when it happens. Both count
the same tick, so an interval adds to an instant with no conversion, and one
instant less another is an interval.

`Milliseconds` and `Seconds` are lengths of time counted in their own units, so
that a call wanting milliseconds is given the number the caller wrote. They
convert to and from `Interval`, and each other, truncating towards zero when
the unit is coarser, so nothing is rounded behind the caller's back.

`UtcOffset` is how far a civil time is from UTC, in minutes east of Greenwich.

The calendar - a year, a month, a day, an hour, a minute, a second - is in
[gregorian.md](gregorian.md).

### The tick

Times are counted in ticks of 10⁻⁸ seconds, in a 64-bit signed integer. That is
exactly the unit of a Windows FILETIME, so one converts without rounding, and
it is fine enough for anything that does not need nanoseconds.

A count of ticks spans 5845 years: 2922 either side of the epoch, which is
2000-01-01T00:00:00Z - tick zero. The range therefore runs from 924 BC to
4922 AD, and a date outside it is *refused* and reported when it is asked to
become a `DateTime`, never wrapped or truncated. See
[gregorian.md](gregorian.md) for which dates can be named at all.

Leap seconds are not represented: a minute here has 60 seconds, always. What
that buys is that the count agrees with POSIX time, and with every other count
of seconds from an epoch, so a `time_t`, a stored file time and this library all
mean the same thing by the same number.

What it costs is worth stating plainly, because no value shows it: the civil
readings of a tick are always right, and a *difference* of two instants is a
count of UTC seconds, which is not the physical time between them. Five leap
seconds have been inserted since 2000-01-01, so the interval between an instant
before the first of them and one after the last is five seconds short of the
time that actually elapsed - a span that began then and ended now was five
seconds longer than the difference says. Which differences are affected cannot
be told from the values; it needs a table of leap seconds, and this library has
none. A duration that must be physical - a timeout, an elapsed time - belongs to
a monotonic clock, which no leap second touches. A `DateTime` returns *when*,
and an `Interval` between two of them is a count of UTC seconds.

### Null times

A time can be null, which is not a time at all: a value that was never set, or a
record that says "no time". Null is `NullTick`, the lowest tick there is - the
one value with no positive equivalent, so no real time can be turned into a null
by being negated - and `isNull()` says whether a value is one. It is not zero,
and not the epoch: both of those are real times.

A null used where a value is needed reports `TIMERR_NULL_VALUE`:

- **Arithmetic** on a null - adding, subtracting or negating - reports, and
  returns another null, so that a null spreads through a computation instead of
  becoming a number part-way along it. This includes `DateTime` arithmetic, and
  the difference of two instants when either is null.
- **Reading one as a value** reports and returns the least misleading thing it
  can: `asTime_t()` returns 0, and `asGregorian()` returns a value with no date,
  rather than a date that never was.

Two things are deliberately silent. A null that *stays* null - a unit
conversion, `asMilliseconds()` of a null interval - invents no value, so it is
not reported. And a comparison is not arithmetic: it needs no value, so a null
equals a null, and sorts before every real time, the null tick being the lowest
there is.

As text a null is `null`, which is also what it reads back from:
`Interval::fromString("null")` is a null interval, and a null `DateTime` or
`Interval` in a Variant renders as JSON's own `null` rather than as the text of
one. A number that would *be* the null tick is refused as a text, and a text
that is not a time at all is answered with a null rather than with zero: zero is
a time, and the text did not say zero.

A `Gregorian` has no null - a civil date is a date, and a time of day with no
date is what year 0 means - so only the four tick-counting types can be null.

### When an operation cannot return correctly

An operation that cannot return correctly reports it, and returns the type's
"nothing". It never returns a wrapped number: a count of ticks that ran past the
end of its range is a duration of the wrong sign or an instant in the wrong
century, and nothing about it looks wrong. Four reports cover every case, all in
the `TIM` set:

	This `Interval` is null, so there is no value for adding
	The result of subtracting is past the range of `Interval`
	The current time is not known: this target has no clock, or reading it failed
	The host's zone offset is not known at that instant, so UTC is answered

What the caller is given instead:

| The operation | What it returns |
|---|---|
| Adding, subtracting or negating a null, or an instant moved by a null | another null, so that a null spreads rather than becoming a number |
| A sum, difference or unit conversion that ran past the end of a Tick | a null of the result's type |
| `DateTime::asGregorian()` for a null, or for an instant a zone would push past either end of the range | a `Gregorian` with no date: a time of day, at midnight |
| `DateTime::fromTime_t()` for a count of seconds outside the range | a null instant |
| `Gregorian::fromDayNumber()` for a day whose year will not fit the fields | a `Gregorian` with no date |
| `now()` with no clock, or a clock that failed | a null instant |
| `localOffset()` where the host cannot say | UTC, and the report says it |

Two of those are the whole range rather than a corner of it: the span from the
first instant to the last is more than a `Tick` holds - 5845 years against 2922
either side - so the interval between the two ends of the range is refused. And
an instant within eighteen hours of either end, read in a zone that would take
it past, has no date, where before it answered a date from the *other* end of
the range with a zone designator on it.

Where the result is a `Gregorian` with no date, that value is a time of day -
midnight - and `hasDate()` is false. `isValid()` is true of it, a time of day
being a valid value; the report and `hasDate()` are what say that a conversion
failed rather than that the result is a time.

A caller who expects a value to be out of range and does not want the buffer
filled by each one takes a checkpoint first and rolls back after:

	ErrBuf::MsgSequence	at = ErrCheckpoint();
	Interval		sum = a + b;
	ErrRollback(at);	// ...and sum.isNull() says whether it was returned

### Zones are offsets, not names

A `DateTime` knows no time zone. It is a count of ticks from the epoch, and
that is all; the zone is a parameter of each operation that needs one. So there
is no local/UTC flag that can fall out of step with the value it describes, and
no adjustment that can be applied twice or left applied by accident.

An offset is a number of minutes - east positive, zero being UTC - so a local
reading is the UTC reading plus the offset. There is no zone database here: a
zone this library cannot name is one the caller can still write as a number.
Two things do ask the host what time it is: `now()` reads the clock, and
`localOffset()` asks for the offset in force at an instant, so that `now()` in
local time needs no arguments. A target with no clock at all defines
`HAVE_NO_CLOCK`, and its `now()` reports that rather than failing to build.

### Public methods

Defined in [datetime.h](https://github.com/cjheath/strpp/blob/main/include/datetime.h).

Making one:

- `Interval()`, `Interval(Tick ticks)` - an interval of that many 10⁻⁸ second
  ticks, or zero.
- `Interval(Milliseconds)`, `Interval(Seconds)` - the same length, converted.
  `Milliseconds(Tick ms)`, `Milliseconds(const Interval&)`,
  `Milliseconds(const Seconds&)` and the `Seconds` forms are the same for those
  two, each counted in its own unit.
- `UtcOffset()`, `UtcOffset::hours(h)`, `UtcOffset::minutes(m)` - an offset
  east of Greenwich. Zero minutes is UTC.
- `DateTime()` - the epoch, 2000-01-01T00:00:00Z.
- `DateTime::fromTicks(Tick)`, `DateTime(Tick)` - an instant that many ticks
  from the epoch.
- `DateTime::fromTime_t(time_t)`, `DateTime::fromString(...)` - the same
  instant, given another way.

Reading:

- `ticks()` - the count of 10⁻⁸ second ticks. The one read all of these return.
- `isNull()` - whether the value is null: a time that was never set. See "Null
  times" below.
- `ms()`, `seconds()` - a `Milliseconds` or a `Seconds` as its own number.
- `asMinutes()`, `asSeconds()` - an offset. `isUTC()` says whether it is zero.
- `asMilliseconds()`, `asSeconds()`, `asInterval()` - the same length in
  another unit, truncating towards zero.
- `asTime_t()` - a count of seconds, as `time_t`, truncated.
- `asGregorian(off = UtcOffset(), Interval* time_of_day = 0)` - the date and
  time of this instant in a zone. The zone is applied here and nowhere else,
  and the time since midnight can be asked for separately through the
  out-pointer, which defaults to null.
- `toString(off = UtcOffset(), flags = IsoPunctuate)` - the instant as ISO 8601
  text in that zone, written with `Z` for UTC and `+hh:mm` otherwise. The
  default is the punctuated form the standard prefers; `flags = 0` is the basic
  form. The zone is left off when `IsoNoZone` is set, and the fraction of a
  second is written to however many digits the flags ask for, 0 to 8 - the
  default is none, so a text that must read back as the same instant asks for
  eight.
- `now()` - the current instant, from the host's clock.
- `localOffset(when)` - the host's offset at that instant, which is the one in
  force then and not the one in force now.
- `nowWithOffset(UtcOffset* offset = 0)` - `now()`, and the offset to read it
  in, from one reading of the clock.

Building:

- `operator+`, `operator-`, `+=`, `-=` - an interval added to or taken from
  another, or from an instant; one instant less another is an `Interval`.
- `operator==`, `!=`, `<`, `<=`, `>`, `>=` - two of a kind compared.
- `Interval::fromString(text, ErrNum* err_return = 0)` and the same for
  `Milliseconds` and `Seconds` - a number in the type's own unit, and nothing
  else. A text that is not one is reported as `TIMERR_INVALID_TEXT`.

### Times as text

An interval is written in seconds with eight digits of a fraction, so
`Interval(-150000000)` writes `-1.50000000` and reads back as the same count of
ticks. A `Milliseconds` or a `Seconds` is written as its own number: `1500`
and `-90`. Neither carries its unit in the text, because the type is what says
which unit it is.

Reading a duration is reading a number at a resolution, which is what
`StrVal::asFixedPoint` and `asInteger` do - see [strval.md](strval.md),
"Integers as text". The resolution is 10⁻⁸ seconds: a text written with more
places than that is read to eight and the rest dropped, quietly, because that
is precision below what this layer keeps. A text whose *whole* part is too
large for a count of ticks, or that is not a number at all, is a different
matter and is reported as `TIMERR_INVALID_TEXT` rather than answered with a
number that is not the one it named.

An instant is written in ISO 8601, in the zone it is asked for:

	DateTime::fromString("2002-01-03T11:12:13Z").toString()
	// 2002-01-03T11:12:13Z

	DateTime::fromString("2002-01-03T11:12:13Z").toString(UtcOffset::hours(10))
	// 2002-01-03T21:12:13+10:00

`DateTime::fromString` reads the zone from the text as well as the time, and
hands it back through the out-pointer, so a text can be read and written again
in the zone it came in. A text with no zone designator is read as **UTC** and
never as local time: a result that depends on where it is read is not a
result. Reading an instant from a text that names a time of day but no date is
reported as `TIMERR_NO_DATE`, and one whose date is outside the range as
`TIMERR_OUT_OF_RANGE`.

See [Errors](error.md) and the `TIM` set in `strpp_err.h`.
