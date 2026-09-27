/*
 * The time layer: Interval, Milliseconds, Seconds, UtcOffset and DateTime.
 *
 * The conversion matrix below is the legacy library's own test, which was
 * written to be a compile check ("convert it to every other type") as much as
 * a test; here every conversion is asserted by value as well.
 *
 * The values that matter most are the ones the legacy class got wrong: a
 * negative interval rendered as text, truncation towards zero on both sides of
 * the epoch, the day a count of ticks lands on before 1970, and the ends of the
 * range a DateTime can hold.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<datetime.h>
#include	<gregorian.h>
#include	<errbuf.h>

#include	<cstdio>
#include	<cstring>

static bool	show_passes = false;
static int	test_count;
static int	failure_count;
static const char*	new_group;

void		conversion_tests();
void		text_tests();
void		epoch_tests();
void		range_tests();
void		offset_tests();
void		null_tests();
void		overflow_tests();

int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	conversion_tests();
	text_tests();
	epoch_tests();
	range_tests();
	offset_tests();
	null_tests();
	overflow_tests();

	printf("Completed %d tests with %d failures\n", test_count, failure_count);
	return failure_count == 0 ? 0 : 1;
}

static void
test_group(const char* group)
{
	new_group = group;
}

static void
report(const char* when, bool passed, const char* detail)
{
	test_count++;
	if (!passed)
	{
		if (new_group)
		{
			printf("%s:\n", new_group);
			new_group = 0;
		}
		printf("%d:\t%s: FAIL%s%s\n", test_count, when, detail ? " " : "", detail ? detail : "");
		failure_count++;
	}
	else if (show_passes)
	{
		if (new_group)
		{
			printf("%s:\n", new_group);
			new_group = 0;
		}
		printf("%d:\t%s: PASS\n", test_count, when);
	}
}

static void
expect(const char* when, bool cond)
{
	report(when, cond, 0);
}

static void
expect_eq_int(const char* when, long got, long want)
{
	char	detail[64];
	bool	ok = got == want;

	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted %ld got %ld)", want, got);
	report(when, ok, ok ? 0 : detail);
}

static void
expect_eq_str(const char* when, StrVal got, const char* want)
{
	StrVal	wantv(want);
	bool	ok = got.length() == wantv.length() && got == wantv;
	char	detail[512];

	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted \"%s\" got \"%s\")", want, got.asUTF8());
	report(when, ok, ok ? 0 : detail);
}

static void
expect_eq_err(const char* when, ErrNum got, ErrNum want)
{
	char	detail[64];
	bool	ok = got == want;

	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted 0x%X got 0x%X)", (int32_t)want, (int32_t)got);
	report(when, ok, ok ? 0 : detail);
}

void
use(const void*)
{
}

/*
 * Every duration type, made from every other and read back, which is the
 * legacy test's matrix with the values checked rather than only compiled.
 */
/*ARGSUSED*/
void
conversion_tests()
{
	test_group("Intervals: from each type to every other");

	Interval	interval(150000000);		// 1.5 seconds, in ticks
	Interval	zero;
	Interval	negative(-150000000);

	expect_eq_int("an interval counts ticks", (long)interval.ticks(), 150000000);
	expect_eq_int("the default interval is zero", (long)zero.ticks(), 0);

	// Interval from Milliseconds and Seconds, and back
	Milliseconds	from_ms(1500);
	Seconds		from_sec(2);

	expect_eq_int("Milliseconds(1500) is 1.5 seconds of ticks",
			(long)Interval(from_ms).ticks(), 150000000);
	expect_eq_int("Seconds(2) is two seconds of ticks",
			(long)Interval(from_sec).ticks(), 200000000);

	expect_eq_int("a interval read as milliseconds truncates",
			(long)interval.asMilliseconds().ms(), 1500);
	expect_eq_int("...and as seconds truncates towards zero",
			(long)interval.asSeconds().seconds(), 1);
	expect_eq_int("a negative interval truncates towards zero too",
			(long)negative.asSeconds().seconds(), -1);
	expect_eq_int("...which is not the floor", (long)negative.ticks()/TicksPerSecond, -1);

	// Milliseconds from Interval, Seconds and an integer
	Milliseconds	ms_i(interval);
	Milliseconds	ms_s(from_sec);
	Milliseconds	ms_plain(250);

	use(&ms_i); use(&ms_s);
	expect_eq_int("an interval read as milliseconds", (long)ms_i.ms(), 1500);
	expect_eq_int("two seconds read as milliseconds", (long)ms_s.ms(), 2000);
	expect_eq_int("milliseconds convert to ticks", (long)ms_plain.asInterval().ticks(), 25000000);
	expect_eq_int("milliseconds convert to seconds, truncating", (long)ms_s.asSeconds().seconds(), 2);
	expect_eq_int("milliseconds convert to time_t", (long)ms_s.asTime_t(), 2);

	// Seconds from Interval, Milliseconds and an integer
	Seconds		sec_i(interval);
	Seconds		sec_ms(from_ms);
	Seconds		sec_plain(90);

	use(&sec_i); use(&sec_ms);
	expect_eq_int("an interval read as seconds", (long)sec_i.seconds(), 1);
	expect_eq_int("milliseconds read as seconds", (long)sec_ms.seconds(), 1);
	expect_eq_int("seconds convert to milliseconds", (long)sec_plain.asMilliseconds().ms(), 90000);
	expect_eq_int("seconds convert to ticks", (long)sec_plain.asInterval().ticks(), 9000000000LL);
	expect_eq_int("seconds convert to time_t", (long)sec_plain.asTime_t(), 90);

	// Arithmetic on every type, both signs
	expect_eq_int("two intervals add", (long)(interval+interval).ticks(), 300000000);
	expect_eq_int("an interval subtracts to a negative one",
			(long)(zero-interval).ticks(), -150000000);
	expect_eq_int("and negates", (long)(-negative).ticks(), 150000000);
	expect("an interval compares less than a larger one", negative < zero);
	expect("and greater than a smaller one", zero > negative);
	expect("and equal to itself", interval == Interval(150000000));

	Interval	sum(1);
	sum += Interval(2);
	expect_eq_int("+= adds", (long)sum.ticks(), 3);
	sum -= Interval(5);
	expect_eq_int("-= subtracts past zero", (long)sum.ticks(), -2);
}

/*
 * Text, including the negative intervals the legacy class formatted wrongly:
 * it cast a negative remainder to unsigned, and lost the fraction to it.
 */
void
text_tests()
{
	test_group("Intervals: as text, both signs");

	expect_eq_str("an interval is written in seconds",
			Interval(150000000).toString(), "1.50000000");
	expect_eq_str("zero is written with its fraction",
			Interval(0).toString(), "0.00000000");
	expect_eq_str("a negative interval keeps its sign",
			Interval(-150000000).toString(), "-1.50000000");
	expect_eq_str("the smallest tick is written as one",
			Interval(-1).toString(), "-0.00000001");
	expect_eq_str("...and so is the largest positive one",
			Interval(1).toString(), "0.00000001");
	expect_eq_str("a whole second has a zero fraction",
			Interval(TicksPerSecond).toString(), "1.00000000");

	ErrNum	err = 0;
	expect_eq_int("a text reads back as the same ticks",
			(long)Interval::fromString("-1.5", &err).ticks(), -150000000);
	expect_eq_err("...with nothing reported", err, 0);
	expect_eq_int("a text with a comma radix reads too",
			(long)Interval::fromString("1,5", &err).ticks(), 150000000);
	expect_eq_int("eight digits of fraction are all used",
			(long)Interval::fromString("0.00000001", &err).ticks(), 1);
	expect_eq_int("a long interval reads",
			(long)Interval::fromString("100000", &err).ticks(), 10000000000000LL);
	expect_eq_int("...and the most negative one there is",
			(long)Interval::fromString("-92233720368.54775807", &err).ticks(), DateTime::MinTicks);
	expect_eq_err("...reads with nothing reported", err, 0);
	err = 0;
	{
		Interval	too_far = Interval::fromString("-92233720368.54775808", &err);

		expect_eq_err("one tick below the smallest time is the null value, and is refused",
				err, TIMERR_INVALID_TEXT);
		expect("...and answered with a null rather than with a value", too_far.isNull());
	}

	err = 0;
	Interval::fromString("half a minute", &err);
	expect_eq_err("a text that is not a number is reported", err, TIMERR_INVALID_TEXT);
	err = 0;
	Interval::fromString("1.5 seconds", &err);
	expect_eq_err("text after the number is reported", err, TIMERR_INVALID_TEXT);
	err = 0;
	Interval::fromString("", &err);
	expect_eq_err("an empty text is reported", err, TIMERR_INVALID_TEXT);

	test_group("Milliseconds and Seconds: as text");

	expect_eq_str("milliseconds are written in their own unit",
			Milliseconds(1500).toString(), "1500");
	expect_eq_str("seconds are written in their own unit",
			Seconds(-90).toString(), "-90");

	err = 0;
	expect_eq_int("a milliseconds text reads back",
			(long)Milliseconds::fromString("-1500", &err).ms(), -1500);
	expect_eq_err("...with nothing reported", err, 0);
	expect_eq_int("a seconds text reads back",
			(long)Seconds::fromString("90", &err).seconds(), 90);
	err = 0;
	Milliseconds::fromString("1.5", &err);
	expect_eq_err("a fraction is not a whole number of milliseconds", err, TIMERR_INVALID_TEXT);
}

/*
 * The epoch, the bridge to time_t, and the arrival there of a negative count.
 */
void
epoch_tests()
{
	test_group("The epoch and time_t");

	expect_eq_str("the epoch is 2000-01-01T00:00:00Z",
			DateTime().toString(), "2000-01-01T00:00:00Z");
	expect_eq_int("the epoch is tick zero", (long)DateTime().ticks(), 0);
	expect_eq_int("and 946684800 seconds from 1970", (long)DateTime().asTime_t(), 946684800);

	expect_eq_str("time_t zero is 1970-01-01T00:00:00Z",
			DateTime::fromTime_t(0).toString(), "1970-01-01T00:00:00Z");
	expect_eq_int("which is tick -946684800 seconds",
			(long)DateTime::fromTime_t(0).ticks(), -94668480000000000LL);
	expect_eq_int("a time_t survives the round trip",
			(long)DateTime::fromTime_t(1234567890).asTime_t(), 1234567890);

	// The default text carries no fraction, so it is not exact to the tick:
	// a caller who wants a text that reads back as the same instant asks for
	// the digits. Both are ISO 8601, and both are the same instant.
	expect_eq_str("the second before the epoch is the year before",
			DateTime::fromTicks(-1).toString(), "1999-12-31T23:59:59Z");
	expect_eq_str("...written to the tick when the fraction is asked for",
			DateTime::fromTicks(-1).toString(UtcOffset(), IsoPunctuate|8),
			"1999-12-31T23:59:59.99999999Z");
	expect_eq_str("...and the basic form has no punctuation",
			DateTime::fromTicks(-1).toString(UtcOffset(), 0), "19991231T235959Z");
	expect_eq_int("a day of ticks is that many",
			(long)TicksPerDay, 8640000000000LL);
	expect_eq_str("...which is one day before the epoch",
			DateTime::fromTicks(-TicksPerDay).toString(), "1999-12-31T00:00:00Z");

	test_group("Instant arithmetic");

	DateTime	then = DateTime::fromTime_t(0);
	DateTime	now = then + Seconds(90);

	expect_eq_int("an instant plus an interval moves by it",
			(long)(now-then).ticks(), 9000000000LL);
	expect_eq_int("...and the difference of two instants is an interval",
			(long)(now-then).asSeconds().seconds(), 90);
	expect("instants compare", then < now);
	now -= Seconds(90);
	expect("and come back", now == then);
}

/*
 * The ends of the range, which are refused rather than wrapped, and the day
 * numbers that agree with the calendar this replaces.
 */
void
range_tests()
{
	test_group("The range of a DateTime");

	expect_eq_str("the first instant is in 924 BC",
			DateTime::fromTicks(DateTime::MinTicks).asGregorian().toString(IsoDateOnly|IsoPunctuate), "-0923-03-25");
	expect_eq_str("the last instant is in 4922 AD",
			DateTime::fromTicks(DateTime::MaxTicks).asGregorian().toString(IsoDateOnly|IsoPunctuate), "4922-10-08");

	expect_eq_int("the epoch's day number is the legacy one",
			(long)Gregorian(2000,1,1).dayNumber(), 730120);
	expect_eq_int("...and 1970's is too",
			(long)Gregorian(1970,1,1).dayNumber(), 719163);
	expect_eq_int("...and 1/1/0001 is day 1",
			(long)Gregorian(1,1,1).dayNumber(), 1);
	expect_eq_int("...and the day before it, 1 BC's last, is day 0",
			(long)Gregorian(-1,12,31).dayNumber(), 0);
	expect_eq_int("9999-12-31 has the day number the legacy gave it",
			(long)Gregorian(9999,12,31).dayNumber(), 3652059);

	expect_eq_int("the last day of the range is the day it says",
			(long)DateTime::fromTicks(DateTime::MaxTicks).asGregorian().dayNumber(), DateTime::MaxDay);
	expect_eq_int("...and the first is too",
			(long)DateTime::fromTicks(DateTime::MinTicks).asGregorian().dayNumber(), DateTime::MinDay);

	ErrNum	err = 0;
	Gregorian(9999,12,31).asDateTime(UtcOffset(), &err);
	expect_eq_err("a date past the end of the range is refused", err, TIMERR_OUT_OF_RANGE);

	err = 0;
	Gregorian	first = DateTime::fromTicks(DateTime::MinTicks).asGregorian();
	expect_eq_int("...to the day the range begins on", (long)first.dayNumber(), DateTime::MinDay);

	// One day before the first instant's day, and the same instant again
	err = 0;
	Gregorian::fromDayNumber(DateTime::MinDay-1).asDateTime(UtcOffset(), &err);
	expect_eq_err("the day before the range is refused", err, TIMERR_OUT_OF_RANGE);
}

/*
 * Zones are offsets, applied once and never stored. The legacy class used the
 * offset in force *now* for every date; here the offset is an argument, so a
 * date in another half of the year is adjusted the same way and the two do not
 * disagree.
 */
/*ARGSUSED*/
void
offset_tests()
{
	test_group("Zone offsets");

	expect_eq_str("zero minutes is UTC", UtcOffset().toString(), "Z");
	expect_eq_str("ten hours east", UtcOffset::hours(10).toString(), "+10:00");
	expect_eq_str("five and a half hours east", UtcOffset::minutes(330).toString(), "+05:30");
	expect_eq_str("three and a half hours west", UtcOffset::minutes(-210).toString(), "-03:30");
	expect_eq_int("an offset counts minutes", UtcOffset::hours(10).asMinutes(), 600);
	expect_eq_int("...and seconds", UtcOffset::hours(10).asSeconds(), 36000);
	expect("zero is UTC and nothing else is", UtcOffset().isUTC());
	expect("and ten hours is not", !UtcOffset::hours(10).isUTC());

	expect_eq_str("an instant read ten hours east",
			DateTime().toString(UtcOffset::hours(10)), "2000-01-01T10:00:00+10:00");
	expect_eq_str("...and ten hours west",
			DateTime().toString(UtcOffset::hours(-10)), "1999-12-31T14:00:00-10:00");
	expect_eq_str("the same instant as UTC",
			DateTime().toString(), "2000-01-01T00:00:00Z");
	expect_eq_str("...and with the zone left off",
			DateTime().toString(UtcOffset::hours(10), IsoNoZone|IsoPunctuate), "2000-01-01T10:00:00");

	// The date and the time of day, which the conversion can return separately
	Interval	time_of_day;
	Gregorian	noonish = DateTime::fromTicks(TicksPerDay/2).asGregorian(UtcOffset(), &time_of_day);
	expect_eq_str("half a day in is midday", noonish.toString(), "2000-01-01T12:00:00");
	expect_eq_int("...and the time since midnight is half a day",
			(long)time_of_day.ticks(), TicksPerDay/2);

	// A zone applies to the civil time, so it round trips from either side of
	// the year, which is where an offset taken from "now" would disagree
	Gregorian	winter(2002,1,3,11,12,13);
	Gregorian	summer(2002,7,3,11,12,13);

	expect_eq_str("a date in January reads back at +10:00",
			winter.asDateTime(UtcOffset::hours(10)).asGregorian(UtcOffset::hours(10)).toString(),
			"2002-01-03T11:12:13");
	expect_eq_str("a date in July reads back at +10:00",
			summer.asDateTime(UtcOffset::hours(10)).asGregorian(UtcOffset::hours(10)).toString(),
			"2002-07-03T11:12:13");
	expect_eq_int("both were adjusted by exactly ten hours earlier",
			(long)(summer.asDateTime(UtcOffset::hours(10)) - summer.asDateTime()).ticks(),
			-36000*TicksPerSecond);
	expect_eq_int("...as was the winter one",
			(long)(winter.asDateTime(UtcOffset::hours(10)) - winter.asDateTime()).ticks(),
			-36000*TicksPerSecond);

	// A text carries its zone, or is read as UTC
	UtcOffset	zone;
	ErrNum		err = 0;
	DateTime	east = DateTime::fromString("2002-01-03T11:12:13+10:00", &zone, &err);

	expect_eq_err("a text with a zone reads", err, 0);
	expect_eq_int("...and returns that zone", zone.asMinutes(), 600);
	expect_eq_str("...and the instant it names is the one it says",
			east.toString(), "2002-01-03T01:12:13Z");
	expect_eq_str("a text with no zone is UTC",
			DateTime::fromString("2002-01-03T11:12:13").toString(), "2002-01-03T11:12:13Z");
	expect_eq_str("...and either way it round trips",
			east.toString(zone), "2002-01-03T11:12:13+10:00");
}

/*
 * The null value: a time that was never set. It is the lowest tick, the one
 * with no positive equivalent, so no value can be negated into one. Using it
 * where a value is needed reports rather than returning something that looks
 * real, and it spreads through a computation instead of becoming a number.
 */
void
null_tests()
{
	test_group("A null time");

	Interval	null_interval(NullTick);
	DateTime	null_datetime(NullTick);

	expect("a null interval says so", null_interval.isNull());
	expect("...and so does a null instant", null_datetime.isNull());
	expect("the epoch is not null", !DateTime().isNull());
	expect("...nor is a zero interval", !Interval().isNull());

	// The smallest time that exists is a value, and one tick below it is not
	expect("the smallest interval is a value", !Interval(DateTime::MinTicks).isNull());
	expect("...and one tick below it is null", Interval(DateTime::MinTicks-1).isNull());

	// It says so as text, and reads back from the same text
	expect_eq_str("a null interval is written as null", null_interval.toString(), "null");
	expect_eq_str("...and a null instant", null_datetime.toString(), "null");
	expect_eq_str("...and null milliseconds", Milliseconds(NullTick).toString(), "null");
	expect("...and a null interval reads back", Interval::fromString("null").isNull());
	expect("...as does a null instant", DateTime::fromString("null").isNull());
	expect("...and null seconds", Seconds::fromString("null").isNull());

	// A civil date has no null: a text that says null is not a date
	ErrNum	err = 0;
	Gregorian::fromString("null", 0, &err);
	expect_eq_err("a Gregorian cannot be null, so that text is refused",
			err, TIMERR_INVALID_TEXT);

	/*
	 * Arithmetic on a null reports, and returns another null rather than a
	 * number. The report goes to the error buffer, an operator having nowhere
	 * to return one from; the checkpoint takes it back so that the rest of
	 * this test is not looking at it.
	 */
	ErrBuf::MsgSequence	at = ErrCheckpoint();
	Interval	sum = null_interval + Interval(1);
	int		reported = (int)ErrBuffer()->count();
	ErrRollback(at);
	expect("adding to a null returns a null", sum.isNull());
	expect("...and reports", reported > 0);

	at = ErrCheckpoint();
	DateTime	moved = null_datetime + Seconds(5);
	reported = (int)ErrBuffer()->count();
	ErrRollback(at);
	expect("a null instant with an interval added is still null", moved.isNull());
	expect("...and reports", reported > 0);

	at = ErrCheckpoint();
	DateTime	never = DateTime() - null_interval;
	reported = (int)ErrBuffer()->count();
	ErrRollback(at);
	expect("a real instant moved by a null interval is null", never.isNull());
	expect("...and reports", reported > 0);

	at = ErrCheckpoint();
	Interval	between = DateTime() - null_datetime;
	reported = (int)ErrBuffer()->count();
	ErrRollback(at);
	expect("the interval between a real instant and a null one is null", between.isNull());
	expect("...and reports", reported > 0);

	/*
	 * And reading one as a value reports too, where the result would have to
	 * be a value: a count of seconds, or a date.
	 */
	at = ErrCheckpoint();
	time_t		seconds = null_datetime.asTime_t();
	reported = (int)ErrBuffer()->count();
	ErrRollback(at);
	expect_eq_int("a null instant read as a time_t returns zero", (long)seconds, 0);
	expect("...and reports", reported > 0);

	at = ErrCheckpoint();
	Gregorian	date = null_datetime.asGregorian();
	reported = (int)ErrBuffer()->count();
	ErrRollback(at);
	expect("a null instant read as a date has no date", !date.hasDate());
	expect("...and reports", reported > 0);

	/*
	 * A null that stays null invents no value, so it is not reported: a unit
	 * conversion of a null is another null. Neither is a comparison, which
	 * needs no value - a null is equal to a null, and sorts before every real
	 * time, the null tick being the lowest there is.
	 */
	expect("a null interval is still null as milliseconds",
			null_interval.asMilliseconds().isNull());
	expect("...and as seconds", null_interval.asSeconds().isNull());
	expect("...and a null instant as an interval", Interval(null_interval).isNull());
	expect("...and null milliseconds as an interval",
			Milliseconds(NullTick).asInterval().isNull());

	expect("a null interval equals a null interval", null_interval == Interval(NullTick));
	expect("...and is not equal to a real one", null_interval != Interval(0));
	expect("...and sorts before every real interval",
			null_interval < Interval(DateTime::MinTicks));
	expect("...and a null instant before every real one", null_datetime < DateTime());
}

/*
 * The one thing reported since the buffer was last cleared, rendered from the
 * default text and its parameters, and then cleared away. What was reported is
 * the only way to tell an overflow from a null operand: both return a null, and
 * only the report says which it was.
 */
static StrVal
reported_text()
{
	ErrBuf*	buf = ErrBuffer();
	StrVal	said;

	if (buf && buf->count() > 0)
	{
		{
			ErrBuf::Message	msg = buf->message(0);
			said = StrVal::format(msg.default_text, msg.parameters);
		}
		buf->clear();	// Only once the message has been let go
	}
	return said;
}

/*
 * An operation whose result will not fit, which used to be answered with a
 * wrapped count: a duration of the wrong sign, or an instant in the wrong
 * century, with nothing about it to say so. Each of these now reports and
 * returns a null, and the report is what says which of the two failures it was.
 */
void
overflow_tests()
{
	test_group("A result that will not fit");

	ErrBuffer()->clear();

	// The smallest interval that exists, less one tick: that is the null tick
	// exactly, so before this it quietly became "no value" with no report.
	Interval	smallest(DateTime::MinTicks);
	Interval	slipped = smallest - Interval(1);
	expect("the smallest interval less one tick is a null", slipped.isNull());
	expect_eq_str("...reported as a result past the range", reported_text(),
			"The result of subtracting is past the range of `Interval`");

	// Two ticks past it used to wrap to MaxTicks: an instant in 4922 AD
	slipped = smallest - Interval(2);
	expect("...and two ticks past it is a null too", slipped.isNull());
	expect_eq_str("...reported, and not wrapped", reported_text(),
			"The result of subtracting is past the range of `Interval`");

	slipped = Interval(DateTime::MaxTicks) + Interval(1);
	expect("the largest interval plus one tick is a null", slipped.isNull());
	expect_eq_str("...reported", reported_text(),
			"The result of adding is past the range of `Interval`");

	// The other two units, in their own units
	expect("seconds past their range are a null",
			(Seconds(DateTime::MaxTicks) + Seconds(1)).isNull());
	expect_eq_str("...reported in seconds", reported_text(),
			"The result of adding is past the range of `Seconds`");
	expect("...and milliseconds too",
			(Milliseconds(DateTime::MinTicks) - Milliseconds(1)).isNull());
	expect_eq_str("...reported in milliseconds", reported_text(),
			"The result of subtracting is past the range of `Milliseconds`");

	test_group("An instant moved past the ends of the range");

	DateTime	earliest = DateTime::fromTicks(DateTime::MinTicks);
	DateTime	latest = DateTime::fromTicks(DateTime::MaxTicks);

	expect("the earliest instant less a tick is a null",
			(earliest - Interval(1)).isNull());
	expect_eq_str("...reported as an instant past the range", reported_text(),
			"The result of subtracting is past the range of `DateTime`");
	expect("...and the latest plus a tick", (latest + Interval(1)).isNull());
	expect_eq_str("...reported", reported_text(),
			"The result of adding is past the range of `DateTime`");

	/*
	 * The span of the whole range is more than a Tick holds - 5845 years
	 * against 2922 either side - so the interval between its ends does not fit,
	 * and used to come out negative: an interval that says "before" for a
	 * subtraction that means "after".
	 */
	expect("the span of the whole range does not fit an interval",
			(latest - earliest).isNull());
	expect_eq_str("...reported", reported_text(),
			"The result of subtracting is past the range of `Interval`");

	test_group("A date read past the end of the range");

	/*
	 * The zone is applied to the tick count before the date is read, so an
	 * instant within 18 hours of an end of the range, read in a zone that takes
	 * it past, used to return a date from the other end: a 924 BC instant
	 * became 4922-10-08 with a zone designator on it, which is as plausible as
	 * a wrong result gets.
	 */
	expect("the latest instant read 18 hours east has no date",
			!latest.asGregorian(UtcOffset::hours(18)).hasDate());
	expect_eq_str("...reported as a date past the range", reported_text(),
			"The result of reading a date in that zone is past the range of `DateTime`");
	expect("the earliest instant read 18 hours west has no date",
			!earliest.asGregorian(UtcOffset::hours(-18)).hasDate());
	expect_eq_str("...reported", reported_text(),
			"The result of reading a date in that zone is past the range of `DateTime`");

	// ...and the time of day that the conversion could not return is a null,
	// and its text carries no zone, having no date to put in one
	{
		Interval	time_of_day;

		latest.asGregorian(UtcOffset::hours(18), &time_of_day);
		expect("...and its time of day is a null", time_of_day.isNull());
		reported_text();
		expect_eq_str("...and its text is a time with no zone on it",
				latest.toString(UtcOffset::hours(18)), "00:00:00");
		// Rendering it reports too: the failure is in the conversion, which
		// toString() goes through, and not in the writing of the text
		expect_eq_str("...having reported the conversion that failed", reported_text(),
				"The result of reading a date in that zone is past the range of `DateTime`");
	}

	// An instant well inside the range reads in any zone, and reports nothing:
	// it is only the ends that a zone can push past
	expect("an instant well inside the range reads in any zone",
			earliest.asGregorian(UtcOffset::hours(18)).hasDate());
	expect_eq_str("...and reports nothing", reported_text(), "");

	test_group("A conversion that would not fit");

	expect("a count of seconds too large for an interval is a null",
			Seconds(100000000000000LL).asInterval().isNull());
	expect_eq_str("...reported as a conversion past the range", reported_text(),
			"The result of converting from seconds is past the range of `Interval`");
	expect("...and so is one constructed from it",
			Interval(Seconds(100000000000000LL)).isNull());
	expect_eq_str("...reported", reported_text(),
			"The result of converting from seconds is past the range of `Interval`");

	expect("a count of seconds too large for milliseconds is a null",
			Seconds(40000000000000000LL).asMilliseconds().isNull());
	expect_eq_str("...reported", reported_text(),
			"The result of converting from seconds is past the range of `Milliseconds`");
	expect("...and a count of milliseconds too large for an interval",
			Milliseconds(100000000000000LL).asInterval().isNull());
	expect_eq_str("...reported", reported_text(),
			"The result of converting from milliseconds is past the range of `Interval`");

	test_group("A time_t outside the range");

	// A time_t from a corrupt file or a wrong-width field: a million million
	// seconds answered 2033-06-30T17:10:05Z before this
	expect("a time_t of a million million has no instant",
			DateTime::fromTime_t(1000000000000000LL).isNull());
	expect_eq_str("...reported", reported_text(),
			"The result of reading a time_t is past the range of `DateTime`");

	expect("a real time_t still converts", !DateTime::fromTime_t(1234567890).isNull());
	expect_eq_str("...and reports nothing", reported_text(), "");
	expect("the most negative time_t is refused rather than wrapped",
			DateTime::fromTime_t((time_t)(-9223372036854775807LL - 1)).isNull());
	reported_text();
}
