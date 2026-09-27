/*
 * Re-assembly of contiguous slices: three slices of one string, joined back
 * into it. What makes it worth checking is that no slice copies anything - all
 * four share the one body, and putting the pieces back together reaches the
 * original body, not a copy of it.
 *
 * The body's address is different every run, so it is printed, not asserted;
 * what is asserted is which body each slice uses, where in it each starts, and
 * the characters that come out.
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<unistd.h>

#define		protected	public
#define		private		public

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
 * Every slice is shown the same way: its name, the body it uses (as the
 * address, which is the one thing here that differs between runs), and where
 * in that body it starts and ends. Which body it *should* be is checked.
 */
static void
show(const char* name, StrVal& s, StrVal& hello_world, long offset, long chars)
{
	say(StrVal::format("{1}\t0x{2}[{3}, {4}]\n",
		Variant(name)
		<< StrVal::fromUInt64((uint64_t)(uintptr_t)(StrBody*)s.body, 'x')
		<< (long)s.offset << (long)s.num_chars));

	/*
	 * Ref<T>::operator== compares the bodies themselves, and a StrBody has no
	 * equality of its own, so the addresses are what is compared here.
	 */
	expect(StrVal(name) + " uses hello_world's body, and does not copy it",
		(StrBody*)s.body == (StrBody*)hello_world.body);
	expect_eq_int(StrVal(name) + ": where it starts", (long)s.offset, offset);
	expect_eq_int(StrVal(name) + ": how many characters", (long)s.num_chars, chars);
}

int
main()
{
	StrVal	hello_world("Hello, world");
	StrVal	hello = hello_world.substr(0, 5);
	StrVal	comma = hello_world.substr(5, 2);
	StrVal	world = hello_world.substr(7, 5);
	StrVal	reassembled = hello + comma;
	reassembled += world;

	show("hello_world", hello_world, hello_world, 0, 12);
	show("hello", hello, hello_world, 0, 5);
	show("comma", comma, hello_world, 5, 2);
	show("world", world, hello_world, 7, 5);
	show("reassembled", reassembled, hello_world, 0, 12);

	// And the characters, which is what the re-assembly is for
	expect_eq_str("hello", hello, "Hello");
	expect_eq_str("comma", comma, ", ");
	expect_eq_str("world", world, "world");
	expect_eq_str("reassembled", reassembled, "Hello, world");
	expect("...which is the whole of hello_world again", reassembled == hello_world);
	expect("...and not a second copy of it: one body is still all there is",
		(StrBody*)reassembled.body == (StrBody*)hello_world.body);

	say(StrVal::format("{1} tests, {2} failures\n",
		Variant((int)test_count) << (int)failure_count));
	return failure_count == 0 ? 0 : 1;
}
