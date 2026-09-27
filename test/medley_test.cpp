/*
 * A medley of string tests: comparing, indexing, slicing, a borrowed body,
 * characters outside the Basic Multilingual Plane, a text in Mandarin, and a
 * non-optimal encoding of one character.
 *
 * Every value it looks at is checked, and all of its output goes out through
 * one write(2) with its text made by StrVal::format, so the test needs no
 * stdio at all.
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<unistd.h>

#include	<strval.h>
#include	<variant.h>		// StrVal::format's arguments are Variants

#include	<cstring>
#include	"memory_monitor.h"

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

static void
expect_eq_char(const StrVal& when, UCS4 got, UCS4 wanted)
{
	test_count++;
	if (got == wanted)
		return;

	failure_count++;
	say(StrVal::format("{1}:\t{2}: FAIL\n\twanted: U-{3}\n\tgot:    U-{4}\n",
		Variant((int)test_count) << when
		<< StrVal::fromUInt32(wanted, 'X') << StrVal::fromUInt32(got, 'X')));
}

/*
 * A string's characters, one by one, as the indexer reports them: the last
 * index is the one past the end, which reads as the terminator.
 */
static void
show_chars(const char* name, const StrVal& s)
{
	say(StrVal::format("{1} =\n", Variant(name)));
	for (int i = 0; i <= s.length(); i++)
		say(StrVal::format("\t{1}: 0x{2} '{3}'\n",
			Variant(i) << StrVal::fromUInt32(s[i], 'X') << StrVal(s[i])));
}

void tests()
{
	StrVal	foobar("foo bar");

	say(StrVal::format("foobar.compare() == {1}\n",
		Variant(foobar.compare("foo bar"))));
	expect_eq_int("foobar compares equal to \"foo bar\"", foobar.compare("foo bar"), 0);
	expect("foobar == \"foo bar\"", foobar == StrVal("foo bar"));
	expect_eq_int("...and it is seven characters long", foobar.length(), 7);

	show_chars("foobar", foobar);
	{
		const char*	wanted = "foo bar";
		for (int i = 0; i <= foobar.length(); i++)
		{
			// The character at the length is the terminator, not a character
			expect_eq_char(StrVal::format("foobar[{1}]", Variant(i)),
				foobar[i], (UCS4)(unsigned char)wanted[i]);
		}
	}

	StrVal	foo = foobar.substr(0, 3);
	say(StrVal::format("foobar[0,3] = \"{1}\"\n", Variant(foo)));
	expect_eq_str("foobar[0,3]", foo, "foo");
	show_chars("foobar[0,3]", foo);
	expect_eq_char("...its [2]", foo[2], 'o');
	expect_eq_char("...and its [3], past the end, is the terminator", foo[3], 0);

	StrVal	bar = foobar.substr(4, 3);
	say(StrVal::format("foobar[4,3] = \"{1}\"\n", Variant(bar)));
	expect_eq_str("foobar[4,3]", bar, "bar");
	show_chars("foobar[4,3]", bar);
	expect_eq_char("...its [0]", bar[0], 'b');
	expect_eq_char("...and its [3], past the end, is the terminator", bar[3], 0);

	/*
	 * A borrowed body: the literal outlives the body, which outlives the
	 * StrVal. Nothing is copied, so the characters must come through exactly
	 * as the literal has them, from three characters in.
	 */
	StrBody	const_body("Borrow this data but don't fudge it\n", ArrayBorrow, 0, 1);
	StrVal	cstr(&const_body);
	StrVal	from_three = cstr.substr(3);
	say(StrVal::format("{1}", Variant(from_three)));
	expect_eq_str("a borrowed body, from its third character",
		from_three, "row this data but don't fudge it\n");
	expect_eq_int("...the whole of it", cstr.length(), 36);

	// Unicode substitution characters:
	StrVal	galley("�☐");	// "Replacement character" = U-FFFD, "Ballot Box" = U-2610
	say(StrVal::format("galley = U-{1} U-{2} '{3}'\n",
		Variant(StrVal::fromUInt32(galley[0], 'X'))
		<< StrVal::fromUInt32(galley[1], 'X') << galley));
	expect_eq_char("the Replacement character", galley[0], 0xFFFD);
	expect_eq_char("...and the Ballot Box", galley[1], 0x2610);
	expect_eq_int("...two characters", galley.length(), 2);
	expect_eq_int("...in six bytes", galley.numBytes(), 6);

	// Two characters outside the Basic Multilingual Plane, four bytes each
	StrVal	emoji("\xF0\x9F\x8E\x89\xF0\x9F\x8D\xBE");	// Party Popper U-1F389, Bottle with Popping Cork U-1F37E
	say(StrVal::format("emoji = U-{1} U-{2} '{3}'\n",
		Variant(StrVal::fromUInt32(emoji[0], 'X'))
		<< StrVal::fromUInt32(emoji[1], 'X') << emoji));
	expect_eq_char("a Party Popper", emoji[0], 0x1F389);
	expect_eq_char("...and a Bottle with Popping Cork", emoji[1], 0x1F37E);
	expect_eq_int("...two characters, eight bytes", emoji.numBytes(), 8);

	// In Mandarin: "It is possible that some Person speaks more than one Language."
	StrVal	multilingual("某一个人讲多过一种语言可 能的。");
	say(StrVal::format("multilingual.length: {1}\n", Variant(multilingual.length())));
	say(StrVal::format("multilingual = {1}\n", Variant(multilingual)));
	expect_eq_int("a Mandarin text is counted in characters, not bytes",
		multilingual.length(), 16);
	expect_eq_int("...fifteen of three bytes and one of one", multilingual.numBytes(), 46);
	expect_eq_str("...and is unchanged", multilingual, "某一个人讲多过一种语言可 能的。");

	// The single-character constructor:
	StrVal	each((UCS4)0x4E2A);		// 个 is Mandarin quantifier, more-or-less "each"
	say(StrVal::format("each = {1}, length: {2} chars, utf8: {3} bytes\n",
		Variant(each) << (int)each.length() << (int)each.numBytes()));
	expect_eq_str("a single-character string is that character", each, "个");
	expect_eq_int("...one character", each.length(), 1);
	expect_eq_int("...three bytes", each.numBytes(), 3);

	/*
	 * A character that is not encoded as tightly as it could be. "个" is
	 * U-4E2A, three bytes as \xE4\xB8\xAA, and this text has it in the
	 * four-byte form \xF0\x84\xB8\xAA.
	 *
	 * Storing a string copies the bytes it was given, so what comes back out
	 * is what went in - but a transform rebuilds the text character by
	 * character, writing each one as UTF8Put encodes it, so the same character
	 * comes out of asLower() three bytes wide. Both halves are checked here:
	 * the compaction the file was named for is the transform's, not the
	 * constructor's.
	 */
	const char*	raw = "每一\xF0\x84\xB8\xAA四方体的形状是 长方形。";
	StrVal  square_is_rectangle(raw);
	say(StrVal::format("square_is_rectangle.length: {1} chars, raw={2} bytes, stored={3} bytes\n",
		Variant((int)square_is_rectangle.length()) << (int)strlen(raw)
		<< (int)square_is_rectangle.numBytes()));
	say(StrVal::format("square_is_rectangle = {1}\n", Variant(square_is_rectangle)));
	expect_eq_int("the text is fifteen characters", square_is_rectangle.length(), 15);
	expect_eq_int("...which the same number of bytes came in as", square_is_rectangle.numBytes(), (long)strlen(raw));
	expect_eq_int("...forty-four of them", square_is_rectangle.numBytes(), 44);
	expect_eq_char("the fourth-form character decodes to the same character",
		square_is_rectangle[2], 0x4E2A);
	expect_eq_int("...and is still four bytes of the text",
		square_is_rectangle.substr(2, 1).numBytes(), 4);
	expect_eq_str("...with the bytes given, not rewritten",
		square_is_rectangle.asUTF8(), raw);
	/*
	 * UTF8Len answers for the *character*, not for the bytes it was stored
	 * as: U-4E2A is three bytes written optimally, whatever this text holds.
	 */
	expect_eq_int("...whose character is three bytes written optimally",
		UTF8Len(square_is_rectangle[2]), 3);
	expect_eq_int("...while the space beside it is one", UTF8Len(square_is_rectangle[10]), 1);

	/*
	 * What a program that uses the raw data needs is the bytes it loaded, so
	 * every operation that copies them keeps them: the character is still
	 * four bytes of the text after a slice, a join, an insert or a repeat.
	 */
	StrVal	non_optimal = square_is_rectangle.substr(2, 1);		// The one 4-byte character
	expect_eq_int("the character is four bytes of the text", non_optimal.numBytes(), 4);
	expect_eq_str("...and a slice of it keeps them", non_optimal, "\xF0\x84\xB8\xAA");
	expect_eq_str("...so does a join", StrVal("x") + non_optimal, "x\xF0\x84\xB8\xAA");
	expect_eq_str("...and a repeat", non_optimal * 2, "\xF0\x84\xB8\xAA\xF0\x84\xB8\xAA");
	{
		StrVal	inserted("x");
		inserted.insert(1, non_optimal);
		expect_eq_str("...and an insert", inserted, "x\xF0\x84\xB8\xAA");
	}
	expect_eq_str("...and reading the whole text back gives what was loaded",
		square_is_rectangle, raw);

	// ...while the same text after a transform: one byte fewer, because the
	// four-byte form of U-4E2A is written canonically as three
	StrVal	lowered = square_is_rectangle.asLower();
	say(StrVal::format("square_is_rectangle.asLower(): {1} chars, {2} bytes\n",
		Variant((int)lowered.length()) << (int)lowered.numBytes()));
	expect_eq_int("a transform rebuilds each character, so the text is smaller",
		lowered.numBytes(), 43);
	expect_eq_int("...while the character count is unchanged", lowered.length(), 15);
	expect_eq_char("...and the character is the same one", lowered[2], 0x4E2A);
	expect_eq_int("...now in its canonical three bytes",
		lowered.substr(2, 1).numBytes(), 3);
	expect_eq_str("...and the rest of the text is untouched",
		lowered.substr(0, 2), "每一");

	StrVal	formatted = StrVal::format("Value: '{1}', length {2}", Variant("param1") << 6);
	say(StrVal::format("formatted = {1}\n", Variant(formatted)));
	expect_eq_str("a format fills both parameters",
		formatted, "Value: 'param1', length 6");

	StrVal	lower = StrVal("LOWER").asLower();
	say(StrVal::format("lower = {1}\n", Variant(lower)));
	expect_eq_str("lower-casing", lower, "lower");
}

int
main(int argc, const char** argv)
{
#if defined(MEMCHECK)
	start_recording_allocations();
#endif
	tests();
#if defined(MEMCHECK)
	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();
#endif

	say(StrVal::format("{1} tests, {2} failures\n",
		Variant((int)test_count) << (int)failure_count));
	return failure_count == 0 ? 0 : 1;
}
