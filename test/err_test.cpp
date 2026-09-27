/*
 * Errors and error numbers: the two halves of a generated message set, and
 * what calling one and reading the result back looks like. See doc/errbuf.md.
 *
 * Every value the sketch produces is checked here - the number it returns, the
 * set and message it was built from, the text the buffer holds and the
 * parameters that go with it - and all of its output goes out through one
 * write(2) with its text made by StrVal::format, so the test needs no stdio.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */
#include	<unistd.h>

#include	<error.h>
#include	<errbuf.h>
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
 * What the catalog compiler puts into a generated public header,
 * <Module>_err.h: a number per message, with its default text in a comment.
 */
#define	ADLERR_SET		1024
#define	ERRNUM_Something	ErrNum(ADLERR_SET, 20)	// Something went wrong with a thing

/*
 * ...and what its companion <Module>_msg.h emits, one reporting function per
 * message, gathering the parameters its default text calls for.
 */
static ThreadLocal<VariantArray>	scratch;

inline ErrNum
ErrorADL_Something(StrVal thing)
{
	VariantArray&	params = *scratch.get();
	params.evacuate();				// Empty, keeping the storage
	params.append(Variant(thing));
	return Error(ERRNUM_Something, "Something went wrong with a thing", params);
}

ErrNum
fail()
{
	return ErrorADL_Something("the thing");
}

int
main(int argc, const char** argv)
{
	ErrNum	e = fail();

	say(StrVal::format("fail() returned {1}\n",
		Variant(StrVal::fromInt32((int32_t)e, 'X'))));

	expect("the number it returns is the one the message set names",
		e == ERRNUM_Something);
	expect_eq_int("...its set", e.set(), ADLERR_SET);
	expect_eq_int("...and its message number", e.msg(), 20);
	expect("...which is a failure", e.is_failure());
	expect("...and not information", !e.is_info());

	if (e != ERRNUM_Something)
	{
		say(StrVal("The message set did not report what this test asked for\n"));
		return 1;
	}

	/*
	 * What a display does: read the message, format its text with the
	 * parameters that go with it, let it go, then retire it.
	 */
	ErrBuf*		buf = ErrBuffer();
	StrVal		text;
	StrVal		first;
	{
		ErrBuf::Message	m = buf->message(0);
		text = StrVal(m.default_text);
		first = m.parameters.length() > 0 ? m.parameters[0].as_strval() : StrVal();
		expect_eq_int("the buffer holds the one message", buf->count(), 1);
		expect_eq_str("...whose error number is the one that was returned",
			StrVal::fromInt32((int32_t)m.error, 'X'), StrVal::fromInt32((int32_t)e, 'X').asUTF8());
		expect_eq_str("...whose text is the message set's own", text,
			"Something went wrong with a thing");
		expect_eq_int("...with one parameter", m.parameters.length(), 1);
		expect_eq_str("...the thing that went wrong", first, "the thing");
		expect_eq_str("...which formats into the text, as a display renders it",
			StrVal::format(text, m.parameters), "Something went wrong with a thing");
	}
	buf->clear();	// Only once the message has been let go

	say(StrVal::format("It happened: \"{1}\" [0] {2}\n", Variant(text) << first));
	expect_eq_int("...and the buffer is empty again afterwards", buf->count(), 0);

	say(StrVal::format("{1} tests, {2} failures\n",
		Variant((int)test_count) << (int)failure_count));
	return failure_count == 0 ? 0 : 1;
}
