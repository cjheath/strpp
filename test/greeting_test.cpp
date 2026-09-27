/*
 * Greeting: a string built by joining, decorated with emoji, then case
 * converted. Every value it builds is checked, and the whole of its output
 * goes out through one write(2) with its text made by StrVal::format, so the
 * test needs no stdio at all.
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<unistd.h>

#include	<strval.h>
#include	<variant.h>		// StrVal::format's arguments are Variants

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
	say(StrVal::format("{1}:\t{2}: FAIL\n",
		Variant((int)test_count) << when));
}

static void
expect_eq_str(const StrVal& when, const StrVal& got, const char* wanted)
{
	test_count++;
	if (got == wanted)
		return;

	failure_count++;
	say(StrVal::format("{1}:\t{2}: FAIL\n\twanted: {3}\n\tgot:    {4}\n",
		Variant((int)test_count) << when << StrVal(wanted) << got));
}

static void
expect_eq_int(const StrVal& when, long got, long wanted)
{
	test_count++;
	if (got == wanted)
		return;

	failure_count++;
	say(StrVal::format("{1}:\t{2}: FAIL\n\twanted: {3}\n\tgot:    {4}\n",
		Variant((int)test_count) << when
			<< StrVal::fromInt64(wanted) << StrVal::fromInt64(got)));
}

/*
 * The greeting is decorated and converted, and both are checked: the emoji are
 * not letters, so case conversion must pass them through byte for byte, and
 * must not merge them or break them apart.
 */
static void
greet(StrVal greeting)
{
	StrVal	decorated = greeting + "! 🎉🍾";
	StrVal	upper = decorated;
	upper.toUpper();

	say(StrVal::format("{1}\n{2}\n", Variant(decorated) << upper));

	expect_eq_str("the greeting with the emoji appended",
		decorated, "Hello, world! 🎉🍾");
	expect_eq_str("upper-cased, with the emoji untouched",
		upper, "HELLO, WORLD! 🎉🍾");
	expect_eq_int("...counted as one character each", upper.length(), 16);
	expect_eq_int("...of four bytes each", upper.numBytes(), 22);
	expect("...and none of them changed by the conversion",
		upper.tail(2) == decorated.tail(2));
	expect("...while the text before them was converted",
		upper.head(12) == "HELLO, WORLD");
}

int
main()
{
	greet(StrVal("Hello, world"));

	say(StrVal::format("{1} tests, {2} failures\n",
		Variant((int)test_count) << (int)failure_count));
	return failure_count == 0 ? 0 : 1;
}
