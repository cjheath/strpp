/*
 * Unicode Strings
 * Thorough test suite for the StrVal class
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<strval.h>

#include	<cstdio>
#include	<cstring>
#include	<climits>

bool		show_passes = false;
int		test_count;
int		failure_count;
const char*	new_group;

void		construction_tests();
void		indexing_tests();
void		substring_tests();
void		find_char_tests();
void		find_substring_tests();
void		find_any_not_tests();
void		concatenation_tests();
void		repeat_tests();
void		insert_append_prepend_tests();
void		case_conversion_tests();
void		json_tests();
void		int_conversion_tests();
void		comparison_tests();
void		copy_on_write_tests();
void		raw_binary_tests();
void		malformed_utf8_tests();
void		long_string_bookmark_tests();

int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	construction_tests();
	indexing_tests();
	substring_tests();
	find_char_tests();
	find_substring_tests();
	find_any_not_tests();
	concatenation_tests();
	repeat_tests();
	insert_append_prepend_tests();
	case_conversion_tests();
	json_tests();
	int_conversion_tests();
	comparison_tests();
	copy_on_write_tests();
	raw_binary_tests();
	malformed_utf8_tests();
	long_string_bookmark_tests();

	printf("Completed %d tests with %d failures\n", test_count, failure_count);
	return failure_count == 0 ? 0 : 1;
}

void
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

void
expect(const char* when, bool cond)
{
	report(when, cond, 0);
}

void
expect_eq(const char* when, long got, long want)
{
	char	detail[64];
	bool	ok = got == want;
	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted %ld got %ld)", want, got);
	report(when, ok, ok ? 0 : detail);
}

void
expect_eq(const char* when, UCS4 got, UCS4 want)
{
	char	detail[64];
	bool	ok = got == want;
	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted 0x%X got 0x%X)", want, got);
	report(when, ok, ok ? 0 : detail);
}

// Compare the *content* of a StrVal against a plain UTF-8 C string
void
expect_eq(const char* when, StrVal got, const char* want)
{
	StrVal	wantv(want);
	bool	ok = got.length() == wantv.length() && got == wantv;
	char	detail[512];
	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted \"%s\" got \"%s\")", want, got.asUTF8());
	report(when, ok, ok ? 0 : detail);
}

void
expect_eq_err(const char* when, ErrNum got, ErrNum want)
{
	char	detail[64];
	bool	ok = got == want;
	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted 0x%X got 0x%X)", (int32_t)want, (int32_t)got);
	report(when, ok, ok ? 0 : detail);
}

/*
 * Construction, basic properties
 */
void
construction_tests()
{
	test_group("Construction: empty string");
	StrVal	empty;
	expect("default-constructed is empty", empty.isEmpty());
	expect("default-constructed length is 0", empty.length() == 0);
	expect("default-constructed bool is false", !(bool)empty);
	expect_eq("default-constructed numBytes", (long)empty.numBytes(), 0);

	test_group("Construction: from C string (ASCII)");
	StrVal	foo("foo");
	expect("\"foo\" is not empty", !foo.isEmpty());
	expect("\"foo\" bool is true", (bool)foo);
	expect_eq("\"foo\" length", (long)foo.length(), 3);
	expect_eq("\"foo\" numBytes", (long)foo.numBytes(), 3);
	expect_eq("\"foo\" round-trips through asUTF8", foo, "foo");

	test_group("Construction: empty C string");
	StrVal	emptyC("");
	expect("\"\" is empty", emptyC.isEmpty());
	expect_eq("\"\" length", (long)emptyC.length(), 0);

	test_group("Construction: 2-byte UTF-8 character (e-acute)");
	StrVal	eacute("caf\xC3\xA9");	// "café"
	expect_eq("café character length", (long)eacute.length(), 4);
	expect_eq("café byte length", (long)eacute.numBytes(), 5);
	expect_eq("café[3] is U+00E9", eacute[3], (UCS4)0x00E9);

	test_group("Construction: 3-byte UTF-8 characters (CJK)");
	StrVal	cjk("\xE4\xB8\xAD\xE6\x96\x87");	// "中文" (Chinese)
	expect_eq("中文 character length", (long)cjk.length(), 2);
	expect_eq("中文 byte length", (long)cjk.numBytes(), 6);
	expect_eq("中文[0] is U+4E2D", cjk[0], (UCS4)0x4E2D);
	expect_eq("中文[1] is U+6587", cjk[1], (UCS4)0x6587);

	test_group("Construction: 4-byte UTF-8 character (emoji)");
	StrVal	emoji("\xF0\x9F\x98\x80");	// U+1F600 GRINNING FACE
	expect_eq("emoji character length", (long)emoji.length(), 1);
	expect_eq("emoji byte length", (long)emoji.numBytes(), 4);
	expect_eq("emoji[0] is U+1F600", emoji[0], (UCS4)0x1F600);

	test_group("Construction: single UCS4 character");
	StrVal	singleAscii((UCS4)'Q');
	expect_eq("single ASCII char length", (long)singleAscii.length(), 1);
	expect_eq("single ASCII char content", singleAscii, "Q");

	StrVal	singleWide((UCS4)0x4E2A);	// 个
	expect_eq("single wide char length", (long)singleWide.length(), 1);
	expect_eq("single wide char byte length", (long)singleWide.numBytes(), 3);
	expect_eq("single wide char content", singleWide, "\xE4\xB8\xAA");

	test_group("Construction: from length-delimited buffer (embedded NUL)");
	StrVal	embeddedNul("a\0b", (StrValIndex)3);
	expect_eq("embedded-NUL length", (long)embeddedNul.length(), 3);
	expect_eq("embedded-NUL [0]", embeddedNul[0], (UCS4)'a');
	expect_eq("embedded-NUL [1]", embeddedNul[1], (UCS4)0);
	expect_eq("embedded-NUL [2]", embeddedNul[2], (UCS4)'b');

	test_group("Construction: with pre-allocation");
	StrVal	prealloc("hi", (StrValIndex)2, 40);
	expect_eq("prealloc length unaffected", (long)prealloc.length(), 2);
	expect_eq("prealloc content", prealloc, "hi");
	prealloc += " there";			// Should not need to reallocate, but must still work
	expect_eq("prealloc after append", prealloc, "hi there");

	test_group("Construction: copy constructor");
	StrVal	original("hello");
	StrVal	copy(original);
	expect_eq("copy has same content", copy, "hello");
	expect("copy == original", copy == original);
}

/*
 * Character indexing (operator[])
 */
void
indexing_tests()
{
	test_group("Indexing: ASCII string");
	StrVal	s("abc");
	expect_eq("s[0]", s[0], (UCS4)'a');
	expect_eq("s[1]", s[1], (UCS4)'b');
	expect_eq("s[2]", s[2], (UCS4)'c');
	expect_eq("s[length()] is NUL", s[s.length()], (UCS4)0);
	expect_eq("s[length()+3] out of range", s[s.length()+3], (UCS4)UCS4_NONE);
	expect_eq("s[-1] out of range", s[-1], (UCS4)UCS4_NONE);

	test_group("Indexing: multi-byte string, character not byte offsets");
	// "a" + Greek alpha (2 bytes) + "b" + CJK "中" (3 bytes) + "c"
	StrVal	mixed("a\xCE\xB1" "b\xE4\xB8\xAD" "c");
	expect_eq("mixed character length", (long)mixed.length(), 5);
	expect_eq("mixed byte length", (long)mixed.numBytes(), 1+2+1+3+1);
	expect_eq("mixed[0]", mixed[0], (UCS4)'a');
	expect_eq("mixed[1] is alpha", mixed[1], (UCS4)0x03B1);
	expect_eq("mixed[2]", mixed[2], (UCS4)'b');
	expect_eq("mixed[3] is 中", mixed[3], (UCS4)0x4E2D);
	expect_eq("mixed[4]", mixed[4], (UCS4)'c');
	expect_eq("mixed[5] is NUL", mixed[5], (UCS4)0);
}

/*
 * Substrings: substr/head/tail/shorter
 */
void
substring_tests()
{
	test_group("Substrings: ASCII");
	StrVal	hw("Hello, world");
	expect_eq("substr(0,5)", hw.substr(0, 5), "Hello");
	expect_eq("substr(7,5)", hw.substr(7, 5), "world");
	expect_eq("substr(7)", hw.substr(7), "world");
	expect_eq("head(5)", hw.head(5), "Hello");
	expect_eq("tail(5)", hw.tail(5), "world");
	expect_eq("shorter(7)", hw.shorter(7), "Hello");

	test_group("Substrings: edge cases");
	expect("substr(at >= length) is empty", hw.substr(20, 3).isEmpty());
	expect("substr(at, 0) is empty", hw.substr(2, 0).isEmpty());
	expect("substr(at, -2) is empty (illegal len)", hw.substr(2, -2).isEmpty());
	expect_eq("substr(0, 999) clamps to whole string", hw.substr(0, 999), "Hello, world");
	expect_eq("substr(0, -1) is whole string", hw.substr(0, -1), "Hello, world");
	expect("tail(chars > length) is empty", hw.tail(hw.length()+5).isEmpty());
	expect("shorter(chars > length) is empty", hw.shorter(hw.length()+5).isEmpty());
	expect_eq("head(0) is empty", hw.head(0), "");

	test_group("Substrings: multi-byte character boundaries");
	// "中文测试" = 4 CJK characters, 3 bytes each
	StrVal	cjk("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95");
	expect_eq("cjk substr(1,2)", cjk.substr(1, 2), "\xE6\x96\x87\xE6\xB5\x8B");
	expect_eq("cjk head(1)", cjk.head(1), "\xE4\xB8\xAD");
	expect_eq("cjk tail(1)", cjk.tail(1), "\xE8\xAF\x95");
	expect_eq("cjk substr keeps byte-correct length", (long)cjk.substr(1, 2).numBytes(), 6);
}

/*
 * Character search: find/rfind
 */
void
find_char_tests()
{
	test_group("find(char)");
	StrVal	s("abcabc");
	expect_eq("find 'b'", (long)s.find('b'), 1);
	expect_eq("find 'b' after 1", (long)s.find('b', 1), 4);
	expect_eq("find 'z' (absent)", (long)s.find('z'), -1);
	expect_eq("find in empty string", (long)StrVal("").find('a'), -1);

	test_group("rfind(char)");
	expect_eq("rfind 'b'", (long)s.rfind('b'), 4);
	expect_eq("rfind 'b' before 4", (long)s.rfind('b', 4), 1);
	expect_eq("rfind 'z' (absent)", (long)s.rfind('z'), -1);
	expect_eq("rfind in empty string", (long)StrVal("").rfind('a'), -1);

	test_group("find/rfind(char) with Unicode content");
	StrVal	greek("\xCE\xB1\xCE\xB2\xCE\xB1");	// alpha beta alpha
	expect_eq("find alpha", (long)greek.find(0x03B1), 0);
	expect_eq("find alpha after 0", (long)greek.find(0x03B1, 0), 2);
	expect_eq("rfind alpha", (long)greek.rfind(0x03B1), 2);
	expect_eq("find beta", (long)greek.find(0x03B2), 1);
}

/*
 * Substring search: find/rfind
 */
void
find_substring_tests()
{
	test_group("find(StrVal)");
	StrVal	s("the quick brown fox jumps over the lazy dog");
	expect_eq("find \"quick\"", (long)s.find(StrVal("quick")), 4);
	expect_eq("find \"the\" (first)", (long)s.find(StrVal("the")), 0);
	expect_eq("find \"the\" after 0", (long)s.find(StrVal("the"), 0), 31);
	expect_eq("find \"zzz\" (absent)", (long)s.find(StrVal("zzz")), -1);
	expect_eq("find empty needle", (long)s.find(StrVal("")), 0);

	test_group("rfind(StrVal)");
	expect_eq("rfind \"the\"", (long)s.rfind(StrVal("the")), 31);
	expect_eq("rfind \"the\" before 31", (long)s.rfind(StrVal("the"), 31), 0);
	expect_eq("rfind \"zzz\" (absent)", (long)s.rfind(StrVal("zzz")), -1);

	test_group("find(StrVal) with Unicode content");
	StrVal	multi("\xE4\xB8\xAD\xE6\x96\x87 says \xCE\xB1\xCE\xB2");	// "中文 says αβ"
	expect_eq("find \xE6\x96\x87", (long)multi.find(StrVal("\xE6\x96\x87")), 1);
	expect_eq("find \"says\"", (long)multi.find(StrVal("says")), 3);
	expect_eq("find alpha-beta", (long)multi.find(StrVal("\xCE\xB1\xCE\xB2")), 8);
}

/*
 * findAny/rfindAny/findNot/rfindNot
 */
void
find_any_not_tests()
{
	test_group("findAny/rfindAny");
	StrVal	s("hello world");
	StrVal	vowels("aeiou");
	expect_eq("findAny vowel", (long)s.findAny(vowels), 1);		// 'e'
	expect_eq("findAny vowel after 1", (long)s.findAny(vowels, 1), 4);	// 'o'
	expect_eq("rfindAny vowel", (long)s.rfindAny(vowels), 7);		// 'o' in "world"
	StrVal	digits("0123456789");
	expect_eq("findAny digit (absent)", (long)s.findAny(digits), -1);

	test_group("findNot/rfindNot");
	StrVal	padded("   xyz   ");
	StrVal	space(" ");
	expect_eq("findNot space", (long)padded.findNot(space), 3);
	expect_eq("rfindNot space", (long)padded.rfindNot(space), 5);
	StrVal	allSpace("   ");
	expect_eq("findNot on all-matching string", (long)allSpace.findNot(space), -1);

	test_group("findAny/findNot with Unicode set");
	StrVal	mixedChars("a\xCE\xB1茶z");	// a, alpha, tea(茶), z
	StrVal	greekSet("\xCE\xB1\xCE\xB2\xCE\xB3");	// alpha beta gamma
	expect_eq("findAny greek letter", (long)mixedChars.findAny(greekSet), 1);
}

/*
 * Concatenation: operator+, operator+=
 */
void
concatenation_tests()
{
	test_group("operator+ (StrVal + StrVal)");
	StrVal	a("foo");
	StrVal	b("bar");
	expect_eq("a + b", a + b, "foobar");
	expect_eq("a unchanged after a+b", a, "foo");
	expect_eq("b unchanged after a+b", b, "bar");

	test_group("operator+ (StrVal + const char*)");
	expect_eq("a + \"baz\"", a + "baz", "foobaz");

	test_group("operator+ (const char* + StrVal)");
	expect_eq("\"baz\" + a", "baz" + a, "bazfoo");

	test_group("operator+ (StrVal + UCS4)");
	expect_eq("a + '!'", a + (UCS4)'!', "foo!");
	StrVal	greekBase("x");
	expect_eq("x + alpha", greekBase + (UCS4)0x03B1, "x\xCE\xB1");

	test_group("operator+= (StrVal)");
	StrVal	acc("foo");
	acc += b;
	expect_eq("acc after += StrVal", acc, "foobar");

	test_group("operator+= (UCS4)");
	StrVal	acc2("foo");
	acc2 += (UCS4)'!';
	expect_eq("acc2 after += UCS4", acc2, "foo!");

	test_group("operator+= on empty string");
	StrVal	empty;
	empty += StrVal("grown");
	expect_eq("empty += grows correctly", empty, "grown");

	test_group("Concatenation of contiguous slices (fast path)");
	StrVal	hw("Hello, world");
	StrVal	hello = hw.substr(0, 5);
	StrVal	comma = hw.substr(5, 2);
	StrVal	world = hw.substr(7, 5);
	StrVal	reassembled = hello + comma;
	reassembled += world;
	expect_eq("reassembled contiguous slices", reassembled, "Hello, world");
}

/*
 * Repetition: operator*
 */
void
repeat_tests()
{
	test_group("operator* (repeat)");
	StrVal	ab("ab");
	expect_eq("ab * 3", ab*3, "ababab");
	expect_eq("ab * 1", ab*1, "ab");
	expect_eq("ab * 0", ab*0, "");
	expect_eq("ab unchanged after *", ab, "ab");

	StrVal	greek("\xCE\xB1");	// alpha
	expect_eq("alpha * 3", greek*3, "\xCE\xB1\xCE\xB1\xCE\xB1");
	expect_eq("alpha * 3 char length", (long)(greek*3).length(), 3);
}

/*
 * insert/append/prepend
 */
void
insert_append_prepend_tests()
{
	test_group("append/prepend");
	StrVal	s("world");
	s.prepend(StrVal("hello "));
	expect_eq("after prepend", s, "hello world");
	s.append(StrVal("!"));
	expect_eq("after append", s, "hello world!");

	test_group("insert at arbitrary position");
	StrVal	s2("helloworld");
	s2.insert(5, StrVal(", "));
	expect_eq("insert in the middle", s2, "hello, world");

	test_group("insert at start / end");
	StrVal	s3("bc");
	s3.insert(0, StrVal("a"));
	expect_eq("insert at start", s3, "abc");
	s3.insert(s3.length(), StrVal("d"));
	expect_eq("insert at end", s3, "abcd");

	test_group("insert into Unicode content preserves character boundaries");
	StrVal	cjk("\xE4\xB8\xAD\xE6\x96\x87");	// 中文
	cjk.insert(1, StrVal("-"));
	expect_eq("insert between CJK chars", cjk, "\xE4\xB8\xAD-\xE6\x96\x87");
	expect_eq("insert between CJK chars, char length", (long)cjk.length(), 3);

	test_group("insert does not corrupt an unrelated shared copy");
	StrVal	base("shared");
	StrVal	aliased(base);
	aliased.append(StrVal(" mutated"));
	expect_eq("base is untouched by append on the alias", base, "shared");
	expect_eq("aliased reflects the append", aliased, "shared mutated");
}

/*
 * Case conversion
 */
void
case_conversion_tests()
{
	test_group("asLower/asUpper (ASCII), non-mutating");
	StrVal	mixed("Hello World");
	expect_eq("asLower", mixed.asLower(), "hello world");
	expect_eq("asUpper", mixed.asUpper(), "HELLO WORLD");
	expect_eq("original unchanged after asLower/asUpper", mixed, "Hello World");

	test_group("toLower/toUpper (ASCII), mutating");
	StrVal	m1("MixedCase");
	m1.toLower();
	expect_eq("toLower mutates in place", m1, "mixedcase");
	StrVal	m2("MixedCase");
	m2.toUpper();
	expect_eq("toUpper mutates in place", m2, "MIXEDCASE");

	test_group("Case conversion: Latin-1 accented characters");
	StrVal	cafe("caf\xC3\xA9");			// café
	expect_eq("café upper", cafe.asUpper(), "CAF\xC3\x89");	// CAFÉ
	StrVal	CAFE("CAF\xC3\x89");			// CAFÉ
	expect_eq("CAFÉ lower", CAFE.asLower(), "caf\xC3\xA9");	// café

	test_group("Case conversion: Greek");
	StrVal	greekLower("\xCE\xB1\xCE\xB2\xCE\xB3");	// alpha beta gamma
	expect_eq("greek upper", greekLower.asUpper(), "\xCE\x91\xCE\x92\xCE\x93");	// ALPHA BETA GAMMA
	StrVal	greekUpper("\xCE\x91\xCE\x92\xCE\x93");
	expect_eq("greek lower", greekUpper.asLower(), "\xCE\xB1\xCE\xB2\xCE\xB3");

	test_group("Case conversion: already-lower/upper is idempotent");
	StrVal	lower("already lower");
	expect_eq("lower.asLower() unchanged", lower.asLower(), "already lower");
	StrVal	upper("ALREADY UPPER");
	expect_eq("upper.asUpper() unchanged", upper.asUpper(), "ALREADY UPPER");

	test_group("Case conversion: mixed ASCII/non-ASCII content");
	StrVal	mix("Caf\xC3\xA9 123!");
	expect_eq("mixed content upper", mix.asUpper(), "CAF\xC3\x89 123!");
}

/*
 * JSON escaping (toJSON/asJSON)
 */
void
json_tests()
{
	test_group("JSON: control character escapes");
	expect_eq("newline", StrVal("a\nb").asJSON(), "a\\nb");
	expect_eq("tab", StrVal("a\tb").asJSON(), "a\\tb");
	expect_eq("carriage return", StrVal("a\rb").asJSON(), "a\\rb");
	expect_eq("backspace", StrVal("a\bb").asJSON(), "a\\bb");
	expect_eq("form feed", StrVal("a\fb").asJSON(), "a\\fb");
	expect_eq("backslash", StrVal("a\\b").asJSON(), "a\\\\b");
	expect_eq("double quote", StrVal("a\"b").asJSON(), "a\\\"b");
	expect_eq("forward slash", StrVal("a/b").asJSON(), "a\\/b");
	expect_eq("NUL byte", StrVal("a\0b", (StrValIndex)3).asJSON(), "a\\0b");

	test_group("JSON: other control chars use \\u escape");
	expect_eq("SOH (0x01)", StrVal("a\x01" "b").asJSON(), "a\\u0001b");

	test_group("JSON: printable ASCII is untouched");
	expect_eq("plain ASCII", StrVal("Hello, World! 123").asJSON(), "Hello, World! 123");

	test_group("JSON: BMP Unicode characters are left as literal UTF-8");
	expect_eq("café literal in JSON", StrVal("caf\xC3\xA9").asJSON(), "caf\xC3\xA9");
	expect_eq("CJK literal in JSON", StrVal("\xE4\xB8\xAD\xE6\x96\x87").asJSON(), "\xE4\xB8\xAD\xE6\x96\x87");

	test_group("JSON: astral characters (emoji) become surrogate pair escapes");
	// U+1F600 -> high D83D, low DE00
	expect_eq("emoji surrogate pair", StrVal("\xF0\x9F\x98\x80").asJSON(), "\\uD83D\\uDE00");

	test_group("JSON: non-mutating asJSON leaves original intact");
	StrVal	orig("a\nb");
	StrVal	json = orig.asJSON();
	expect_eq("original unaffected by asJSON", orig, "a\nb");
	expect_eq("asJSON result correct", json, "a\\nb");

	test_group("JSON: toJSON mutates in place");
	StrVal	mut("tab\there");
	mut.toJSON();
	expect_eq("toJSON mutated", mut, "tab\\there");
}

/*
 * Integer parsing (asInt32)
 */
void
int_conversion_tests()
{
	ErrNum		err;
	StrValIndex	scanned;

	test_group("asInt32: basic decimal");
	expect_eq("\"123\"", (long)StrVal("123").asInt32(&err), 123);
	expect_eq("\"123\" err", err, ErrNum(0));
	expect_eq("\"-42\"", (long)StrVal("-42").asInt32(&err), -42);
	expect_eq("\"-42\" err", err, ErrNum(0));
	expect_eq("\"+42\"", (long)StrVal("+42").asInt32(&err), 42);
	expect_eq("\"0\"", (long)StrVal("0").asInt32(&err), 0);

	test_group("asInt32: leading/trailing whitespace ignored");
	expect_eq("\"  42  \"", (long)StrVal("  42  ").asInt32(&err), 42);
	expect_eq("\"  42  \" err", err, ErrNum(0));

	test_group("asInt32: radix auto-detection (radix 0)");
	expect_eq("\"0x1A\" hex", (long)StrVal("0x1A").asInt32(&err, 0), 26);
	expect_eq("\"0X1a\" hex lowercase digits", (long)StrVal("0X1a").asInt32(&err, 0), 26);
	expect_eq("\"0b101\" binary", (long)StrVal("0b101").asInt32(&err, 0), 5);
	expect_eq("\"010\" octal", (long)StrVal("010").asInt32(&err, 0), 8);
	expect_eq("\"10\" decimal (no leading zero)", (long)StrVal("10").asInt32(&err, 0), 10);

	test_group("asInt32: explicit radix overrides auto-detection of octal");
	expect_eq("\"010\" radix 10", (long)StrVal("010").asInt32(&err, 10), 10);

	test_group("asInt32: explicit radix 16/2/36");
	expect_eq("\"FF\" radix 16", (long)StrVal("FF").asInt32(&err, 16), 255);
	expect_eq("\"ff\" radix 16", (long)StrVal("ff").asInt32(&err, 16), 255);
	expect_eq("\"101\" radix 2", (long)StrVal("101").asInt32(&err, 2), 5);
	expect_eq("\"Z\" radix 36", (long)StrVal("Z").asInt32(&err, 36), 35);

	test_group("asInt32: error - no digits (blank string)");
	StrVal("   ").asInt32(&err, 10, &scanned);
	expect_eq("all-blank err", err, ErrNum(STRERR_SET, STRERR_NO_DIGITS));

	test_group("asInt32: error - not a number");
	StrVal("abc").asInt32(&err, 0, &scanned);
	expect_eq("\"abc\" err", err, ErrNum(STRERR_SET, STRERR_NOT_NUMBER));
	expect_eq("\"abc\" scanned", (long)scanned, 0);

	test_group("asInt32: error - trailing text");
	long	tv = StrVal("12x").asInt32(&err, 0, &scanned);
	expect_eq("\"12x\" value still parsed", tv, 12);
	expect_eq("\"12x\" err", err, ErrNum(STRERR_SET, STRERR_TRAIL_TEXT));
	expect_eq("\"12x\" scanned", (long)scanned, 2);

	test_group("asInt32: error - illegal radix");
	StrVal("123").asInt32(&err, 37, &scanned);
	expect_eq("radix 37 err", err, ErrNum(STRERR_SET, STRERR_ILLEGAL_RADIX));
	StrVal("123").asInt32(&err, -1, &scanned);
	expect_eq("radix -1 err", err, ErrNum(STRERR_SET, STRERR_ILLEGAL_RADIX));

	test_group("asInt32: error - overflow (exceeds unsigned long)");
	StrVal("99999999999999999999999999").asInt32(&err, 10, &scanned);
	expect_eq("huge decimal overflow err", err, ErrNum(STRERR_SET, STRERR_NUMBER_OVERFLOW));

	test_group("asInt32: error - overflow (exceeds signed range, unsigned still fits)");
	// LONG_MAX + 1: fits an unsigned long, but not a signed one
	char	buf[32];
	snprintf(buf, sizeof(buf), "%lu", (unsigned long)LONG_MAX + 1);
	StrVal(buf).asInt32(&err, 10, &scanned);
	expect_eq("LONG_MAX+1 overflow err", err, ErrNum(STRERR_SET, STRERR_NUMBER_OVERFLOW));
}

/*
 * Comparison operators
 */
void
comparison_tests()
{
	test_group("Comparisons: equality/inequality");
	expect("\"abc\" == \"abc\"", StrVal("abc") == StrVal("abc"));
	expect("\"abc\" != \"abd\"", StrVal("abc") != StrVal("abd"));
	expect("\"\" == \"\"", StrVal("") == StrVal(""));
	expect("\"abc\" != \"\"", StrVal("abc") != StrVal(""));

	test_group("Comparisons: ordering");
	expect("\"abc\" < \"abd\"", StrVal("abc") < StrVal("abd"));
	expect("\"abc\" <= \"abc\"", StrVal("abc") <= StrVal("abc"));
	expect("\"abd\" > \"abc\"", StrVal("abd") > StrVal("abc"));
	expect("\"abc\" >= \"abc\"", StrVal("abc") >= StrVal("abc"));
	expect("\"ab\" < \"abc\" (prefix is smaller)", StrVal("ab") < StrVal("abc"));
	expect("\"abc\" > \"ab\"", StrVal("abc") > StrVal("ab"));

	test_group("Comparisons: Unicode content");
	StrVal	alpha("\xCE\xB1");	// U+03B1
	StrVal	beta("\xCE\xB2");	// U+03B2
	expect("alpha < beta (byte-order matches codepoint order)", alpha < beta);
	expect("alpha == alpha", alpha == StrVal("\xCE\xB1"));
}

/*
 * Copy-on-write / sharing semantics
 */
void
copy_on_write_tests()
{
	test_group("COW: mutating a copy doesn't affect the original");
	StrVal	original("original");
	StrVal	aCopy(original);
	aCopy.toUpper();
	expect_eq("original unaffected by copy's toUpper", original, "original");
	expect_eq("copy reflects toUpper", aCopy, "ORIGINAL");

	test_group("COW: mutating the original doesn't affect a copy");
	StrVal	orig2("base");
	StrVal	copy2(orig2);
	orig2 += "-mutated";
	expect_eq("orig2 mutated", orig2, "base-mutated");
	expect_eq("copy2 unaffected", copy2, "base");

	test_group("COW: substrings are independent once mutated");
	StrVal	whole("abcdef");
	StrVal	left = whole.substr(0, 3);
	StrVal	right = whole.substr(3, 3);
	left.toUpper();
	expect_eq("left mutated", left, "ABC");
	expect_eq("right unaffected", right, "def");
	expect_eq("whole unaffected", whole, "abcdef");

	test_group("COW: assignment shares then diverges on mutation");
	StrVal	src("shared value");
	StrVal	dst;
	dst = src;
	expect_eq("dst equals src after assignment", dst, "shared value");
	dst.append(StrVal(" more"));
	expect_eq("dst diverged after mutation", dst, "shared value more");
	expect_eq("src unaffected by dst mutation", src, "shared value");
}

/*
 * StrRawBinary data
 */
void
raw_binary_tests()
{
	test_group("RawBinary: one byte == one char");
	StrVal	raw("hi\xFF\xFEbye", StrRawBinary);
	expect_eq("raw length == byte count", (long)raw.length(), 7);
	expect_eq("raw numBytes == byte count", (long)raw.numBytes(), 7);
	expect_eq("raw[2] is 0xFF", raw[2], (UCS4)0xFF);
	expect_eq("raw[3] is 0xFE", raw[3], (UCS4)0xFE);

	test_group("RawBinary: find works per-byte");
	expect_eq("find 0xFF", (long)raw.find((UCS4)0xFF), 2);

	test_group("RawBinary: substr works per-byte");
	expect_eq("raw substr", raw.substr(0, 2), StrVal("hi"));
}

/*
 * Malformed/illegal UTF-8 input is handled without crashing
 */
void
malformed_utf8_tests()
{
	test_group("Malformed UTF-8: lone continuation byte");
	// 0x80 is a continuation byte with no valid leader; must decode as an
	// "illegal" replacement UCS4 character (0x80000000 | byte) rather than crash.
	StrVal	bad("a\x80" "b");
	expect_eq("length still counts 3 chars", (long)bad.length(), 3);
	expect_eq("bad[0] is 'a'", bad[0], (UCS4)'a');
	expect_eq("bad[1] is illegal-marker for 0x80", bad[1], (UCS4)(0x80000000 | 0x80));
	expect_eq("bad[2] is 'b'", bad[2], (UCS4)'b');

	test_group("Malformed UTF-8: truncated multi-byte sequence at end of string");
	// 0xE4 introduces a 3-byte sequence but the string ends after 1 byte.
	StrVal	truncated("x\xE4");
	expect("truncated string doesn't crash on length()", truncated.length() >= 1);
	expect_eq("truncated[0] is 'x'", truncated[0], (UCS4)'x');
}

/*
 * Longer strings: exercise forward/backward scan and bookmark caching
 */
void
long_string_bookmark_tests()
{
	test_group("Long string: repeated multi-byte characters, indexed forward and back");
	// 30 repetitions of a 3-byte CJK character, so byte offset != char offset
	StrVal	longStr = StrVal("\xE4\xB8\xAD") * 30;
	expect_eq("long string char length", (long)longStr.length(), 30);
	expect_eq("long string byte length", (long)longStr.numBytes(), 90);

	// Access forward repeatedly (builds up a forward-search bookmark)
	for (int i = 0; i < 30; i++)
	{
		char	msg[32];
		snprintf(msg, sizeof(msg), "longStr[%d]", i);
		expect_eq(msg, longStr[i], (UCS4)0x4E2D);
	}

	// Access near the end, forcing a backward search from the end
	expect_eq("longStr near end [29]", longStr[29], (UCS4)0x4E2D);
	expect_eq("longStr near end [25]", longStr[25], (UCS4)0x4E2D);

	// Interleave a substring extraction well into the string
	expect_eq("mid substring", longStr.substr(10, 5), StrVal("\xE4\xB8\xAD") * 5);

	test_group("Long string: mixed content, find deep into the string");
	StrVal	haystack = StrVal("\xE4\xB8\xAD") * 20 + StrVal("needle") + StrVal("\xE4\xB8\xAD") * 20;
	expect_eq("find needle deep in Unicode haystack", (long)haystack.find(StrVal("needle")), 20);
}
