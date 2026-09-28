#include	"memory_monitor.h"
#include	<variant.h>
#include	<errbuf.h>		// The child reads its own error buffer

#include	<csignal>
#include	<fcntl.h>
#include	<unistd.h>
#include	<sys/wait.h>

static int	test_count = 0;
static int	failure_count = 0;

/*
 * The one way out, for everything this test says: the text is a StrVal, and is
 * written in as many calls as it takes. Nothing here uses printf.
 */
static void
say(StrVal text)
{
	const char*	cp = text.asUTF8();
	ssize_t		bytes = text.numBytes();
	while (bytes > 0)
	{
		ssize_t	written = write(1, cp, bytes);
		if (written <= 0)
			break;			// Nothing more can be done about it
		cp += written;
		bytes -= written;
	}
}

static void
expect(const StrVal& when, bool ok)
{
	test_count++;
	if (ok)
		return;

	failure_count++;
	say(StrVal::format("{1}:\t{2}: FAIL\n", Variant((int)test_count) << when));
}

static void
expect_eq_str(const StrVal& when, const StrVal& got, const char* wanted)
{
	test_count++;
	if (got == wanted)
		return;

	failure_count++;
	say(StrVal::format("{1}:\t{2}: FAIL\n\twanted: {3}\n\tgot:    {4}\n",
		Variant((int)test_count) << when << Variant(wanted) << got));
}

static void
expect_eq_int(const StrVal& when, long got, long wanted)
{
	test_count++;
	if (got == wanted)
		return;

	failure_count++;
	say(StrVal::format("{1}:\t{2}: FAIL\n\twanted: {3}\n\tgot:    {4}\n",
		Variant((int)test_count) << when << wanted << got));
}

/*
 * A check where an assertion stood: it is counted and reported like the rest
 * instead of killing the process, so one wrong expectation does not hide the
 * ones behind it. The expression is its own name.
 */
#define	CHECK(expr)	expect(#expr, (expr))

void variant_array_tests();
void variant_tests();
void unsigned_tests();
void time_variant_tests();

// The pipe a dying child's error buffer is written into
static int	report_fd = -1;

/*
 * The report is made before the assertion kills the process, so the only way to
 * read it is from inside the dying child. abort() raises SIGABRT, so a handler
 * for it can drain the buffer into the pipe first and then let abort finish.
 *
 * Each Message holds a slice of the buffer's parameter array, and delivered()
 * refuses while any slice is outstanding, so the Message must be gone before it
 * is called - hence the inner scope, as in strassert.cpp.
 */
static void
dump_error_buffer(int sig)
{
	ErrBuf*	buf = ErrBuffer();
	if (buf && report_fd >= 0)
		for (ErrBuf::MsgIndex i = 0; i < buf->count(); i++)
		{
			StrVal	line;
			{
				ErrBuf::Message	msg = buf->message(i);
				line = StrVal::fromInt32((int32_t)msg.error, 'X')+": "
					+ StrVal::format(msg.default_text, msg.parameters)+"\n";
			}
			(void)!write(report_fd, line.asUTF8(), line.numBytes());
			buf->delivered();
		}
	signal(sig, SIG_DFL);		// Return, and abort() finishes the job
}

/*
 * A coercion that must fail kills the process, so it is run in a child and
 * judged by how the child died and what it had reported - the same way
 * assert_test.cpp tests a failure of StrppAssert. What the child says on
 * standard error is sent nowhere, so the output stays clean.
 */
static bool
coercion_aborts(bool (*body)(), StrVal& report)
{
	int	fds[2];
	if (pipe(fds) != 0)
		return false;

	// Empty this process's buffered output before forking: abort() flushes
	// what a child inherited, so every line printed so far would come out
	// once per child as well
	fflush(stdout);

	pid_t	child = fork();
	if (child == 0)
	{
		close(fds[0]);
		int	devnull = open("/dev/null", O_WRONLY);
		if (devnull >= 0)
			dup2(devnull, 2);
		report_fd = fds[1];
		signal(SIGABRT, dump_error_buffer);
		body();				// Expected to die, not to return
		_exit(0);
	}

	close(fds[1]);
	char	buf[512];
	int	got = 0;
	int	n;
	while (got < 511 && (n = (int)read(fds[0], buf+got, 511-got)) > 0)
		got += n;
	buf[got] = '\0';
	close(fds[0]);
	report = StrVal(buf);

	int	status = 0;
	waitpid(child, &status, 0);
	return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

// Beyond what a signed int holds, so reading it as one would change the number
static bool	uint_does_not_fit_int()
{
	Variant	too_big(4000000000u);
	int	overflowed = too_big.as_int();
	(void)overflowed;
	return true;
}

// The same one width up: an unsigned long beyond LONG_MAX read as a long
static bool	ulong_does_not_fit_long()
{
	Variant	too_big(18000000000000000000ul);
	long	overflowed = too_big.as_long();
	(void)overflowed;
	return true;
}

// Beyond LONG_MAX as well, so no signed type holds it and as_signed() has none
// to choose. On a target where a long is as wide as a long long, the test above
// is already this case; it is written out anyway so that it is tested wherever.
static bool	ullong_does_not_fit_signed()
{
	Variant	too_big(18446744073709551615ull);
	long long	overflowed = too_big.as_signed();
	(void)overflowed;
	return true;
}

/*
 * Whether a report names the message expected, by its number - the 8-digit
 * prefix of "A0000802: ...". What a test wants to know is which message was
 * raised; the wording is not its business, and neither are the values, which
 * are searched for separately because those are values and not prose.
 */
static bool
same_code(StrVal report, const char* code)
{
	return report.substr(0, 8) == code;
}

int
main(int argc, const char** argv)
{
#if defined(MEMCHECK)
	start_recording_allocations();
#endif

	say(StrVal::format("sizeof(Variant) == {1}\n", Variant((long)sizeof(Variant))));
	say(StrVal::format("sizeof(StrRef) == {1}\n", Variant((long)sizeof(StrRef))));
	say(StrVal::format("sizeof(StrVal) == {1}\n", Variant((long)sizeof(StrVal))));
	say(StrVal::format("sizeof(StringArray) == {1}\n", Variant((long)sizeof(StringArray))));
	say(StrVal::format("sizeof(VariantArray) == {1}\n", Variant((long)sizeof(VariantArray))));
	say(StrVal::format("sizeof(StrVariantMap) == {1}\n", Variant((long)sizeof(StrVariantMap))));

	expect_eq_int("a Variant is one word of union and its type", sizeof(Variant), 24);
	expect("...no larger than a StrVal", sizeof(StrVal) == sizeof(Variant));
	expect("...while a StrRef, which is a body and an offset, is smaller",
		sizeof(StrRef) < sizeof(StrVal));

	/*
	 * The two time types share the widest word the union already had, so a
	 * Variant with a DateTime in it is no larger than one with a StrVal: 8
	 * bytes of union and the type. If this ever fails, a type was added that
	 * needed storage of its own.
	 */
	CHECK(sizeof(Variant) == 24);

	variant_array_tests();
	variant_tests();
	unsigned_tests();
	time_variant_tests();

#if defined(MEMCHECK)
	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();
#endif

	say(StrVal::format("{1} tests, {2} failures\n",
		Variant((int)test_count) << (int)failure_count));
	return failure_count == 0 ? 0 : 1;
}

/*
 * An array passed by value, and what it renders as: the expected rendering is
 * the parameter, so that each caller says what it is looking for.
 */
void variant_array_from_param(VariantArray a, const char* wanted)
{
	StrVal	rendered = Variant(a).as_json();
	say(StrVal::format("VariantArray from param = {1}\n", Variant(rendered)));
	expect_eq_str(StrVal("an array passed by value renders as ") + wanted, rendered, wanted);
}

void variant_array_tests()
{
	VariantArray	va;
	StringArray	sa;

	sa << "foo";
	sa << "bar";

	VariantArray	va2;
	va2 << 69;
	va2 << 81729L;

	StrVariantMap	map;
	map.put("fred", 23);
	map.put("fly", "boo");

	va << true;	// Gets cast to int
	va << 4;
	va << 4L;
	va << 8LL;
	va << "baz";
	va << sa;
	va << va2;
	va << map;
	StrVal	rendered = Variant(va).as_json();
	say(StrVal::format("VariantArray = {1}\n", Variant(rendered)));
	expect_eq_str("an array of every type it holds, rendered",
		rendered, "[ 1, 4, 4, 8, \"baz\", [ \"foo\", \"bar\" ], [ 69, 81729 ],"
			  " { \"fly\": \"boo\", \"fred\": 23 } ]");

	va.clear();

	va = Variant(23) << "appendage";	// Get an array by appending to a Variant
	va << 1234567890123456789LL;		// And again
	rendered = Variant(va).as_json();
	say(StrVal::format("VariantArray from append = {1}\n", Variant(rendered)));
	expect_eq_str("appending to a Variant makes an array of it and the addend",
		rendered, "[ 23, \"appendage\", 1234567890123456789 ]");

	VariantArray	na("boo");		// Construct from value coerced to Variant
	na << va[2];				// Append a 2nd value
	rendered = Variant(na).as_json();
	say(StrVal::format("VariantArray from element = {1}\n", Variant(rendered)));
	expect_eq_str("an array built from a value and an element",
		rendered, "[ \"boo\", 1234567890123456789 ]");

	// variant_array_from_param("bah");	// Unfortunately this can't be made to work
	variant_array_from_param(VariantArray("bah"), "[ \"bah\" ]");			// This works
	variant_array_from_param(VariantArray("baz") << 31, "[ \"baz\", 31 ]");		// and this
	variant_array_from_param(VariantArray() << "bah" << 47, "[ \"bah\", 47 ]");	// this too
	variant_array_from_param("bah" << Variant(53), "[ \"bah\", 53 ]");			// So does this
	variant_array_from_param(Variant(29), "[ 29 ]");

	/*
	 * The closing bracket of a nested structure stands at its *parent's*
	 * indent, one level less than the opening. Sizing it as "the separator
	 * less one level per depth" instead took the parent's indent away too,
	 * which looks right at the outermost level and is wrong everywhere else -
	 * and nothing here had covered the indented mode at depth until px's
	 * golden files caught it.
	 */
	{
		VariantArray	inner;
		inner << 2 << 3;
		VariantArray	outer;
		outer << 1 << inner;
		Variant		v(outer);

		CHECK(v.as_json(-2) == "[1,[2,3]]");		// Tight
		CHECK(v.as_json(-1) == "[ 1, [ 2, 3 ] ]");	// Compact: spaces, inside too
		CHECK(v.as_json(0) == "[\n  1,\n  [\n    2,\n    3\n  ]\n]");
		CHECK(v.as_json(1) == "[\n    1,\n    [\n      2,\n      3\n    ]\n  ]");
		say(StrVal("as_json: tight, compact and both indented forms close at the parent's indent\n"));
	}
}

/*
 * The unsigned types. Each shares its signed twin's storage, so what is worth
 * checking is that the value is neither lost nor reinterpreted, including where
 * a signed reading of those bits would have to be a different number.
 */
void unsigned_tests()
{
	// Const, so that the accessors used below are the ones that only read: the
	// mutable as_strval() coerces, and would leave these as Strings
	const Variant	u(4000000000u);			// Beyond what a signed int holds
	const Variant	ul(18000000000000000000ul);	// And beyond a signed long
	const Variant	ull(18446744073709551615ull);	// The largest there is

	say(StrVal::format("unsigned types: {1}, {2}, {3}\n",
		Variant(u.type_name()) << ul.type_name() << ull.type_name()));
	expect_eq_str("an unsigned value keeps its own type", u.type_name(), "UInteger");
	expect_eq_str("...one width up too", ul.type_name(), "ULong");
	expect_eq_str("...and the widest", ull.type_name(), "ULongLong");
	CHECK(u.type() == Variant::UInteger);
	CHECK(ul.type() == Variant::ULong);
	CHECK(ull.type() == Variant::ULongLong);

	CHECK(u.as_uint() == 4000000000u);
	CHECK(ul.as_ulong() == 18000000000000000000ul);
	CHECK(ull.as_ulonglong() == 18446744073709551615ull);

	// A signed reading of the same bits is a different number: these are the
	// digits of the unsigned value, which is what the types are for
	// A copy, because the const accessors only assert: it is the mutable ones
	// that coerce, and the coercion is what renders the unsigned digits
	Variant	mu(u), mul(ul), mull(ull);
	StrVal	us = mu.as_strval();
	StrVal	uls = mul.as_strval();
	StrVal	ulls = mull.as_strval();
	CHECK(us == "4000000000");
	CHECK(uls == "18000000000000000000");
	CHECK(ulls == "18446744073709551615");
	say(StrVal::format("as text: {1}, {2}, {3}\n", Variant(us) << uls << ulls));
	expect_eq_str("...which reads back as its own digits", us, "4000000000");

	CHECK(Variant(u).as_json() == "4000000000");
	CHECK(Variant(ull).as_json() == "18446744073709551615");

	// A copy carries the type and the value, and coerces as its signed twin
	// does. Widening goes by value, not by bits, and the type follows: after
	// the coercion it is a LongLong and reads as one.
	Variant	back(u);
	CHECK(back.as_longlong() == 4000000000LL);
	CHECK(back.type() == Variant::LongLong);

	// as_signed() returns the value in the closest signed type that holds it,
	// which is the read that does not have to be told the width
	{
		Variant	a(5u);
		CHECK(a.as_signed() == 5 && a.type() == Variant::Integer);
		Variant	b(u);				// 4000000000, beyond INT_MAX
		CHECK(b.as_signed() == 4000000000LL && b.type() == Variant::Long);
		Variant	c(5ul);
		CHECK(c.as_signed() == 5 && c.type() == Variant::Long);
		Variant	d(5ull);
		CHECK(d.as_signed() == 5 && d.type() == Variant::LongLong);
		Variant	e(47L);				// Already signed: answered as it stands
		CHECK(e.as_signed() == 47 && e.type() == Variant::Long);
		Variant	f("42");
		CHECK(f.as_signed() == 42 && f.type() == Variant::Integer);
		say(StrVal("as_signed: 5u as Integer, 4000000000u as Long, \"42\" as Integer\n"));
	}

	// A signed type of the same width cannot hold those values, so the coercion
	// is refused rather than reinterpreting the bits. Each of these dies, so
	// each is run in a child, which reports through the error buffer before it
	// goes. as_signed() dies for the last two as well: no signed type holds
	// them, so there is none for it to choose.
	StrVal	report;
	CHECK(coercion_aborts(uint_does_not_fit_int, report));
	CHECK(same_code(report, "A0000802")
			&& report.find(StrVal("4000000000")) >= 0
			&& report.find(StrVal("Integer")) >= 0);
	CHECK(coercion_aborts(ulong_does_not_fit_long, report));
	CHECK(same_code(report, "A0000802")
			&& report.find(StrVal("18000000000000000000")) >= 0
			&& report.find(StrVal("Long")) >= 0);
	CHECK(coercion_aborts(ullong_does_not_fit_signed, report));
	CHECK(same_code(report, "A0000802")
			&& report.find(StrVal("18446744073709551615")) >= 0
			&& report.find(StrVal("LongLong")) >= 0);
	say(StrVal("refused: a UInteger beyond INT_MAX, a ULong beyond LONG_MAX\n"));
	say(StrVal("...each naming the value it could not hold\n"));
}

/*
 * The two time types in a Variant. Neither is a number, so is_number() must not
 * be widened to take them in: a refusal of one is a type that does not convert,
 * not a value that does not fit, since no other value of that type would have
 * fared any better.
 */
static bool	interval_read_as_a_number()
{
	Variant	v(Interval(1));
	int	n = v.as_int();		// Reports, and asserts
	(void)n;
	return true;
}

static bool	datetime_read_as_an_interval()
{
	DateTime	epoch;
	Variant		v(epoch);
	Interval	i = v.as_interval();
	(void)i;
	return true;
}

static bool	interval_read_as_a_datetime()
{
	Variant		v(Interval(1));
	DateTime	d = v.as_datetime();
	(void)d;
	return true;
}

/*
 * The arm of coerce() that handles a target type with no case of its own. No
 * type Variant knows can reach it, since each has a case, so the test reaches
 * it through a subclass, which may call the protected member - and with a value
 * that is not a VariantType at all, which is what a type that was added and not
 * implemented looks like from inside.
 */
struct CoercibleVariant : public Variant
{
	CoercibleVariant(Variant v) : Variant(v) {}
	using Variant::coerce;			// Protected in Variant, public here
};

static bool	coercion_without_a_case()
{
	CoercibleVariant	v((Variant(Interval(1))));
	v.coerce((Variant::VariantType)99);	// Reports, and asserts
	return true;
}

void time_variant_tests()
{
	Variant	interval(Interval(-150000000));
	Variant	datetime(DateTime::fromTicks(0));

	CHECK(interval.type() == Variant::Interval);
	CHECK(StrVal(interval.type_name()) == "Interval");
	CHECK(interval.as_interval().ticks() == -150000000);

	CHECK(datetime.type() == Variant::DateTime);
	CHECK(StrVal(datetime.type_name()) == "DateTime");
	CHECK(datetime.as_datetime().ticks() == 0);

	// A copy carries the type and the value, as an assignment does
	Variant	copied(interval);
	CHECK(copied.type() == Variant::Interval);
	CHECK(copied.as_interval().ticks() == -150000000);

	Variant	assigned;
	assigned = datetime;
	CHECK(assigned.type() == Variant::DateTime);
	CHECK(assigned.as_datetime().ticks() == 0);

	// JSON has no time, so both are written as the text of one
	CHECK(interval.as_json() == "\"-1.50000000\"");
	CHECK(datetime.as_json() == "\"2000-01-01T00:00:00Z\"");

	/*
	 * A null time is not a time with a value, so it is JSON's own null and
	 * not the text of one - and a Variant holding it reads back as a null
	 * instead of as a date that never was.
	 */
	CHECK(Variant(Interval(NullTick)).as_json() == "null");
	CHECK(Variant(DateTime::fromTicks(NullTick)).as_json() == "null");
	CHECK(Variant(Interval(NullTick)).as_strval() == "null");
	{
		Variant	null_time((Interval(NullTick)));

		CHECK(null_time.type() == Variant::Interval);
		CHECK(null_time.as_interval().isNull());
	}

	// A text is the one thing either converts from, and a successful coercion
	// changes the type, as it does for the numeric types
	{
		Variant	text("1.5");
		CHECK(text.as_interval().ticks() == 150000000);
		CHECK(text.type() == Variant::Interval);

		Variant	when("2002-01-03T11:12:13Z");
		CHECK(when.as_datetime().toString() == "2002-01-03T11:12:13Z");
		CHECK(when.type() == Variant::DateTime);
	}

	// Reading one as text renders it, in a message and in StrVal::format
	{
		Variant	as_text(Interval(150000000));
		CHECK(as_text.as_strval() == "1.50000000");
		CHECK(as_text.type() == Variant::String);

		CHECK(StrVal::format("{1} and {2}",
				VariantArray() << Interval(150000000) << DateTime::fromTicks(0))
			== "1.50000000 and 2000-01-01T00:00:00Z");
	}

	/*
	 * Neither is a number and neither becomes the other: a length of time is
	 * not a point in time, whatever both are counted in. Each of these dies,
	 * so each is run in a child, which reports through the error buffer before
	 * it goes.
	 */
	StrVal	report;
	CHECK(coercion_aborts(interval_read_as_a_number, report));
	CHECK(same_code(report, "A0000801")
			&& report.find(StrVal("Integer")) >= 0
			&& report.find(StrVal("Interval")) >= 0);

	CHECK(coercion_aborts(datetime_read_as_an_interval, report));
	CHECK(same_code(report, "A0000801")
			&& report.find(StrVal("Interval")) >= 0
			&& report.find(StrVal("DateTime")) >= 0);

	CHECK(coercion_aborts(interval_read_as_a_datetime, report));
	CHECK(same_code(report, "A0000801")
			&& report.find(StrVal("DateTime")) >= 0
			&& report.find(StrVal("Interval")) >= 0);

	/*
	 * And the one the data must not be lost to: a target type this library has
	 * no coercion for. It is refused, naming both types, and the value is left
	 * as it was. The half of that which only a build with assertions off can
	 * show is in variant_ndebug_test.cpp.
	 */
	CHECK(coercion_aborts(coercion_without_a_case, report));
	CHECK(same_code(report, "A0000803")
			&& report.find(StrVal("Interval")) >= 0
			&& report.find(StrVal("Corrupt type")) >= 0);

	say(StrVal("a Variant holds an Interval or a DateTime, and neither is a number\n"));
	say(StrVal("refused: a time read as a number, and an interval as an instant\n"));
	say(StrVal("...and a coercion with no case reports it rather than discarding the value\n"));
}

void variant_tests()
{
	Variant vi(23);
	Variant vll(47LL);
	Variant vstr("foo");
	Variant vmap(Variant::StrVarMap);

	StrVal		s("foo");
	StrVariantMap	v;
	v.insert(s, 23LL);
	v.insert("baz", vll);

	v.insert("bar", vmap);
	say(StrVal::format("v has {1} elements\n", Variant((long)v.size())));
	expect_eq_int("three keys were inserted", v.size(), 3);

	StrVariantMap	vm = vmap.as_variant_map();
	StrVariantMap	vm2 = vm;
	// This will Unshare vm
	vm.insert("foo", vll);
	say(StrVal::format("vm2 has {1} elements\n", Variant((long)vm2.size())));
	say(StrVal::format("vm has {1} elements\n", Variant((long)vm.size())));
	expect_eq_int("the copy is still empty: inserting into vm unshared it", vm2.size(), 0);
	expect_eq_int("...and vm has the one it was given", vm.size(), 1);

	Variant f = vm["foo"];
	say(StrVal::format("Found \"foo\" as type {1}\n", Variant(f.type_name())));
	expect_eq_str("a map lookup finds the value it holds and its type", f.type_name(), "LongLong");

	int	fl = f.as_long();
	say(StrVal::format("Found foo={1} as_long\n", Variant(fl)));
	expect_eq_int("...which reads back as the number it was", fl, 47);

	StrVal	fs = f.as_strval();

	say(StrVal::format("Found foo=\"{1}\" as strval\n", Variant(fs)));
	expect_eq_str("...and as that number's text", fs, "47");
}
