/*
 * The UTF-8 pointer class: increment, decrement, and add or subtract integers,
 * over both one-byte and two-byte characters. Every position it reaches is
 * checked, and the whole of the output goes out through one write(2) with its
 * text made by StrVal::format, so the test needs no stdio at all.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */
#include	<unistd.h>

#include	<utf8_ptr.h>
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
 * What a step produced, written out: every value this test checks is also
 * shown, so that a run of it reads as the transcript it always was.
 */
static void
show(const char* name, const StrVal& value)
{
	say(StrVal::format("{1}={2}\n", Variant(name) << value));
}

/*
 * What is left of the string from a pointer, which is what the pointer's own
 * position is worth checking as: the class's whole job is to land on the start
 * of a character, and every move it makes is visible in what it now points at.
 */
static StrVal
rest(const NulGuardedUTF8Ptr& at)
{
	return StrVal(static_cast<const char*>(at));
}

UTF8	source[] = "Hello, world";

int
main()
{
	GuardedUTF8Ptr	hello(source);
	GuardedUTF8Ptr	world = hello;

	say(StrVal("--- incr/decr ---\n"));

	// Post-increment: the character read is the one it was on, the pointer moves on
	UCS4	h = *hello++;
	show("h", StrVal(h));
	show("rest", rest(hello));
	expect_eq_str("post-increment reads 'H'", StrVal(h), "H");
	expect_eq_str("...and leaves the pointer on \"ello, world\"", rest(hello), "ello, world");

	// Assignment puts it back
	hello = world;
	show("rest", rest(hello));
	expect_eq_str("assignment puts it back", rest(hello), "Hello, world");

	// Pre-increment: the pointer moves first, then the character is read
	UCS4	e = *++hello;
	show("e", StrVal(e));
	show("rest", rest(hello));
	expect_eq_str("pre-increment reads 'e'", StrVal(e), "e");
	expect_eq_str("...and leaves the pointer on \"ello, world\"", rest(hello), "ello, world");

	// Pre-decrement comes back to 'H'
	h = *--hello;
	show("h", StrVal(h));
	show("rest", rest(hello));
	expect_eq_str("pre-decrement reads 'H' again", StrVal(h), "H");
	expect_eq_str("...at the start", rest(hello), "Hello, world");

	// Post-decrement: read first, then step back
	hello++;
	e = *hello--;
	show("e", StrVal(e));
	show("rest", rest(hello));
	expect_eq_str("post-decrement reads 'e'", StrVal(e), "e");
	expect_eq_str("...and leaves the pointer where it was", rest(hello), "Hello, world");

	say(StrVal("--- add and subtract ---\n"));
	show("hello+1", rest(hello+1));
	expect_eq_str("hello+1", rest(hello+1), "ello, world");
	hello += 1;
	show("hello+=1", rest(hello));
	expect_eq_str("hello+=1", rest(hello), "ello, world");
	hello -= -1;
	show("hello-=(-1)", rest(hello));
	expect_eq_str("hello-=(-1) is the same as +1", rest(hello), "llo, world");
	hello += -1;
	show("hello+=(-1)", rest(hello));
	expect_eq_str("hello+=(-1) comes back", rest(hello), "ello, world");
	show("(hello+=1)-1", rest(hello-1));
	expect_eq_str("(hello+=1)-1", rest(hello-1), "Hello, world");
	hello -= 1;
	show("(hello+=1)-=1", rest(hello));
	expect_eq_str("(hello+=1)-=1", rest(hello), "Hello, world");

	/*
	 * A two-byte character, written over the first byte of the string. The
	 * buffer is the test's own, and the pointer's moves are the same ones
	 * again, over a character that is not one byte wide.
	 */
	say(StrVal("--- 2-byte ---\n"));
	UTF8*	hack = source;
	UTF8Put(hack, 0xe8);			// è, two bytes: 0xC3 0xA8
	show("rest", rest(hello));
	expect_eq_str("the string now begins with è", rest(hello), "èllo, world");
	show("len", StrVal::fromInt32((int32_t)hello.len()));
	expect_eq_int("...whose two bytes count as one character", hello.len(), 2);
	expect("...so the pointer is at the start of one", hello.is1st());
	expect("...and a whole-character step lands on the next start", (hello+1).is1st());
	expect("...while a one-byte step lands inside it, which is not a start",
		!GuardedUTF8Ptr((static_cast<const char*>(hello))+1).is1st());

	say(StrVal("--- 2-byte add and subtract ---\n"));
	show("hello+1", rest(hello+1));
	expect_eq_str("hello+1 steps over the whole character", rest(hello+1), "llo, world");
	hello += 1;
	show("hello+=1", rest(hello));
	expect_eq_str("hello+=1", rest(hello), "llo, world");
	hello -= -1;
	show("hello-=(-1)", rest(hello));
	expect_eq_str("hello-=(-1) is the same as +1", rest(hello), "lo, world");
	hello += -1;
	show("hello+=(-1)", rest(hello));
	expect_eq_str("hello+=(-1) comes back", rest(hello), "llo, world");
	show("(hello+=1)-1", rest(hello-1));
	expect_eq_str("(hello+=1)-1 steps over it and back", rest(hello-1), "èllo, world");
	hello -= 1;
	show("(hello+=1)-=1", rest(hello));
	expect_eq_str("...and lands where it started", rest(hello), "èllo, world");

	say(StrVal::format("{1} tests, {2} failures\n",
		Variant((int)test_count) << (int)failure_count));
	return failure_count == 0 ? 0 : 1;
}
