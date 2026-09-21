/*
 * Standalone test suite for StrVal::format and its format language (see
 * include/strformat.h).
 *
 * format() expands a text by interpolating the parameters of a VariantArray at
 * the markers the text names them by: {1} is the first, {2} the second, and a
 * text may use them in any order or more than once, because a translation is
 * free to. A parameter is rendered as text, and whatever the text wants around
 * it - backticks for a name, slashes for a Syntax - is written into the text
 * rather than added by the caller.
 *
 * A marker may say how its parameter is rendered, after a colon: the
 * representation first (b, o, d, x, X), then the minimum characters to pad to
 * (spaces, or zeroes when the first digit is a 0), then after a < the maximum
 * characters to cut to, and then - three dots, or an ellipsis, and nothing
 * else - what the text shows in place of the end that was lost.
 *
 * A specification is applied as far as it is understood, and whatever else the
 * marker holds is passed over, since a text may come from a message catalog
 * that nothing has checked. A marker that names no parameter is left as it
 * stands, so a text that does not match its parameters shows the reader the
 * marker it could not fill.
 *
 * NOTE: this program deliberately makes no stdio call at all, since the
 * library is not allowed one either. Its report is built by appending to StrVal
 * and written with write(2).
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<strval.h>
#include	<variant.h>			// format() renders Variant parameters

#include	<unistd.h>			// write, for the report only

// The specification is packed, and meant to stay that way: a representation, a
// flag, a tail and two lengths. See StrFormatSpec.
static_assert(sizeof(StrFormatSpec) <= 8, "StrFormatSpec is meant to stay small");

bool		show_passes = false;
int		test_count;
int		failure_count;
const char*	new_group;

void
test_group(const char* group)
{
	new_group = group;
}

static void
write_report(StrVal line)
{
	StrValIndex	bytes = 0;
	const char*	text = line.asUTF8(bytes);
	(void)!write(1, text, bytes);
}

static void
report(const char* when, bool passed, const char* detail)
{
	test_count++;
	if (!passed)
	{
		if (new_group)
		{
			write_report(StrVal(new_group)+":\n");
			new_group = 0;
		}
		write_report(StrVal::fromInt32(test_count, 0)+":\t"+when+": FAIL"
				+(detail ? StrVal(" ")+detail : StrVal())+"\n");
		failure_count++;
	}
	else if (show_passes)
	{
		if (new_group)
		{
			write_report(StrVal(new_group)+":\n");
			new_group = 0;
		}
		write_report(StrVal::fromInt32(test_count, 0)+":\t"+when+": PASS\n");
	}
}

void
expect(const char* when, bool cond)
{
	report(when, cond, 0);
}

void
expect_eq_int(const char* when, long got, long want)
{
	bool	ok = got == want;
	StrVal	detail;
	if (!ok)
		detail = StrVal("(wanted ")+StrVal::fromInt32(want, 0)+" got "+StrVal::fromInt32(got, 0)+")";
	report(when, ok, ok ? 0 : detail.asUTF8());
}

// Compare the *content* of a StrVal against a plain UTF-8 C string
void
expect_eq_str(const char* when, StrVal got, const char* want)
{
	StrVal	wantv(want);
	bool	ok = got.length() == wantv.length() && got == wantv;
	StrVal	detail;
	if (!ok)
		detail = StrVal("(wanted \"")+wantv+"\" got \""+got+"\")";
	report(when, ok, ok ? 0 : detail.asUTF8());
}

void
position_tests()
{
	test_group("Format: a text names its parameters by position");
	expect_eq_str("a text with no marker is unchanged",
		StrVal::format("nothing to substitute", VariantArray()), "nothing to substitute");
	expect_eq_str("an empty text answers an empty string",
		StrVal::format("", VariantArray() << Variant("unused")), "");
	expect_eq_str("{1} takes the first parameter",
		StrVal::format("A {1} was expected", VariantArray() << Variant("brace")),
		"A brace was expected");
	expect_eq_str("a text may use its parameters out of order",
		StrVal::format("The {2} of {1} was not found",
			VariantArray() << Variant("TOP.B") << Variant("Discount")),
		"The Discount of TOP.B was not found");
	expect_eq_str("a parameter may be used more than once",
		StrVal::format("{1}, and {1} again", VariantArray() << Variant("twice")),
		"twice, and twice again");
	expect_eq_str("markers may be adjacent",
		StrVal::format("{1}{2}", VariantArray() << Variant("one") << Variant("two")),
		"onetwo");
	expect_eq_str("a marker may start the text",
		StrVal::format("{1} was expected", VariantArray() << Variant("A value")),
		"A value was expected");
	expect_eq_str("a marker may end the text",
		StrVal::format("This is {1}", VariantArray() << Variant("the end")),
		"This is the end");
	expect_eq_str("a two-digit marker names the tenth parameter",
		StrVal::format("{10} was the tenth",
			VariantArray() << 1 << 2 << 3 << 4 << 5 << 6 << 7 << 8 << 9 << Variant("ten")),
		"ten was the tenth");
}

void
unfilled_marker_tests()
{
	test_group("Format: a marker that names no parameter is left as it stands");
	expect_eq_str("a marker past the end of the parameters",
		StrVal::format("A {1} and a {3}", VariantArray() << Variant("value")),
		"A value and a {3}");
	expect_eq_str("{0} names no parameter",
		StrVal::format("{0} was expected", VariantArray() << Variant("x")),
		"{0} was expected");
	expect_eq_str("a marker with no parameters at all",
		StrVal::format("{1} was expected", VariantArray()),
		"{1} was expected");
	expect_eq_str("an empty marker is left alone",
		StrVal::format("{}", VariantArray() << Variant("x")), "{}");
	expect_eq_str("braces that name nothing are left alone",
		StrVal::format("a set {of} braces", VariantArray() << Variant("x")),
		"a set {of} braces");
	expect_eq_str("a lone opening brace is left alone",
		StrVal::format("a lone {", VariantArray() << Variant("x")), "a lone {");
	expect_eq_str("a lone closing brace is left alone",
		StrVal::format("a lone }", VariantArray() << Variant("x")), "a lone }");
	expect_eq_str("a marker followed by text, not a brace",
		StrVal::format("{1", VariantArray() << Variant("x")), "{1");
	expect_eq_str("a marker whose specification names no parameter",
		StrVal::format("A {3:8}", VariantArray() << Variant("value")),
		"A {3:8}");
}

void
doubled_brace_tests()
{
	test_group("Format: a doubled brace is a literal brace");
	expect_eq_str("a doubled opening brace is not a marker",
		StrVal::format("{{1}} was expected", VariantArray() << Variant("x")), "{1} was expected");
	expect_eq_str("a doubled brace beside a real marker",
		StrVal::format("{{ and {1}", VariantArray() << Variant("x")), "{ and x");
	expect_eq_str("both braces doubled",
		StrVal::format("{{}}", VariantArray() << Variant("x")), "{}");
	expect_eq_str("a doubled closing brace",
		StrVal::format("a }} b", VariantArray() << Variant("x")), "a } b");
	expect_eq_str("a lone brace needs no doubling",
		StrVal::format("a { b } c", VariantArray() << Variant("x")), "a { b } c");
	expect_eq_str("three braces are one literal and one marker",
		StrVal::format("{{{1}}}", VariantArray() << Variant("x")), "{x}");
	expect_eq_str("a backslash is an ordinary character",
		StrVal::format("a \\d digit, {1}", VariantArray() << Variant("x")), "a \\d digit, x");
}

void
delimiter_tests()
{
	test_group("Format: the text carries a parameter's delimiters, not the caller");
	expect_eq_str("a name is written in backticks",
		StrVal::format("The object `{1}` already has the supertype `{2}`",
			VariantArray() << Variant("TOP.B") << Variant("TOP.A")),
		"The object `TOP.B` already has the supertype `TOP.A`");
	expect_eq_str("a name with spaces survives",
		StrVal::format("Is `{1}` set?", VariantArray() << Variant("Is Array")),
		"Is `Is Array` set?");
	expect_eq_str("a Syntax is written between slashes",
		StrVal::format("A value matching /{1}/ was expected",
			VariantArray() << Variant("[1-9]*\\d")),
		"A value matching /[1-9]*\\d/ was expected");
	expect_eq_str("quotes in the text are the text's own",
		StrVal::format("Value: '{1}'", VariantArray() << Variant("param1")),
		"Value: 'param1'");
}

void
representation_tests()
{
	test_group("Format: the data representation comes first");
	expect_eq_str("decimal is the default",
		StrVal::format("{1}", VariantArray() << 255), "255");
	expect_eq_str("d names decimal too",
		StrVal::format("{1:d}", VariantArray() << 255), "255");
	expect_eq_str("x is lower case hexadecimal",
		StrVal::format("{1:x}", VariantArray() << 255), "ff");
	expect_eq_str("X is upper case hexadecimal",
		StrVal::format("{1:X}", VariantArray() << 255), "FF");
	expect_eq_str("o is octal",
		StrVal::format("{1:o}", VariantArray() << 255), "377");
	expect_eq_str("b is binary",
		StrVal::format("{1:b}", VariantArray() << 5), "101");
	// A non-decimal base renders the bit pattern at the width of the value's
	// own type, so no sign is involved: decimal is where a sign belongs.
	expect_eq_str("hexadecimal renders the bit pattern, not a sign",
		StrVal::format("{1:X}", VariantArray() << -255), "FFFFFF01");
	expect_eq_str("...so an int of -1 is eight f's",
		StrVal::format("{1:x}", VariantArray() << -1), "ffffffff");
	expect_eq_str("...and a long of -1 is sixteen",
		StrVal::format("{1:x}", VariantArray() << (long)-1), "ffffffffffffffff");
	expect_eq_str("...and binary is that many bits",
		StrVal::format("{1:b}", VariantArray() << -1),
		"11111111111111111111111111111111");
	expect_eq_str("decimal keeps the sign, which is what decimal is for",
		StrVal::format("{1}", VariantArray() << -255), "-255");
	expect_eq_str("...as does d",
		StrVal::format("{1:d}", VariantArray() << -255), "-255");
	expect_eq_str("...and the sign of the most negative value",
		StrVal::format("{1}", VariantArray() << (long long)(-9223372036854775807LL-1)),
		"-9223372036854775808");
	expect_eq_str("zero renders as one digit",
		StrVal::format("{1:X}", VariantArray() << 0), "0");
	expect_eq_str("a long renders in the same representations",
		StrVal::format("{1:x}", VariantArray() << (long)48879L), "beef");
	expect_eq_str("a long long renders in the same representations",
		StrVal::format("{1:b}", VariantArray() << (long long)6LL), "110");
	expect_eq_str("a base carries no prefix of its own",
		StrVal::format("0x{1:X}", VariantArray() << 255), "0xFF");
	expect_eq_str("...so the text places the prefix where it wants",
		StrVal::format("{1} is 0b{2:b}", VariantArray() << 5 << 5), "5 is 0b101");
	// The unsigned types render their own digits, which a signed reading of the
	// same bits would not give
	expect_eq_str("an unsigned parameter renders its own digits",
		StrVal::format("{1}", VariantArray() << 4000000000u), "4000000000");
	expect_eq_str("...including one no signed type could hold",
		StrVal::format("{1}", VariantArray() << 18446744073709551615ull), "18446744073709551615");
	expect_eq_str("...and a non-decimal base renders its bit pattern",
		StrVal::format("{1:X}", VariantArray() << 4294967295u), "FFFFFFFF");
	expect_eq_str("a representation leaves a string alone",
		StrVal::format("{1:x}", VariantArray() << Variant("named")), "named");
}

void
minimum_tests()
{
	test_group("Format: the minimum, padded for in front");
	expect_eq_str("a minimum pads a short string with spaces",
		StrVal::format("[{1:8}]", VariantArray() << Variant("abc")), "[     abc]");
	expect_eq_str("a minimum pads a short number with spaces",
		StrVal::format("[{1:8}]", VariantArray() << 42), "[      42]");
	expect_eq_str("a leading zero pads with zeroes",
		StrVal::format("[{1:08}]", VariantArray() << 42), "[00000042]");
	expect_eq_str("...and a negative number keeps its sign outermost",
		StrVal::format("[{1:08}]", VariantArray() << -42), "[-0000042]");
	expect_eq_str("a minimum already met pads nothing",
		StrVal::format("[{1:3}]", VariantArray() << Variant("abcdef")), "[abcdef]");
	expect_eq_str("a representation, then a minimum of ten, zero padded",
		StrVal::format("[{1:x010}]", VariantArray() << 255), "[00000000ff]");
	expect_eq_str("a minimum with a string and zeroes",
		StrVal::format("[{1:08}]", VariantArray() << Variant("abc")), "[00000abc]");

	// The two-digit hexadecimal of a byte escape: the representation first,
	// then the 0 that asks for zeroes rather than spaces, then the width
	expect_eq_str("two hexadecimal digits, padded with spaces",
		StrVal::format("\\x{1:X2}", VariantArray() << 5), "\\x 5");
	expect_eq_str("...and with zeroes, which is what an escape wants",
		StrVal::format("\\x{1:X02}", VariantArray() << 5), "\\x05");
	expect_eq_str("...and a value that needs no padding is untouched",
		StrVal::format("\\x{1:X02}", VariantArray() << 0xAB), "\\xAB");
	expect_eq_str("a brace in the text is doubled, so \\u{...} is written",
		StrVal::format("\\u{{{1:X}}}", VariantArray() << 0x1F600), "\\u{1F600}");
}

void
maximum_tests()
{
	test_group("Format: the maximum, cut beyond");
	expect_eq_str("a maximum cuts a longer string",
		StrVal::format("{1:<4}", VariantArray() << Variant("abcdef")), "abcd");
	expect_eq_str("a maximum leaves a shorter string alone",
		StrVal::format("{1:<8}", VariantArray() << Variant("abc")), "abc");
	expect_eq_str("a maximum with a tail replaces the end that was lost",
		StrVal::format("{1:<6...}", VariantArray() << Variant("abcdefghij")), "abc...");
	expect_eq_str("a short string takes no tail",
		StrVal::format("{1:<6...}", VariantArray() << Variant("abc")), "abc");
	expect_eq_str("a tail of one character",
		StrVal::format("{1:<5\xE2\x80\xA6}", VariantArray() << Variant("abcdefghij")), "abcd\xE2\x80\xA6");
	expect_eq_str("a maximum cuts a number too",
		StrVal::format("{1:<3}", VariantArray() << 12345), "123");
	expect_eq_str("a tail longer than the maximum shows only the tail",
		StrVal::format("{1:<2...}", VariantArray() << Variant("abcdef")), "..");
}

void
minimum_and_maximum_tests()
{
	test_group("Format: a minimum and a maximum together");
	expect_eq_str("a value between them is padded to the minimum",
		StrVal::format("[{1:4<8}]", VariantArray() << Variant("abc")), "[ abc]");
	expect_eq_str("a value over the maximum is cut to it",
		StrVal::format("[{1:4<8}]", VariantArray() << Variant("abcdefghij")), "[abcdefgh]");
	expect_eq_str("the example: hexadecimal, at least two, at most eight",
		StrVal::format("[{1:X2<8}]", VariantArray() << 255), "[FF]");
	expect_eq_str("...where the minimum pads a value that is short of it",
		StrVal::format("[{1:X2<8}]", VariantArray() << 5), "[ 5]");
	expect_eq_str("a minimum is met again after a cut and a tail",
		StrVal::format("[{1:8<6...}]", VariantArray() << Variant("abcdefghij")), "[  abc...]");
}

void
sizing_tests()
{
	test_group("Format: sizing counts characters, not bytes or columns");
	// Ten characters, sixteen bytes: two two-byte and two three-byte sequences
	StrVal	multi("caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xC3\xA9\xC3\xA9");
	expect_eq_int("the sample is ten characters", (long)multi.length(), 10);
	expect_eq_int("...and seventeen bytes", (long)multi.numBytes(), 17);
	expect_eq_str("a minimum counts characters",
		StrVal::format("[{1:12}]", VariantArray() << multi),
		"[  caf\xC3\xA9 \xE6\x97\xA5\xE6\x9C\xAC \xC3\xA9\xC3\xA9]");
	expect_eq_str("a maximum counts characters",
		StrVal::format("{1:<4}", VariantArray() << multi), "caf\xC3\xA9");
	expect_eq_str("a maximum and a tail count characters",
		StrVal::format("{1:<5...}", VariantArray() << multi), "ca...");
	expect_eq_str("a wide character counts as one",
		StrVal::format("[{1:3}]", VariantArray() << Variant("\xE6\x97\xA5\xE6\x9C\xAC")),
		"[ \xE6\x97\xA5\xE6\x9C\xAC]");
}

void
long_string_tests()
{
	test_group("Format: the length asked for in the example");
	StrVal	fifty = StrVal("0123456789")*5;	// Fifty characters
	expect_eq_int("the sample is fifty characters", (long)fifty.length(), 50);
	StrVal	cut = StrVal::format("{1:<40...}", VariantArray() << fifty);
	expect_eq_int("at most forty characters, the tail included", (long)cut.length(), 40);
	expect_eq_str("...being three dots after thirty-seven characters", cut.substr(37, 3), "...");
	expect_eq_str("...with the parameter's own start before them", cut.substr(0, 4), "0123");
	expect_eq_str("a parameter that fits keeps its own end",
		StrVal::format("{1:<40...}", VariantArray() << Variant("short")), "short");

	// The maximum is a maximum, not a target: something of exactly 38, 39 or
	// 40 characters comes through whole and takes no tail. Only a value past
	// the maximum is cut, and it drops back to what the tail leaves - 37 here.
	StrVal	forty = StrVal("0123456789")*4;			// Forty characters
	StrVal	thirty_eight = forty.head(38);
	StrVal	thirty_nine = forty.head(39);
	StrVal	forty_one = forty+"X";
	expect_eq_int("the sample is forty characters", (long)forty.length(), 40);
	expect_eq_str("thirty-eight characters are not cut",
		StrVal::format("{1:<40...}", VariantArray() << thirty_eight), thirty_eight.asUTF8());
	expect_eq_str("thirty-nine characters are not cut",
		StrVal::format("{1:<40...}", VariantArray() << thirty_nine), thirty_nine.asUTF8());
	expect_eq_str("forty characters are not cut, and take no tail",
		StrVal::format("{1:<40...}", VariantArray() << forty), forty.asUTF8());
	expect_eq_str("forty-one characters are cut back to thirty-seven and the tail",
		StrVal::format("{1:<40...}", VariantArray() << forty_one),
		(forty.head(37)+"...").asUTF8());
	expect_eq_int("...and the answer is forty either way",
		(long)StrVal::format("{1:<40...}", VariantArray() << forty_one).length(), 40);
	expect_eq_str("a maximum with no tail cuts at the maximum itself",
		StrVal::format("{1:<40}", VariantArray() << forty_one), forty.asUTF8());
	expect_eq_str("...and leaves forty alone",
		StrVal::format("{1:<40}", VariantArray() << forty), forty.asUTF8());

	// A tail of one character rather than three leaves one more character of
	// the value in front of it: what the tail costs is counted in characters,
	// so "…" is one character however many bytes it takes, and {1:<40…} keeps
	// thirty-nine characters and adds the ellipsis.
	StrVal	ellipsis("\xE2\x80\xA6");			// U+2026: three bytes, one character
	expect_eq_int("the ellipsis is three bytes", (long)ellipsis.numBytes(), 3);
	expect_eq_int("...and one character", (long)ellipsis.length(), 1);
	StrVal	cut_one = StrVal::format("{1:<40\xE2\x80\xA6}", VariantArray() << fifty);
	expect_eq_int("a one-character tail still answers forty characters",
		(long)cut_one.length(), 40);
	expect_eq_str("...being thirty-nine characters of the value",
		cut_one.head(39), forty.head(39).asUTF8());
	expect_eq_str("...and the ellipsis after them", cut_one.tail(1), ellipsis.asUTF8());
	expect_eq_str("forty characters are still not cut by it",
		StrVal::format("{1:<40\xE2\x80\xA6}", VariantArray() << forty), forty.asUTF8());
	expect_eq_str("forty-one are cut back to thirty-nine and the ellipsis",
		StrVal::format("{1:<40\xE2\x80\xA6}", VariantArray() << forty_one),
		(forty.head(39)+ellipsis).asUTF8());
}

void
composite_tests()
{
	test_group("Format: a composite parameter is expanded, not named");

	StringArray	strings;
	strings << "a" << "b" << "c";
	Variant	letters(strings);
	expect_eq_str("an array of strings is shown in brackets",
		StrVal::format("{1}", VariantArray() << letters), "[a, b, c]");
	expect_eq_str("an empty array is a pair of brackets",
		StrVal::format("{1}", VariantArray() << Variant(StringArray())), "[]");
	expect_eq_str("an array of variants has each element rendered as a parameter",
		StrVal::format("{1}", VariantArray() << Variant(VariantArray() << 1 << "two" << 3L)),
		"[1, two, 3]");

	StrVariantMap	map;
	map.put("fred", 23);
	map.put("fly", "boo");
	expect_eq_str("a map says itself in JSON",
		StrVal::format("{1}", VariantArray() << Variant(map)),
		"{ \"fly\": \"boo\", \"fred\": 23 }");	// The JSON emitter's compact form
	expect_eq_str("a parameter that was never set says so",
		StrVal::format("{1}", VariantArray() << Variant()), "<None>");
	expect_eq_str("a composite takes the marker's cut and its tail",
		StrVal::format("{1:<6...}", VariantArray() << letters), "[a,...");
	expect_eq_str("...and the marker's padding",
		StrVal::format("[{1:12}]", VariantArray() << letters), "[   [a, b, c]]");
	expect_eq_str("...while a representation it has no use for is ignored",
		StrVal::format("{1:x}", VariantArray() << letters), "[a, b, c]");
}

void
depth_tests()
{
	test_group("Format: how deep a composite is expanded");

	StringArray	xy;
	xy << "x" << "y";
	Variant	inner(xy);
	Variant	outer(VariantArray() << 1 << inner);
	Variant	deeper(VariantArray() << 0 << outer);

	expect_eq_str("an array within an array is expanded",
		StrVal::format("{1}", VariantArray() << outer), "[1, [x, y]]");
	expect_eq_str("...and three levels too, well within the limit",
		StrVal::format("{1}", VariantArray() << deeper), "[0, [1, [x, y]]]");
	expect_eq_str("a string parameter is not affected by any of it",
		StrVal::format("{1}", VariantArray() << "plain"), "plain");

	// A structure nested past the limit answers its type name where the limit
	// falls, rather than descending until the stack runs out
	Variant	deep = VariantArray();
	for (int i = 0; i < RENDER_MAX_DEPTH + 4; i++)
	{
		VariantArray	level;
		level << i << deep;		// One more level of nesting each time
		deep = Variant(level);
	}

	StrVal		rendered = StrVal::format("{1}", VariantArray() << deep);
	int		brackets = 0;
	for (StrValIndex at = 0; at < rendered.length(); at++)
		if (rendered[at] == '[')
			brackets++;
	expect_eq_int("a structure past the limit descends exactly as far as it may",
		brackets, RENDER_MAX_DEPTH);
	expect("...naming what it did not expand",
		rendered.find("<VarArray>") < rendered.length());

	// The JSON emitter is bounded by the same constant, maps included
	StrVariantMap	map;
	map.put("k", deep);
	expect("...and the JSON emitter is bounded too",
		Variant(map).as_json().find("\"<VarArray>\"") < Variant(map).as_json().length());

	// A caller who wants more of a structure than the limit allows says so
	expect("a chosen depth may exceed the limit",
		Variant(deep).as_json_at(-1, RENDER_MAX_DEPTH + 8).length()
			> Variant(deep).as_json_at(-1, 1).length());
}

void
bad_specification_tests()
{
	test_group("Format: a specification is applied as far as it is understood");

	// A text may be translated, and a translation may come from a catalog that
	// nothing has checked, so a part of a specification that means nothing
	// here leaves the rest of it working
	expect_eq_str("a representation that does not exist is passed over",
		StrVal::format("{1:q}", VariantArray() << Variant("x")), "x");
	expect_eq_str("...leaving the type's own format",
		StrVal::format("{1:q}", VariantArray() << 42), "42");
	expect_eq_str("...even where a sign was meant",
		StrVal::format("{1:08-}", VariantArray() << 42), "00000042");
	expect_eq_str("...and the same with a zero padded minimum of eight",
		StrVal::format("[{1:08-}]", VariantArray() << -42), "[-0000042]");
	expect_eq_str("a minimum that is understood is still applied",
		StrVal::format("[{1:8-}]", VariantArray() << 42), "[      42]");

	// What is understood of a maximum and its tail is applied, and the rest of
	// the marker is passed over
	expect_eq_str("a tail that is neither dots nor an ellipsis is passed over",
		StrVal::format("{1:<6xyz}", VariantArray() << Variant("abcdefghij")), "abcdef");
	expect_eq_str("two dots leave the maximum alone",
		StrVal::format("{1:<6..}", VariantArray() << Variant("abcdefghij")), "abcdef");
	expect_eq_str("four dots are three dots and a dot passed over",
		StrVal::format("{1:<6....}", VariantArray() << Variant("abcdefghij")), "abc...");
	expect_eq_str("a dot after an ellipsis is passed over",
		StrVal::format("{1:<6\xE2\x80\xA6.}", VariantArray() << Variant("abcdefghij")),
		"abcde\xE2\x80\xA6");

	// What is not a marker at all is still left as it stands
	expect_eq_str("a representation with nothing else",
		StrVal::format("{1:x}", VariantArray() << 15), "f");
	expect_eq_str("a colon with nothing after it",
		StrVal::format("{1:}", VariantArray() << Variant("x")), "x");
	expect_eq_str("a maximum with no minimum",
		StrVal::format("{1:<3}", VariantArray() << Variant("abcdef")), "abc");
	expect_eq_str("an unterminated specification",
		StrVal::format("{1:8", VariantArray() << Variant("x")), "{1:8");
	expect_eq_str("...nor one whose specification holds another brace, which is still a marker",
		StrVal::format("{1:8{2}", VariantArray() << Variant("x") << Variant("y")),
		"{1:8y");
	expect_eq_str("a specification inside a doubled pair",
		StrVal::format("{{{1:x}}}", VariantArray() << 255), "{ff}");
}

int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	position_tests();
	unfilled_marker_tests();
	doubled_brace_tests();
	delimiter_tests();
	representation_tests();
	minimum_tests();
	maximum_tests();
	minimum_and_maximum_tests();
	composite_tests();
	depth_tests();
	sizing_tests();
	long_string_tests();
	bad_specification_tests();

	write_report(StrVal("Completed ")+StrVal::fromInt32(test_count, 0)+" tests with "
			+StrVal::fromInt32(failure_count, 0)+" failures\n");
	return failure_count == 0 ? 0 : 1;
}
