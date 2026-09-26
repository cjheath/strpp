/*
 * The Gregorian class: the civil calendar, the day numbers it counts by, and
 * the ISO 8601 texts it reads and writes.
 *
 * The dates here are the ones that catch a calendar out: a leap day in a
 * century that is not a leap year, a day that does not exist in its month, the
 * first and last days of the range a DateTime can name, and the years before
 * 1 AD, where the count this class uses (1 BC is -1, with no year 0) meets the
 * astronomical count ISO 8601 writes (1 BC is 0000).
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<gregorian.h>
#include	<errbuf.h>

#include	<cstdio>
#include	<cstring>

static bool	show_passes = false;
static int	test_count;
static int	failure_count;
static const char*	new_group;

void		field_tests();
void		day_number_tests();
void		era_tests();
void		parse_tests();
void		format_tests();
void		order_tests();
void		day_number_range_tests();


int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	field_tests();
	day_number_tests();
	era_tests();
	parse_tests();
	format_tests();
	order_tests();
	day_number_range_tests();

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

/*
 * Which field sets are dates that exist. The legacy class could only find out
 * by converting to a count of days and back; here the fields are the only
 * representation there is, so they are checked as themselves.
 */
void
field_tests()
{
	test_group("A date that exists, and one that does not");

	expect("a plain date is valid", Gregorian(2002,1,3,11,12,13).isValid());
	expect("the 29th of February in a leap year is valid", Gregorian(2000,2,29).isValid());
	expect("...but not in a century that is not a leap year", !Gregorian(1900,2,29).isValid());
	expect("...though 2000 is one", Gregorian::isLeapYear(2000));
	expect("...and 1900 is not", !Gregorian::isLeapYear(1900));
	expect("February the 31st is not a date", !Gregorian(2002,2,31).isValid());
	expect("...nor April the 31st", !Gregorian(2000,4,31).isValid());
	expect("...nor December the 32nd", !Gregorian(2000,12,32).isValid());
	expect("...nor a day zero", !Gregorian(2000,1,0).isValid());
	expect("...nor a month zero", !Gregorian(2000,0,1).isValid());
	expect("...nor a month thirteen", !Gregorian(2000,13,1).isValid());
	expect("no hour 24", !Gregorian(2000,1,1,24,0,0).isValid());
	expect("no minute 60", !Gregorian(2000,1,1,0,60,0).isValid());
	expect("no second 60: leap seconds are not represented",
			!Gregorian(2000,1,1,0,0,60).isValid());
	expect("the last tick of a second is valid",
			Gregorian(2000,1,1,0,0,0,TicksPerSecond-1).isValid());
	expect("...and one past it is not",
			!Gregorian(2000,1,1,0,0,0,TicksPerSecond).isValid());

	test_group("A field set, checked and reported");

	ErrNum	err = 0;
	Gregorian	good = Gregorian::fromYMD(2002,1,3,11,12,13,0,&err);
	expect_eq_err("a date that exists is reported as no error", err, 0);
	expect_eq_str("...and is the date it was given", good.toString(), "2002-01-03T11:12:13");

	err = 0;
	Gregorian	bad = Gregorian::fromYMD(2002,2,31,11,12,13,0,&err);
	expect_eq_err("February the 31st is reported", err, TIMERR_INVALID_YMDHMS);
	expect("...and the value says it is not valid", !bad.isValid());

	err = 0;
	bad.asDateTime(UtcOffset(), &err);
	expect_eq_err("...and cannot become an instant either", err, TIMERR_INVALID_YMDHMS);

	test_group("Days in a month");

	expect_eq_int("January has 31 days", Gregorian::daysInMonth(2002,1), 31);
	expect_eq_int("February has 28 in a common year", Gregorian::daysInMonth(2002,2), 28);
	expect_eq_int("...and 29 in a leap year", Gregorian::daysInMonth(2000,2), 29);
	expect_eq_int("...and 28 in 1900", Gregorian::daysInMonth(1900,2), 28);
	expect_eq_int("...and 29 in 2000", Gregorian::daysInMonth(2000,2), 29);
	expect_eq_int("...and 29 in 0 BC, which was a leap year",
			Gregorian::daysInMonth(-1,2), 29);		// -1 is 1 BC, astronomical year 0
	expect_eq_int("a month that is not one has no days", Gregorian::daysInMonth(2002,13), 0);
}

/*
 * The day numbers, which must still mean the same thing they did in the
 * calendar this replaces: day 1 is 1/1/0001, and the day before it is day 0.
 */
void
day_number_tests()
{
	test_group("Day numbers and the days of the week");

	expect_eq_int("1/1/0001 is day 1", (long)Gregorian(1,1,1).dayNumber(), 1);
	/*
	 * Day 0 is 0/12/31, and in this calendar that day is 1 BC's last: the
	 * year the BC/AD count has not got is 1 BC itself, which is -1 here, and
	 * which year 0 is reserved for a time of day instead. Asking a year-0
	 * value for a day number answers 0, but it is not on day 0 - it is not on
	 * any day at all.
	 */
	expect_eq_int("...and 0/12/31, which is 1 BC's last day, is day 0",
			(long)Gregorian(-1,12,31).dayNumber(), 0);
	expect("a year-0 value is a time of day, and not a date",
			!Gregorian(0,12,31).hasDate());
	expect_eq_int("2000-01-01 is day 730120", (long)Gregorian(2000,1,1).dayNumber(), 730120);
	expect_eq_int("1970-01-01 is day 719163", (long)Gregorian(1970,1,1).dayNumber(), 719163);
	expect_eq_int("9999-12-31 is day 3652059", (long)Gregorian(9999,12,31).dayNumber(), 3652059);

	expect_eq_int("1969-07-20 was a Sunday", Gregorian(1969,7,20).dayOfWeek(), 0);
	expect_eq_int("1970-01-01 was a Thursday", Gregorian(1970,1,1).dayOfWeek(), 4);
	expect_eq_int("2000-01-01 was a Saturday", Gregorian(2000,1,1).dayOfWeek(), 6);
	expect_eq_int("2002-01-03 was a Thursday", Gregorian(2002,1,3).dayOfWeek(), 4);

	expect_eq_int("a day's date is its own day number read back",
			(long)Gregorian::fromDayNumber(730120).dayNumber(), 730120);
	expect_eq_int("...and so is one before the epoch",
			(long)Gregorian::fromDayNumber(719163).dayNumber(), 719163);
	expect_eq_int("...and the first of the year 1",
			(long)Gregorian::fromDayNumber(1).dayNumber(), 1);

	test_group("Days of the year");

	expect_eq_int("the first of January is day 1", Gregorian(2002,1,1).dayOfYear(), 1);
	expect_eq_int("the first of March in a leap year is day 61", Gregorian(2000,3,1).dayOfYear(), 61);
	expect_eq_int("...and in a common year 60", Gregorian(1900,3,1).dayOfYear(), 60);
	expect_eq_int("the last of December in a leap year is day 366", Gregorian(2000,12,31).dayOfYear(), 366);
	expect_eq_int("...and in a common year 365", Gregorian(1900,12,31).dayOfYear(), 365);

	test_group("A time of day is not on a day");

	Gregorian	noon = Gregorian::fromString("12:34");
	expect("a time of day has no date", !noon.hasDate());
	expect("...but is a valid value", noon.isValid());
	expect_eq_int("...and its day number is none", (long)noon.dayNumber(), 0);
	expect_eq_int("...and it has no day of the week", noon.dayOfWeek(), -1);
	expect_eq_int("...nor a day of the year", noon.dayOfYear(), 0);
	expect_eq_int("...but it knows the time since midnight",
			(long)noon.timeSinceMidnight().ticks(), (12*3600+34*60)*TicksPerSecond);
	expect_eq_str("...and says so as text", noon.toString(), "12:34:00");

	ErrNum	err = 0;
	noon.asDateTime(UtcOffset(), &err);
	expect_eq_err("a time of day is refused as an instant", err, TIMERR_NO_DATE);
}

/*
 * The years before 1 AD: the count this class uses, where 1 BC is -1 and year
 * 0 means a time of day, against the astronomical count ISO 8601 writes, where
 * 1 BC is 0000.
 */
void
era_tests()
{
	test_group("The years before 1 AD");

	expect_eq_int("1 BC is year -1", Gregorian::fromString("0000-01-01").year(), -1);
	expect_eq_int("...and 2 BC is year -2", Gregorian::fromString("-0001-01-01").year(), -2);
	expect_eq_str("...and year -1 is written 0000",
			Gregorian(-1,1,1).toString(IsoPunctuate|IsoDateOnly), "0000-01-01");
	expect_eq_str("...and year -2 is written -0001",
			Gregorian(-2,1,1).toString(IsoPunctuate|IsoDateOnly), "-0001-01-01");

	expect_eq_int("1/1/0001 is day 1", (long)Gregorian(1,1,1).dayNumber(), 1);
	expect_eq_int("1 BC, being the year before, is a year of 366 days",
			(long)(Gregorian(1,1,1).dayNumber() - Gregorian(-1,1,1).dayNumber()), 366);
	expect_eq_int("...so 1/1/1 BC is day -365", (long)Gregorian(-1,1,1).dayNumber(), -365);
	expect_eq_int("...and 1/1/2 BC is day -730", (long)Gregorian(-2,1,1).dayNumber(), -730);

	// The first day the range can name, which the legacy code returned a
	// negative day of the week for, and could not name at all
	Gregorian	first = Gregorian::fromDayNumber(DateTime::MinDay);
	expect_eq_int("the first day of the range is in 924 BC", first.year(), -924);
	expect_eq_int("...on the 25th of March", first.day(), 25);
	expect_eq_int("...and that was a Sunday", first.dayOfWeek(), 0);
	expect_eq_str("...which reads back as ISO 8601's expanded year",
			first.toString(IsoPunctuate|IsoDateOnly), "-0923-03-25");

	expect_eq_int("a date in BC converts to a day and back",
			(long)Gregorian(-924,3,25).dayNumber(), DateTime::MinDay);
	expect("...with its fields intact", Gregorian(-924,3,25).isValid());
}

/*
 * Reading ISO 8601: every form accepted, and every one refused.
 */
void
parse_tests()
{
	test_group("Reading a date, a time, and both");

	ErrNum		err = 0;
	UtcOffset	zone;

	expect_eq_str("a punctuated date and time",
			Gregorian::fromString("2002-01-03T11:12:13", 0, &err).toString(), "2002-01-03T11:12:13");
	expect_eq_err("...reads with nothing reported", err, 0);
	expect_eq_str("a space where ISO has its T",
			Gregorian::fromString("2002-01-03 11:12:13").toString(), "2002-01-03T11:12:13");
	expect_eq_str("an unpunctuated date",
			Gregorian::fromString("20020103").toString(), "2002-01-03T00:00:00");
	expect_eq_str("an unpunctuated date and time",
			Gregorian::fromString("20020103T111213").toString(), "2002-01-03T11:12:13");
	expect_eq_str("a date without its seconds",
			Gregorian::fromString("2002-01-03T11:12").toString(), "2002-01-03T11:12:00");
	expect_eq_str("a year and its month",
			Gregorian::fromString("2002-01").toString(), "2002-01-01T00:00:00");
	expect_eq_str("a time alone, which the legacy refused",
			Gregorian::fromString("12:34").toString(), "12:34:00");
	expect_eq_str("...and its unpunctuated form",
			Gregorian::fromString("123456").toString(), "12:34:56");

	test_group("Reading the fraction of a second");

	expect_eq_str("three digits are tenths, hundredths and thousandths",
			Gregorian::fromString("2002-01-03T11:12:13.123").toString(IsoPunctuate|3),
			"2002-01-03T11:12:13.123");
	expect_eq_str("a comma is a radix too",
			Gregorian::fromString("2002-01-03T11:12:13,123").toString(IsoPunctuate|3),
			"2002-01-03T11:12:13.123");
	expect_eq_int("...and the ticks are the fraction of a second",
			(long)Gregorian::fromString("2002-01-03T11:12:13.123").fraction(), 12300000);
	expect_eq_int("eight digits are all used",
			(long)Gregorian::fromString("2002-01-03T11:12:13.12345678").fraction(), 12345678);
	expect_eq_int("...and a ninth is past what this resolution holds",
			(long)Gregorian::fromString("2002-01-03T11:12:13.123456789").fraction(), 12345678);

	test_group("Reading a zone");

	expect_eq_int("Z is UTC", Gregorian::fromString("2002-01-03T11:12:13Z", &zone).hour(), 11);
	expect_eq_int("...at no offset", zone.asMinutes(), 0);
	Gregorian::fromString("2002-01-03T11:12:13+10:00", &zone);
	expect_eq_int("an offset east is read", zone.asMinutes(), 600);
	Gregorian::fromString("2002-01-03T11:12:13-05:30", &zone);
	expect_eq_int("an offset west with half an hour", zone.asMinutes(), -330);
	Gregorian::fromString("2002-01-03T11:12:13-0530", &zone);
	expect_eq_int("an offset with no punctuation", zone.asMinutes(), -330);
	err = 0;
	Gregorian::fromString("2002-01-03T11:12:13+19:00", 0, &err);
	expect_eq_err("no zone is further out than 18 hours", err, TIMERR_INVALID_TEXT);
	Gregorian::fromString("2002-01-03T11:12:13+18:00", 0, &err);
	expect_eq_err("...and 18 hours is one", err, 0);

	test_group("What is not a date or a time");

	err = 0;
	Gregorian::fromString("half past three", 0, &err);
	expect_eq_err("words are not a time", err, TIMERR_INVALID_TEXT);
	err = 0;
	Gregorian::fromString("12x34", 0, &err);
	expect_eq_err("a time needs its punctuation", err, TIMERR_INVALID_TEXT);
	err = 0;
	Gregorian::fromString("2002", 0, &err);
	expect_eq_err("four digits alone could be a year or a time, and are refused", err, TIMERR_INVALID_TEXT);
	err = 0;
	Gregorian::fromString("2002-1-3", 0, &err);
	expect_eq_err("a month and a day are two digits each", err, TIMERR_INVALID_TEXT);
	err = 0;
	Gregorian::fromString("2002-01-03T11:12:13 and more", 0, &err);
	expect_eq_err("nothing may follow", err, TIMERR_INVALID_TEXT);
	err = 0;
	Gregorian::fromString("2002-02-31", 0, &err);
	expect_eq_err("a day that does not exist in its month", err, TIMERR_INVALID_TEXT);
	err = 0;
	Gregorian::fromString("2002-01-03T24:00:00", 0, &err);
	expect_eq_err("no hour 24", err, TIMERR_INVALID_TEXT);
	err = 0;
	Gregorian::fromString("", 0, &err);
	expect_eq_err("nothing at all", err, TIMERR_INVALID_TEXT);

	test_group("Reading what was written");

	// Only a text the writer can produce reads back as itself: a fraction is
	// written to the digits it was given, and no more
	const char*	texts[] = {
		"2002-01-03T11:12:13",
		"2002-01-03T11:12:00",
		"2002-01-03T00:00:00",
		"2002-01-03T11:12:13.5",
		"12:34:56",
		"0000-01-01T00:00:00",
		"-0001-01-01T00:00:00",
		"-0923-03-25T00:00:00",
		"1970-01-01T00:00:00",
		0
	};

	for (int n = 0; texts[n]; n++)
	{
		const char*	full_stop = strchr(texts[n], '.');
		int		digits = full_stop ? (int)strlen(full_stop+1) : 0;

		expect_eq_str("a text reads back as it was written",
				Gregorian::fromString(texts[n]).toString(IsoPunctuate|digits), texts[n]);
	}
}

/*
 * Writing ISO 8601: the flags, alone and together.
 */
void
format_tests()
{
	test_group("Writing a date and a time");

	Gregorian	when(2002,1,3,11,12,13,12300000);

	expect_eq_str("the extended form is the default",
			when.toString(), "2002-01-03T11:12:13");
	expect_eq_str("the basic form has no punctuation",
			when.toString(0), "20020103T111213");
	expect_eq_str("a space instead of the T",
			when.toString(IsoPunctuate|IsoSpaceForT), "2002-01-03 11:12:13");
	expect_eq_str("the date alone",
			when.toString(IsoPunctuate|IsoDateOnly), "2002-01-03");
	expect_eq_str("...in the basic form too",
			when.toString(IsoDateOnly), "20020103");
	expect_eq_str("and the fraction, to the digits asked for",
			when.toString(IsoPunctuate|3), "2002-01-03T11:12:13.123");
	expect_eq_str("...or to all eight",
			when.toString(IsoPunctuate|8), "2002-01-03T11:12:13.12300000");
	expect_eq_str("a date alone carries no fraction",
			when.toString(IsoPunctuate|IsoDateOnly|8), "2002-01-03");

	expect_eq_str("a time of day is written without a date",
			Gregorian::fromString("12:34").toString(), "12:34:00");
	expect_eq_str("a year alone is the first of its January",
			Gregorian::fromString("2002-01").toString(IsoPunctuate|IsoDateOnly), "2002-01-01");
}

/*
 * The legacy class kept two representations and a cache between them, so what
 * a getter answered could depend on which getter had been called first. There
 * is one representation here, and this is the test that says a value cannot
 * answer differently because of the order it was asked in.
 */
void
order_tests()
{
	test_group("A value does not depend on the order it is read in");

	Gregorian	a(2002,1,3,11,12,13,12300000);
	Gregorian	b(a);

	int		wd_a = a.dayOfWeek();
	int		wd_b = b.dayOfWeek();
	StrVal		first = a.toString(IsoPunctuate|8);
	long		day_a = (long)a.dayNumber();
	long		day_b = (long)b.dayNumber();
	StrVal		second = a.toString(IsoPunctuate|8);

	expect_eq_int("the day of the week is the same either way", wd_a, wd_b);
	expect_eq_int("...and the day number", day_a, day_b);
	expect_eq_str("...and the text", first, second.asUTF8());
	expect("...and the value is unchanged by having been read", a == b);

	// Read in the other order, and through the conversions
	Gregorian	c(2002,1,3,11,12,13,12300000);
	StrVal		text_first = c.toString();
	long		day_c = (long)c.dayNumber();
	ErrNum		err = 0;
	DateTime	instant = c.asDateTime(UtcOffset(), &err);

	expect_eq_err("converting to an instant reports nothing", err, 0);
	expect_eq_int("...and does not disturb the day number", (long)c.dayNumber(), day_c);
	expect_eq_str("...nor the text", c.toString(), text_first.asUTF8());
	expect("...and the instant reads back as the same date",
			instant.asGregorian().toString() == text_first);
	expect("...and as a value equal to the one it came from", instant.asGregorian() == c);
}

/*
 * The one thing reported since the buffer was last cleared, rendered and then
 * cleared away - the only way to tell a conversion that could not answer from
 * one that answered "no date" for another reason.
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
 * A day number the calendar cannot name. This used to answer a date that never
 * was - 32269-07-04 for a million million days - and isValid() said it was a
 * valid one, so a caller who checked was told nothing was wrong.
 */
void
day_number_range_tests()
{
	test_group("A day number outside the calendar");

	ErrBuffer()->clear();

	Gregorian	far_away = Gregorian::fromDayNumber(1000000000000000LL);
	expect("a day number a million million days out has no date", !far_away.hasDate());
	expect("...and is not a valid date that it could hide behind", far_away.isValid());
	expect_eq_str("...reported as a result past the range", reported_text(),
			"The result of converting a day number is past the range of `Gregorian`");

	// A year that will not fit the fields, though the day number itself is one
	// the calendar could count to
	far_away = Gregorian::fromDayNumber(12000000);
	expect("a year past what the fields hold has no date", !far_away.hasDate());
	expect_eq_str("...reported", reported_text(),
			"The result of converting a day number is past the range of `Gregorian`");

	// A time of day past the end of its day is folded into it, so the count of
	// ticks means what it says
	Gregorian	next_day = Gregorian::fromDayNumber(730120, TicksPerDay + 5*3600*TicksPerSecond);
	expect_eq_str("five hours into the next day is five o'clock on it",
			next_day.toString(), "2000-01-02T05:00:00");
	expect_eq_str("...and reports nothing", reported_text(), "");

	// ...and the last day the fields can hold still reads: the boundary is where
	// the year stops fitting a short, which the calendar itself can compute
	{
		Tick	last_day = Gregorian(32767,12,31).dayNumber();

		expect("the last day the fields hold still reads",
				Gregorian::fromDayNumber(last_day).hasDate());
		expect_eq_int("...as the year it names",
				(long)Gregorian::fromDayNumber(last_day).year(), 32767);
		expect_eq_str("...and reports nothing", reported_text(), "");

		expect("the day after it does not",
				!Gregorian::fromDayNumber(last_day+1).hasDate());
		expect_eq_str("...and says so", reported_text(),
				"The result of converting a day number is past the range of `Gregorian`");
	}
}
