/*
 * Unicode Strings
 * Thorough test suite for the StrVal class
 *
 * Covers construction, indexing, substrings, search, concatenation,
 * mutation, case conversion, JSON escaping, integer parsing, and
 * copy-on-write sharing across the full character range: pure ASCII,
 * multi-byte BMP characters up to U+FFFF (excluding UTF-16 surrogates),
 * and characters above the BMP (emoji etc), plus malformed UTF-8 input.
 *
 * It also has a dedicated section, mixed_encoding_tests(), asserting the
 * *correct* semantics for operations that combine a UTF-8 StrVal with a
 * StrRawBinary (locale 8-bit) StrVal. Several of these currently FAIL:
 * the class only partially implements StrRawBinary interop (see the
 * "REVISIT: Handle StrRawBinary data" / "REVISIT: Only works if the
 * StrDataType matches" comments in strval.h). Those tests are left in
 * place, deliberately failing, to document and guard against the defects.
 *
 * malformed_utf8_tests()/illegal_byte_propagation_tests() require that
 * every byte of invalid UTF-8 - wherever it came from, and after passing
 * through any operation - is exposed losslessly as its own UCS4 "illegal"
 * codepoint (see UCS4IsIllegal()/UTF8EncodeIllegal() in char_encoding.h):
 * one invalid byte in, one illegal-marked character out, never merged with
 * a neighbour and never silently dropped.
 *
 * NOTE: toJSON()/asJSON() on a string containing an illegal-encoded byte
 * currently hits `assert(ch <= 0xFFFFF)` in strval.h and aborts the process
 * (the illegal-marker range 0x80000000-0x800000FF isn't recognised there).
 * That case is deliberately left untested here rather than exercised - a
 * proper non-aborting panic/error handler is planned separately; add the
 * JSON-losslessness test once that lands instead of crashing this binary.
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
void		unicode_range_tests();
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
void		illegal_byte_propagation_tests();
void		long_string_bookmark_tests();
void		mixed_encoding_tests();

int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	construction_tests();
	indexing_tests();
	unicode_range_tests();
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
	illegal_byte_propagation_tests();
	long_string_bookmark_tests();
	mixed_encoding_tests();

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
expect_eq_int(const char* when, long got, long want)
{
	char	detail[64];
	bool	ok = got == want;
	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted %ld got %ld)", want, got);
	report(when, ok, ok ? 0 : detail);
}

void
expect_eq_ch(const char* when, UCS4 got, UCS4 want)
{
	char	detail[64];
	bool	ok = got == want;
	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted 0x%X got 0x%X)", want, got);
	report(when, ok, ok ? 0 : detail);
}

// Compare the *content* of a StrVal against a plain UTF-8 C string
void
expect_eq_str(const char* when, StrVal got, const char* want)
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
	expect_eq_int("default-constructed numBytes", (long)empty.numBytes(), 0);

	test_group("Construction: from C string (ASCII)");
	StrVal	foo("foo");
	expect("\"foo\" is not empty", !foo.isEmpty());
	expect("\"foo\" bool is true", (bool)foo);
	expect_eq_int("\"foo\" length", (long)foo.length(), 3);
	expect_eq_int("\"foo\" numBytes", (long)foo.numBytes(), 3);
	expect_eq_str("\"foo\" round-trips through asUTF8", foo, "foo");

	test_group("Construction: empty C string");
	StrVal	emptyC("");
	expect("\"\" is empty", emptyC.isEmpty());
	expect_eq_int("\"\" length", (long)emptyC.length(), 0);

	test_group("Construction: 2-byte UTF-8 character (e-acute)");
	StrVal	eacute("caf\xC3\xA9");	// "café"
	expect_eq_int("café character length", (long)eacute.length(), 4);
	expect_eq_int("café byte length", (long)eacute.numBytes(), 5);
	expect_eq_ch("café[3] is U+00E9", eacute[3], (UCS4)0x00E9);

	test_group("Construction: 3-byte UTF-8 characters (CJK)");
	StrVal	cjk("\xE4\xB8\xAD\xE6\x96\x87");	// "中文" (Chinese)
	expect_eq_int("中文 character length", (long)cjk.length(), 2);
	expect_eq_int("中文 byte length", (long)cjk.numBytes(), 6);
	expect_eq_ch("中文[0] is U+4E2D", cjk[0], (UCS4)0x4E2D);
	expect_eq_ch("中文[1] is U+6587", cjk[1], (UCS4)0x6587);

	test_group("Construction: 4-byte UTF-8 character (emoji)");
	StrVal	emoji("\xF0\x9F\x98\x80");	// U+1F600 GRINNING FACE
	expect_eq_int("emoji character length", (long)emoji.length(), 1);
	expect_eq_int("emoji byte length", (long)emoji.numBytes(), 4);
	expect_eq_ch("emoji[0] is U+1F600", emoji[0], (UCS4)0x1F600);

	test_group("Construction: single UCS4 character");
	StrVal	singleAscii((UCS4)'Q');
	expect_eq_int("single ASCII char length", (long)singleAscii.length(), 1);
	expect_eq_str("single ASCII char content", singleAscii, "Q");

	StrVal	singleWide((UCS4)0x4E2A);	// 个
	expect_eq_int("single wide char length", (long)singleWide.length(), 1);
	expect_eq_int("single wide char byte length", (long)singleWide.numBytes(), 3);
	expect_eq_str("single wide char content", singleWide, "\xE4\xB8\xAA");

	test_group("Construction: from length-delimited buffer (embedded NUL)");
	StrVal	embeddedNul("a\0b", (StrValIndex)3);
	expect_eq_int("embedded-NUL length", (long)embeddedNul.length(), 3);
	expect_eq_ch("embedded-NUL [0]", embeddedNul[0], (UCS4)'a');
	expect_eq_ch("embedded-NUL [1]", embeddedNul[1], (UCS4)0);
	expect_eq_ch("embedded-NUL [2]", embeddedNul[2], (UCS4)'b');

	test_group("Construction: with pre-allocation");
	StrVal	prealloc("hi", (StrValIndex)2, 40);
	expect_eq_int("prealloc length unaffected", (long)prealloc.length(), 2);
	expect_eq_str("prealloc content", prealloc, "hi");
	prealloc += " there";			// Should not need to reallocate, but must still work
	expect_eq_str("prealloc after append", prealloc, "hi there");

	test_group("Construction: copy constructor");
	StrVal	original("hello");
	StrVal	copy(original);
	expect_eq_str("copy has same content", copy, "hello");
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
	expect_eq_ch("s[0]", s[0], (UCS4)'a');
	expect_eq_ch("s[1]", s[1], (UCS4)'b');
	expect_eq_ch("s[2]", s[2], (UCS4)'c');
	expect_eq_ch("s[length()] is NUL", s[s.length()], (UCS4)0);
	expect_eq_ch("s[length()+3] out of range", s[s.length()+3], (UCS4)UCS4_NONE);
	expect_eq_ch("s[-1] out of range", s[-1], (UCS4)UCS4_NONE);

	test_group("Indexing: multi-byte string, character not byte offsets");
	// "a" + Greek alpha (2 bytes) + "b" + CJK "中" (3 bytes) + "c"
	StrVal	mixed("a\xCE\xB1" "b\xE4\xB8\xAD" "c");
	expect_eq_int("mixed character length", (long)mixed.length(), 5);
	expect_eq_int("mixed byte length", (long)mixed.numBytes(), 1+2+1+3+1);
	expect_eq_ch("mixed[0]", mixed[0], (UCS4)'a');
	expect_eq_ch("mixed[1] is alpha", mixed[1], (UCS4)0x03B1);
	expect_eq_ch("mixed[2]", mixed[2], (UCS4)'b');
	expect_eq_ch("mixed[3] is 中", mixed[3], (UCS4)0x4E2D);
	expect_eq_ch("mixed[4]", mixed[4], (UCS4)'c');
	expect_eq_ch("mixed[5] is NUL", mixed[5], (UCS4)0);
}

/*
 * Coverage across the whole character range: ASCII, BMP up to U+FFFF
 * (excluding surrogates), and above the BMP (emoji etc)
 */
void
unicode_range_tests()
{
	test_group("Unicode range: pure ASCII");
	StrVal	ascii("Plain ASCII 123");
	expect_eq_int("ascii char length == byte length", (long)ascii.length(), (long)ascii.numBytes());

	test_group("Unicode range: high BMP character just below the surrogate range (U+D7FF)");
	char	buf1[8];
	char*	p1 = buf1;
	UTF8Put(p1, (UCS4)0xD7FF);
	*p1 = '\0';
	StrVal	justBelowSurrogates(buf1);
	expect_eq_int("U+D7FF length", (long)justBelowSurrogates.length(), 1);
	expect_eq_ch("U+D7FF round-trips", justBelowSurrogates[0], (UCS4)0xD7FF);

	test_group("Unicode range: BMP character just above the surrogate range (U+E000)");
	char	buf2[8];
	char*	p2 = buf2;
	UTF8Put(p2, (UCS4)0xE000);
	*p2 = '\0';
	StrVal	justAboveSurrogates(buf2);
	expect_eq_int("U+E000 length", (long)justAboveSurrogates.length(), 1);
	expect_eq_ch("U+E000 round-trips", justAboveSurrogates[0], (UCS4)0xE000);

	test_group("Unicode range: maximum BMP character U+FFFF");
	char	buf3[8];
	char*	p3 = buf3;
	UTF8Put(p3, (UCS4)0xFFFF);
	*p3 = '\0';
	StrVal	maxBMP(buf3);
	expect_eq_int("U+FFFF length", (long)maxBMP.length(), 1);
	expect_eq_int("U+FFFF byte length", (long)maxBMP.numBytes(), 3);
	expect_eq_ch("U+FFFF round-trips", maxBMP[0], (UCS4)0xFFFF);

	test_group("Unicode range: above the BMP (emoji, astral plane)");
	StrVal	emoji("\xF0\x9F\x8E\x89\xF0\x9F\x8D\xBE");	// Party popper U+1F389, bottle U+1F37E
	expect_eq_int("emoji pair length", (long)emoji.length(), 2);
	expect_eq_ch("emoji[0]", emoji[0], (UCS4)0x1F389);
	expect_eq_ch("emoji[1]", emoji[1], (UCS4)0x1F37E);

	test_group("Unicode range: mixture of all classes in one string");
	StrVal	all("A\xCE\xB1\xE4\xB8\xAD\xF0\x9F\x98\x80");	// 'A', alpha, 中, emoji
	expect_eq_int("mixture length", (long)all.length(), 4);
	expect_eq_ch("mixture[0] ASCII", all[0], (UCS4)'A');
	expect_eq_ch("mixture[1] 2-byte", all[1], (UCS4)0x03B1);
	expect_eq_ch("mixture[2] 3-byte", all[2], (UCS4)0x4E2D);
	expect_eq_ch("mixture[3] 4-byte", all[3], (UCS4)0x1F600);
}

/*
 * Substrings: substr/head/tail/shorter
 */
void
substring_tests()
{
	test_group("Substrings: ASCII");
	StrVal	hw("Hello, world");
	expect_eq_str("substr(0,5)", hw.substr(0, 5), "Hello");
	expect_eq_str("substr(7,5)", hw.substr(7, 5), "world");
	expect_eq_str("substr(7)", hw.substr(7), "world");
	expect_eq_str("head(5)", hw.head(5), "Hello");
	expect_eq_str("tail(5)", hw.tail(5), "world");
	expect_eq_str("shorter(7)", hw.shorter(7), "Hello");

	test_group("Substrings: edge cases");
	expect("substr(at >= length) is empty", hw.substr(20, 3).isEmpty());
	expect("substr(at, 0) is empty", hw.substr(2, 0).isEmpty());
	expect("substr(at, -2) is empty (illegal len)", hw.substr(2, -2).isEmpty());
	expect_eq_str("substr(0, 999) clamps to whole string", hw.substr(0, 999), "Hello, world");
	expect_eq_str("substr(0, -1) is whole string", hw.substr(0, -1), "Hello, world");
	expect("tail(chars > length) is empty", hw.tail(hw.length()+5).isEmpty());
	expect("shorter(chars > length) is empty", hw.shorter(hw.length()+5).isEmpty());
	expect_eq_str("head(0) is empty", hw.head(0), "");

	test_group("Substrings: multi-byte character boundaries");
	// "中文测试" = 4 CJK characters, 3 bytes each
	StrVal	cjk("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB5\x8B\xE8\xAF\x95");
	expect_eq_str("cjk substr(1,2)", cjk.substr(1, 2), "\xE6\x96\x87\xE6\xB5\x8B");
	expect_eq_str("cjk head(1)", cjk.head(1), "\xE4\xB8\xAD");
	expect_eq_str("cjk tail(1)", cjk.tail(1), "\xE8\xAF\x95");
	expect_eq_int("cjk substr keeps byte-correct length", (long)cjk.substr(1, 2).numBytes(), 6);
}

/*
 * Character search: find/rfind
 */
void
find_char_tests()
{
	test_group("find(char)");
	StrVal	s("abcabc");
	expect_eq_int("find 'b'", (long)s.find('b'), 1);
	expect_eq_int("find 'b' after 1", (long)s.find('b', 1), 4);
	expect_eq_int("find 'z' (absent)", (long)s.find('z'), -1);
	expect_eq_int("find in empty string", (long)StrVal("").find('a'), -1);

	test_group("rfind(char)");
	expect_eq_int("rfind 'b'", (long)s.rfind('b'), 4);
	expect_eq_int("rfind 'b' before 4", (long)s.rfind('b', 4), 1);
	expect_eq_int("rfind 'z' (absent)", (long)s.rfind('z'), -1);
	expect_eq_int("rfind in empty string", (long)StrVal("").rfind('a'), -1);

	test_group("find/rfind(char) with Unicode content");
	StrVal	greek("\xCE\xB1\xCE\xB2\xCE\xB1");	// alpha beta alpha
	expect_eq_int("find alpha", (long)greek.find(0x03B1), 0);
	expect_eq_int("find alpha after 0", (long)greek.find(0x03B1, 0), 2);
	expect_eq_int("rfind alpha", (long)greek.rfind(0x03B1), 2);
	expect_eq_int("find beta", (long)greek.find(0x03B2), 1);
}

/*
 * Substring search: find/rfind
 */
void
find_substring_tests()
{
	test_group("find(StrVal)");
	StrVal	s("the quick brown fox jumps over the lazy dog");
	expect_eq_int("find \"quick\"", (long)s.find(StrVal("quick")), 4);
	expect_eq_int("find \"the\" (first)", (long)s.find(StrVal("the")), 0);
	expect_eq_int("find \"the\" after 0", (long)s.find(StrVal("the"), 0), 31);
	expect_eq_int("find \"zzz\" (absent)", (long)s.find(StrVal("zzz")), -1);
	expect_eq_int("find empty needle", (long)s.find(StrVal("")), 0);

	test_group("rfind(StrVal)");
	expect_eq_int("rfind \"the\"", (long)s.rfind(StrVal("the")), 31);
	expect_eq_int("rfind \"the\" before 31", (long)s.rfind(StrVal("the"), 31), 0);
	expect_eq_int("rfind \"zzz\" (absent)", (long)s.rfind(StrVal("zzz")), -1);

	test_group("find(StrVal) with Unicode content");
	StrVal	multi("\xE4\xB8\xAD\xE6\x96\x87 says \xCE\xB1\xCE\xB2");	// "中文 says αβ"
	expect_eq_int("find \xE6\x96\x87", (long)multi.find(StrVal("\xE6\x96\x87")), 1);
	expect_eq_int("find \"says\"", (long)multi.find(StrVal("says")), 3);
	expect_eq_int("find alpha-beta", (long)multi.find(StrVal("\xCE\xB1\xCE\xB2")), 8);
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
	expect_eq_int("findAny vowel", (long)s.findAny(vowels), 1);		// 'e'
	expect_eq_int("findAny vowel after 1", (long)s.findAny(vowels, 1), 4);	// 'o'
	expect_eq_int("rfindAny vowel", (long)s.rfindAny(vowels), 7);		// 'o' in "world"
	StrVal	digits("0123456789");
	expect_eq_int("findAny digit (absent)", (long)s.findAny(digits), -1);

	test_group("findNot/rfindNot");
	StrVal	padded("   xyz   ");
	StrVal	space(" ");
	expect_eq_int("findNot space", (long)padded.findNot(space), 3);
	expect_eq_int("rfindNot space", (long)padded.rfindNot(space), 5);
	StrVal	allSpace("   ");
	// nthChar(length()) is a valid position (the implicit NUL terminator), and the NUL
	// itself is "not in the set", so an all-matching string returns length(), not -1.
	expect_eq_int("findNot on all-matching string returns length() (the NUL terminator)",
			(long)allSpace.findNot(space), (long)allSpace.length());

	test_group("findAny/findNot with Unicode set");
	StrVal	mixedChars("a\xCE\xB1茶z");	// a, alpha, tea(茶), z
	StrVal	greekSet("\xCE\xB1\xCE\xB2\xCE\xB3");	// alpha beta gamma
	expect_eq_int("findAny greek letter", (long)mixedChars.findAny(greekSet), 1);
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
	expect_eq_str("a + b", a + b, "foobar");
	expect_eq_str("a unchanged after a+b", a, "foo");
	expect_eq_str("b unchanged after a+b", b, "bar");

	test_group("operator+ (StrVal + const char*)");
	expect_eq_str("a + \"baz\"", a + "baz", "foobaz");

	test_group("operator+ (const char* + StrVal)");
	expect_eq_str("\"baz\" + a", "baz" + a, "bazfoo");

	test_group("operator+ (StrVal + UCS4)");
	expect_eq_str("a + '!'", a + (UCS4)'!', "foo!");
	StrVal	greekBase("x");
	expect_eq_str("x + alpha", greekBase + (UCS4)0x03B1, "x\xCE\xB1");

	test_group("operator+= (StrVal)");
	StrVal	acc("foo");
	acc += b;
	expect_eq_str("acc after += StrVal", acc, "foobar");

	test_group("operator+= (UCS4)");
	StrVal	acc2("foo");
	acc2 += (UCS4)'!';
	expect_eq_str("acc2 after += UCS4", acc2, "foo!");

	test_group("operator+= on empty string");
	StrVal	empty;
	empty += StrVal("grown");
	expect_eq_str("empty += grows correctly", empty, "grown");

	test_group("Concatenation of contiguous slices (fast path)");
	StrVal	hw("Hello, world");
	StrVal	hello = hw.substr(0, 5);
	StrVal	comma = hw.substr(5, 2);
	StrVal	world = hw.substr(7, 5);
	StrVal	reassembled = hello + comma;
	reassembled += world;
	expect_eq_str("reassembled contiguous slices", reassembled, "Hello, world");
}

/*
 * Repetition: operator*
 */
void
repeat_tests()
{
	test_group("operator* (repeat)");
	StrVal	ab("ab");
	expect_eq_str("ab * 3", ab*3, "ababab");
	expect_eq_str("ab * 1", ab*1, "ab");
	expect_eq_str("ab * 0", ab*0, "");
	expect_eq_str("ab unchanged after *", ab, "ab");

	StrVal	greek("\xCE\xB1");	// alpha
	expect_eq_str("alpha * 3", greek*3, "\xCE\xB1\xCE\xB1\xCE\xB1");
	expect_eq_int("alpha * 3 char length", (long)(greek*3).length(), 3);
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
	expect_eq_str("after prepend", s, "hello world");
	s.append(StrVal("!"));
	expect_eq_str("after append", s, "hello world!");

	test_group("insert at arbitrary position");
	StrVal	s2("helloworld");
	s2.insert(5, StrVal(", "));
	expect_eq_str("insert in the middle", s2, "hello, world");

	test_group("insert at start / end");
	StrVal	s3("bc");
	s3.insert(0, StrVal("a"));
	expect_eq_str("insert at start", s3, "abc");
	s3.insert(s3.length(), StrVal("d"));
	expect_eq_str("insert at end", s3, "abcd");

	test_group("insert into Unicode content preserves character boundaries");
	StrVal	cjk("\xE4\xB8\xAD\xE6\x96\x87");	// 中文
	cjk.insert(1, StrVal("-"));
	expect_eq_str("insert between CJK chars", cjk, "\xE4\xB8\xAD-\xE6\x96\x87");
	expect_eq_int("insert between CJK chars, char length", (long)cjk.length(), 3);

	test_group("insert does not corrupt an unrelated shared copy");
	StrVal	base("shared");
	StrVal	aliased(base);
	aliased.append(StrVal(" mutated"));
	expect_eq_str("base is untouched by append on the alias", base, "shared");
	expect_eq_str("aliased reflects the append", aliased, "shared mutated");
}

/*
 * Case conversion
 */
void
case_conversion_tests()
{
	test_group("asLower/asUpper (ASCII), non-mutating");
	StrVal	mixed("Hello World");
	expect_eq_str("asLower", mixed.asLower(), "hello world");
	expect_eq_str("asUpper", mixed.asUpper(), "HELLO WORLD");
	expect_eq_str("original unchanged after asLower/asUpper", mixed, "Hello World");

	test_group("toLower/toUpper (ASCII), mutating");
	StrVal	m1("MixedCase");
	m1.toLower();
	expect_eq_str("toLower mutates in place", m1, "mixedcase");
	StrVal	m2("MixedCase");
	m2.toUpper();
	expect_eq_str("toUpper mutates in place", m2, "MIXEDCASE");

	test_group("Case conversion: Latin-1 accented characters");
	StrVal	cafe("caf\xC3\xA9");			// café
	expect_eq_str("café upper", cafe.asUpper(), "CAF\xC3\x89");	// CAFÉ
	StrVal	CAFE("CAF\xC3\x89");			// CAFÉ
	expect_eq_str("CAFÉ lower", CAFE.asLower(), "caf\xC3\xA9");	// café

	test_group("Case conversion: Greek");
	StrVal	greekLower("\xCE\xB1\xCE\xB2\xCE\xB3");	// alpha beta gamma
	expect_eq_str("greek upper", greekLower.asUpper(), "\xCE\x91\xCE\x92\xCE\x93");	// ALPHA BETA GAMMA
	StrVal	greekUpper("\xCE\x91\xCE\x92\xCE\x93");
	expect_eq_str("greek lower", greekUpper.asLower(), "\xCE\xB1\xCE\xB2\xCE\xB3");

	test_group("Case conversion: already-lower/upper is idempotent");
	StrVal	lower("already lower");
	expect_eq_str("lower.asLower() unchanged", lower.asLower(), "already lower");
	StrVal	upper("ALREADY UPPER");
	expect_eq_str("upper.asUpper() unchanged", upper.asUpper(), "ALREADY UPPER");

	test_group("Case conversion: mixed ASCII/non-ASCII content");
	StrVal	mix("Caf\xC3\xA9 123!");
	expect_eq_str("mixed content upper", mix.asUpper(), "CAF\xC3\x89 123!");
}

/*
 * JSON escaping (toJSON/asJSON)
 */
void
json_tests()
{
	test_group("JSON: control character escapes");
	expect_eq_str("newline", StrVal("a\nb").asJSON(), "a\\nb");
	expect_eq_str("tab", StrVal("a\tb").asJSON(), "a\\tb");
	expect_eq_str("carriage return", StrVal("a\rb").asJSON(), "a\\rb");
	expect_eq_str("backspace", StrVal("a\bb").asJSON(), "a\\bb");
	expect_eq_str("form feed", StrVal("a\fb").asJSON(), "a\\fb");
	expect_eq_str("backslash", StrVal("a\\b").asJSON(), "a\\\\b");
	expect_eq_str("double quote", StrVal("a\"b").asJSON(), "a\\\"b");
	expect_eq_str("forward slash", StrVal("a/b").asJSON(), "a\\/b");
	expect_eq_str("NUL byte", StrVal("a\0b", (StrValIndex)3).asJSON(), "a\\0b");

	test_group("JSON: other control chars use \\u escape");
	expect_eq_str("SOH (0x01)", StrVal("a\x01" "b").asJSON(), "a\\u0001b");

	test_group("JSON: printable ASCII is untouched");
	expect_eq_str("plain ASCII", StrVal("Hello, World! 123").asJSON(), "Hello, World! 123");

	test_group("JSON: BMP Unicode characters are left as literal UTF-8");
	expect_eq_str("café literal in JSON", StrVal("caf\xC3\xA9").asJSON(), "caf\xC3\xA9");
	expect_eq_str("CJK literal in JSON", StrVal("\xE4\xB8\xAD\xE6\x96\x87").asJSON(), "\xE4\xB8\xAD\xE6\x96\x87");

	test_group("JSON: astral characters (emoji) become surrogate pair escapes");
	// U+1F600 -> high D83D, low DE00
	expect_eq_str("emoji surrogate pair", StrVal("\xF0\x9F\x98\x80").asJSON(), "\\uD83D\\uDE00");

	test_group("JSON: non-mutating asJSON leaves original intact");
	StrVal	orig("a\nb");
	StrVal	json = orig.asJSON();
	expect_eq_str("original unaffected by asJSON", orig, "a\nb");
	expect_eq_str("asJSON result correct", json, "a\\nb");

	test_group("JSON: toJSON mutates in place");
	StrVal	mut("tab\there");
	mut.toJSON();
	expect_eq_str("toJSON mutated", mut, "tab\\there");
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
	expect_eq_int("\"123\"", (long)StrVal("123").asInt32(&err), 123);
	expect_eq_err("\"123\" err", err, ErrNum(0));
	expect_eq_int("\"-42\"", (long)StrVal("-42").asInt32(&err), -42);
	expect_eq_err("\"-42\" err", err, ErrNum(0));
	expect_eq_int("\"+42\"", (long)StrVal("+42").asInt32(&err), 42);
	expect_eq_int("\"0\"", (long)StrVal("0").asInt32(&err), 0);

	test_group("asInt32: leading/trailing whitespace ignored");
	expect_eq_int("\"  42  \"", (long)StrVal("  42  ").asInt32(&err), 42);
	expect_eq_err("\"  42  \" err", err, ErrNum(0));

	test_group("asInt32: radix auto-detection (radix 0)");
	expect_eq_int("\"0x1A\" hex", (long)StrVal("0x1A").asInt32(&err, 0), 26);
	expect_eq_int("\"0X1a\" hex lowercase digits", (long)StrVal("0X1a").asInt32(&err, 0), 26);
	expect_eq_int("\"0b101\" binary", (long)StrVal("0b101").asInt32(&err, 0), 5);
	expect_eq_int("\"010\" octal", (long)StrVal("010").asInt32(&err, 0), 8);
	expect_eq_int("\"10\" decimal (no leading zero)", (long)StrVal("10").asInt32(&err, 0), 10);

	test_group("asInt32: explicit radix overrides auto-detection of octal");
	expect_eq_int("\"010\" radix 10", (long)StrVal("010").asInt32(&err, 10), 10);

	test_group("asInt32: explicit radix 16/2/36");
	expect_eq_int("\"FF\" radix 16", (long)StrVal("FF").asInt32(&err, 16), 255);
	expect_eq_int("\"ff\" radix 16", (long)StrVal("ff").asInt32(&err, 16), 255);
	expect_eq_int("\"101\" radix 2", (long)StrVal("101").asInt32(&err, 2), 5);
	expect_eq_int("\"Z\" radix 36", (long)StrVal("Z").asInt32(&err, 36), 35);

	test_group("asInt32: error - no digits (blank string)");
	StrVal("   ").asInt32(&err, 10, &scanned);
	expect_eq_err("all-blank err", err, ErrNum(STRERR_SET, STRERR_NO_DIGITS));

	test_group("asInt32: error - not a number");
	StrVal("abc").asInt32(&err, 0, &scanned);
	expect_eq_err("\"abc\" err", err, ErrNum(STRERR_SET, STRERR_NOT_NUMBER));
	expect_eq_int("\"abc\" scanned", (long)scanned, 0);

	test_group("asInt32: error - trailing text");
	long	tv = StrVal("12x").asInt32(&err, 0, &scanned);
	expect_eq_int("\"12x\" value still parsed", tv, 12);
	expect_eq_err("\"12x\" err", err, ErrNum(STRERR_SET, STRERR_TRAIL_TEXT));
	expect_eq_int("\"12x\" scanned", (long)scanned, 2);

	test_group("asInt32: error - illegal radix");
	StrVal("123").asInt32(&err, 37, &scanned);
	expect_eq_err("radix 37 err", err, ErrNum(STRERR_SET, STRERR_ILLEGAL_RADIX));
	StrVal("123").asInt32(&err, -1, &scanned);
	expect_eq_err("radix -1 err", err, ErrNum(STRERR_SET, STRERR_ILLEGAL_RADIX));

	test_group("asInt32: error - overflow (exceeds unsigned long)");
	StrVal("99999999999999999999999999").asInt32(&err, 10, &scanned);
	expect_eq_err("huge decimal overflow err", err, ErrNum(STRERR_SET, STRERR_NUMBER_OVERFLOW));

	test_group("asInt32: error - overflow (exceeds signed range, unsigned still fits)");
	// LONG_MAX + 1: fits an unsigned long, but not a signed one
	char	buf[32];
	snprintf(buf, sizeof(buf), "%lu", (unsigned long)LONG_MAX + 1);
	StrVal(buf).asInt32(&err, 10, &scanned);
	expect_eq_err("LONG_MAX+1 overflow err", err, ErrNum(STRERR_SET, STRERR_NUMBER_OVERFLOW));
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
	expect_eq_str("original unaffected by copy's toUpper", original, "original");
	expect_eq_str("copy reflects toUpper", aCopy, "ORIGINAL");

	test_group("COW: mutating the original doesn't affect a copy");
	StrVal	orig2("base");
	StrVal	copy2(orig2);
	orig2 += "-mutated";
	expect_eq_str("orig2 mutated", orig2, "base-mutated");
	expect_eq_str("copy2 unaffected", copy2, "base");

	test_group("COW: substrings are independent once mutated");
	StrVal	whole("abcdef");
	StrVal	left = whole.substr(0, 3);
	StrVal	right = whole.substr(3, 3);
	left.toUpper();
	expect_eq_str("left mutated", left, "ABC");
	expect_eq_str("right unaffected", right, "def");
	expect_eq_str("whole unaffected", whole, "abcdef");

	test_group("COW: assignment shares then diverges on mutation");
	StrVal	src("shared value");
	StrVal	dst;
	dst = src;
	expect_eq_str("dst equals src after assignment", dst, "shared value");
	dst.append(StrVal(" more"));
	expect_eq_str("dst diverged after mutation", dst, "shared value more");
	expect_eq_str("src unaffected by dst mutation", src, "shared value");
}

/*
 * StrRawBinary data, used alone (no mixing with UTF8 strings)
 */
void
raw_binary_tests()
{
	test_group("RawBinary: one byte == one char (ASCII-range content)");
	StrVal	raw("hi\x7F" "bye", StrRawBinary);
	expect_eq_int("raw length == byte count", (long)raw.length(), 6);
	expect_eq_int("raw numBytes == byte count", (long)raw.numBytes(), 6);
	expect_eq_ch("raw[2] is 0x7F", raw[2], (UCS4)0x7F);

	test_group("RawBinary: find works per-byte");
	expect_eq_int("find 0x7F", (long)raw.find((UCS4)0x7F), 2);

	test_group("RawBinary: substr works per-byte");
	expect_eq_str("raw substr", raw.substr(0, 2), "hi");
}

/*
 * Malformed/illegal UTF-8 input must be handled losslessly: every invalid
 * byte becomes its own UCS4 "illegal" codepoint (UTF8EncodeIllegal(byte),
 * satisfying UCS4IsIllegal()), never merged with a neighbour, never
 * silently dropped, and well-formed characters around it must decode
 * normally. Every expectation below was confirmed against the actual
 * library behaviour with a standalone diagnostic before being written
 * here as a permanent regression test - this whole group currently passes.
 */
void
malformed_utf8_tests()
{
	test_group("Malformed UTF-8: lone continuation byte");
	// 0x80 is a continuation byte with no valid leader.
	StrVal	bad("a\x80" "b");
	expect_eq_int("length still counts 3 chars", (long)bad.length(), 3);
	expect_eq_ch("bad[0] is 'a'", bad[0], (UCS4)'a');
	expect_eq_ch("bad[1] is illegal-marker for 0x80", bad[1], UTF8EncodeIllegal((UTF8)0x80));
	expect("bad[1] satisfies UCS4IsIllegal", UCS4IsIllegal(bad[1]));
	expect_eq_ch("bad[2] is 'b'", bad[2], (UCS4)'b');

	test_group("Malformed UTF-8: three consecutive lone continuation bytes stay separate");
	StrVal	threeBad("\x80\x81\x82");
	expect_eq_int("three lone continuations -> three chars, not merged", (long)threeBad.length(), 3);
	expect_eq_ch("threeBad[0]", threeBad[0], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("threeBad[1]", threeBad[1], UTF8EncodeIllegal((UTF8)0x81));
	expect_eq_ch("threeBad[2]", threeBad[2], UTF8EncodeIllegal((UTF8)0x82));

	test_group("Malformed UTF-8: truncated multi-byte sequence at end of string");
	// 0xE4 introduces a 3-byte sequence but the string ends after 1 byte.
	StrVal	truncated("x\xE4");
	expect_eq_int("truncated string still counts both bytes", (long)truncated.length(), 2);
	expect_eq_ch("truncated[0] is 'x'", truncated[0], (UCS4)'x');
	expect_eq_ch("truncated[1] is illegal-marker for 0xE4", truncated[1], UTF8EncodeIllegal((UTF8)0xE4));

	test_group("Malformed UTF-8: 3-byte leader + 1 valid continuation, then end of string");
	// The leader promises 2 continuation bytes but only 1 follows before the string ends;
	// both the leader byte and the stranded continuation byte surface as their own
	// illegal character - none of the input bytes are lost.
	StrVal	leaderPlusOne("\xE4\x80");
	expect_eq_int("leader+1-continuation -> 2 illegal chars", (long)leaderPlusOne.length(), 2);
	expect_eq_ch("leaderPlusOne[0]", leaderPlusOne[0], UTF8EncodeIllegal((UTF8)0xE4));
	expect_eq_ch("leaderPlusOne[1]", leaderPlusOne[1], UTF8EncodeIllegal((UTF8)0x80));

	test_group("Malformed UTF-8: same, followed by a valid ASCII character");
	StrVal	leaderPlusOneThenAscii("\xE4\x80" "z");
	expect_eq_int("length is 3 (2 illegal + 1 ASCII)", (long)leaderPlusOneThenAscii.length(), 3);
	expect_eq_ch("[0] illegal 0xE4", leaderPlusOneThenAscii[0], UTF8EncodeIllegal((UTF8)0xE4));
	expect_eq_ch("[1] illegal 0x80", leaderPlusOneThenAscii[1], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("[2] 'z' decodes normally", leaderPlusOneThenAscii[2], (UCS4)'z');

	test_group("Malformed UTF-8: positive control - a complete valid 3-byte sequence is NOT illegal");
	StrVal	validCJK("\xE4\xB8\xAD");	// 中, U+4E2D
	expect_eq_int("valid 3-byte sequence is 1 char", (long)validCJK.length(), 1);
	expect_eq_ch("decodes to U+4E2D", validCJK[0], (UCS4)0x4E2D);
	expect("a well-formed character does not satisfy UCS4IsIllegal", !UCS4IsIllegal(validCJK[0]));

	test_group("Malformed UTF-8: truncated 4-byte sequence at end of string");
	StrVal	truncated4("\xF0\x9F");
	expect_eq_int("leader+1-continuation (4-byte lead) -> 2 illegal chars", (long)truncated4.length(), 2);
	expect_eq_ch("truncated4[0]", truncated4[0], UTF8EncodeIllegal((UTF8)0xF0));
	expect_eq_ch("truncated4[1]", truncated4[1], UTF8EncodeIllegal((UTF8)0x9F));

	test_group("Malformed UTF-8: truncated 4-byte sequence (2 of 3 continuations) then ASCII");
	StrVal	truncated4b("\xF0\x9F\x98" "z");
	expect_eq_int("length is 4 (3 illegal + 1 ASCII)", (long)truncated4b.length(), 4);
	expect_eq_ch("[0] illegal 0xF0", truncated4b[0], UTF8EncodeIllegal((UTF8)0xF0));
	expect_eq_ch("[1] illegal 0x9F", truncated4b[1], UTF8EncodeIllegal((UTF8)0x9F));
	expect_eq_ch("[2] illegal 0x98", truncated4b[2], UTF8EncodeIllegal((UTF8)0x98));
	expect_eq_ch("[3] 'z' decodes normally", truncated4b[3], (UCS4)'z');

	test_group("Malformed UTF-8: two invalid leader bytes back-to-back stay separate");
	StrVal	twoLeaders("\xE4\xE4");
	expect_eq_int("two bad leaders -> 2 chars, not merged", (long)twoLeaders.length(), 2);
	expect_eq_ch("twoLeaders[0]", twoLeaders[0], UTF8EncodeIllegal((UTF8)0xE4));
	expect_eq_ch("twoLeaders[1]", twoLeaders[1], UTF8EncodeIllegal((UTF8)0xE4));

	test_group("Malformed UTF-8: bytes that can never start a valid UTF-8 sequence");
	StrVal	byteFF("\xFF");
	expect_eq_int("0xFF is 1 illegal char", (long)byteFF.length(), 1);
	expect_eq_ch("byteFF[0]", byteFF[0], UTF8EncodeIllegal((UTF8)0xFF));
	StrVal	byteFE("\xFE");
	expect_eq_int("0xFE is 1 illegal char", (long)byteFE.length(), 1);
	expect_eq_ch("byteFE[0]", byteFE[0], UTF8EncodeIllegal((UTF8)0xFE));

	test_group("Malformed UTF-8: realistic mix of valid and invalid bytes, nothing lost");
	StrVal	mix("a\x80" "b\x80\x81" "c");
	expect_eq_int("mix length is 6 (a, illegal, b, illegal, illegal, c)", (long)mix.length(), 6);
	expect_eq_ch("mix[0] 'a'", mix[0], (UCS4)'a');
	expect_eq_ch("mix[1] illegal 0x80", mix[1], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("mix[2] 'b'", mix[2], (UCS4)'b');
	expect_eq_ch("mix[3] illegal 0x80", mix[3], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("mix[4] illegal 0x81", mix[4], UTF8EncodeIllegal((UTF8)0x81));
	expect_eq_ch("mix[5] 'c'", mix[5], (UCS4)'c');
}

/*
 * --------------------------------------------------------------------
 * Operations that copy or derive content from a StrVal already containing
 * illegal-encoded bytes must propagate them losslessly too, not just plain
 * construction. Confirmed against the actual library behaviour before
 * being written here; this whole group currently passes.
 * --------------------------------------------------------------------
 */
void
illegal_byte_propagation_tests()
{
	// a, illegal(0x80), b, illegal(0xE4) [truncated leader], c
	StrVal	bad("a\x80" "b\xE4" "c");

	test_group("Propagation: substr()/tail() preserve illegal markers at the right positions");
	expect_eq_int("bad has 5 chars", (long)bad.length(), 5);
	StrVal	first3 = bad.substr(0, 3);
	expect_eq_int("first3 length", (long)first3.length(), 3);
	expect_eq_ch("first3[1] illegal 0x80", first3[1], UTF8EncodeIllegal((UTF8)0x80));
	StrVal	last3 = bad.substr(2, 3);
	expect_eq_int("last3 length", (long)last3.length(), 3);
	expect_eq_ch("last3[1] illegal 0xE4", last3[1], UTF8EncodeIllegal((UTF8)0xE4));
	StrVal	tailed = bad.tail(2);
	expect_eq_ch("tail(2)[0] illegal 0xE4", tailed[0], UTF8EncodeIllegal((UTF8)0xE4));
	expect_eq_ch("tail(2)[1] 'c'", tailed[1], (UCS4)'c');

	test_group("Propagation: operator+ concatenation");
	StrVal	concatenated = bad + StrVal("XYZ");
	expect_eq_int("concatenated length is 8", (long)concatenated.length(), 8);
	expect_eq_ch("concatenated[1] illegal 0x80", concatenated[1], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("concatenated[3] illegal 0xE4", concatenated[3], UTF8EncodeIllegal((UTF8)0xE4));
	expect_eq_ch("concatenated[5] 'X'", concatenated[5], (UCS4)'X');

	test_group("Propagation: prepend()");
	StrVal	prep("XYZ");
	prep.prepend(bad);
	expect_eq_int("prepend result length is 8", (long)prep.length(), 8);
	expect_eq_ch("prep[1] illegal 0x80", prep[1], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("prep[3] illegal 0xE4", prep[3], UTF8EncodeIllegal((UTF8)0xE4));

	test_group("Propagation: insert() into the middle of another string");
	StrVal	ins("12345");
	ins.insert(2, bad);
	expect_eq_int("insert result length is 10", (long)ins.length(), 10);
	expect_eq_ch("ins[3] illegal 0x80", ins[3], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("ins[5] illegal 0xE4", ins[5], UTF8EncodeIllegal((UTF8)0xE4));

	test_group("Propagation: operator* (repeat)");
	StrVal	repeated = bad * 2;
	expect_eq_int("repeated length is 10", (long)repeated.length(), 10);
	expect_eq_ch("repeated[1] illegal 0x80 (1st copy)", repeated[1], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("repeated[6] illegal 0x80 (2nd copy)", repeated[6], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("repeated[8] illegal 0xE4 (2nd copy)", repeated[8], UTF8EncodeIllegal((UTF8)0xE4));

	test_group("Propagation: toLower()/toUpper() pass illegal bytes through unchanged");
	StrVal	lowerSrc("A\x80" "B\xE4" "C");
	StrVal	lowered = lowerSrc;
	lowered.toLower();
	expect_eq_int("lowered length unchanged", (long)lowered.length(), 5);
	expect_eq_ch("lowered[0] 'a'", lowered[0], (UCS4)'a');
	expect_eq_ch("lowered[1] illegal 0x80 untouched", lowered[1], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("lowered[2] 'b'", lowered[2], (UCS4)'b');
	expect_eq_ch("lowered[3] illegal 0xE4 untouched", lowered[3], UTF8EncodeIllegal((UTF8)0xE4));
	expect_eq_ch("lowered[4] 'c'", lowered[4], (UCS4)'c');

	StrVal	upperSrc("a\x80" "b\xE4" "c");
	StrVal	uppered = upperSrc;
	uppered.toUpper();
	expect_eq_int("uppered length unchanged", (long)uppered.length(), 5);
	expect_eq_ch("uppered[0] 'A'", uppered[0], (UCS4)'A');
	expect_eq_ch("uppered[1] illegal 0x80 untouched", uppered[1], UTF8EncodeIllegal((UTF8)0x80));
	expect_eq_ch("uppered[2] 'B'", uppered[2], (UCS4)'B');
	expect_eq_ch("uppered[3] illegal 0xE4 untouched", uppered[3], UTF8EncodeIllegal((UTF8)0xE4));
	expect_eq_ch("uppered[4] 'C'", uppered[4], (UCS4)'C');
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
	expect_eq_int("long string char length", (long)longStr.length(), 30);
	expect_eq_int("long string byte length", (long)longStr.numBytes(), 90);

	// Access forward repeatedly (builds up a forward-search bookmark)
	for (int i = 0; i < 30; i++)
	{
		char	msg[32];
		snprintf(msg, sizeof(msg), "longStr[%d]", i);
		expect_eq_ch(msg, longStr[i], (UCS4)0x4E2D);
	}

	// Access near the end, forcing a backward search from the end
	expect_eq_ch("longStr near end [29]", longStr[29], (UCS4)0x4E2D);
	expect_eq_ch("longStr near end [25]", longStr[25], (UCS4)0x4E2D);

	// Interleave a substring extraction well into the string
	StrVal	wantMid = StrVal("\xE4\xB8\xAD") * 5;
	expect_eq_str("mid substring", longStr.substr(10, 5), wantMid.asUTF8());

	test_group("Long string: mixed content, find deep into the string");
	StrVal	haystack = StrVal("\xE4\xB8\xAD") * 20 + StrVal("needle") + StrVal("\xE4\xB8\xAD") * 20;
	expect_eq_int("find needle deep in Unicode haystack", (long)haystack.find(StrVal("needle")), 20);
}

/*
 * Mixed encoding: operations combining a UTF-8 StrVal with a StrRawBinary
 * (locale 8-bit) StrVal.
 *
 * The RawBinary encoding stores one byte per character, and the code that
 * reads it (StrBodyI::getChar) treats the byte value directly as a UCS4
 * codepoint - i.e. it assumes the locale's 8-bit encoding is ISO-8859-1
 * (Latin-1), whose codepoints 0-255 are identical to Unicode's. All the
 * "correct" expectations below are computed on that basis.
 *
 * Every group here documents a currently-real defect (confirmed by direct
 * inspection of the actual output before writing the assertion), each
 * traceable to a specific gap in strval.h:
 *
 *   - operator[] on RawBinary data sign-extends the raw byte through a
 *     signed `char`, corrupting any byte >= 0x80.
 *   - operator+, insert/append/prepend do not preserve the source
 *     StrDataType (see "REVISIT: Handle StrRawBinary data" at each site);
 *     bytes are copied verbatim and then (re)interpreted under the
 *     destination's own encoding tag, corrupting non-ASCII content.
 *   - find/rfind(StrVal) compare raw bytes only (see "REVISIT: Only works
 *     if the StrDataType matches"), so a character does not match itself
 *     when the two sides use different encodings.
 *   - operator==/compare() is likewise a raw byte compare, so logically
 *     identical text compares unequal across encodings.
 *   - toLower/toUpper's internal transform() steps its input pointer using
 *     UTF8Len() regardless of source encoding, so it can misinterpret
 *     RawBinary byte sequences as UTF-8 lead/continuation bytes and skip
 *     or destroy characters.
 */
void
mixed_encoding_tests()
{
	test_group("RawBinary fundamentals: operator[] must return the raw byte value (0-255)");
	StrVal	highByte("\xE9", StrRawBinary);		// Latin-1 'e' with acute accent = U+00E9
	expect_eq_ch("high raw byte value must be U+00E9, not sign-extended", highByte[0], (UCS4)0x00E9);

	test_group("Mixed: RawBinary + UTF8 concatenation must convert the RawBinary side to proper UTF-8");
	StrVal	combined = highByte + StrVal(" more");
	expect_eq_int("combined length is 6 characters", (long)combined.length(), 6);
	expect_eq_ch("combined[0] is the correct Latin-1 codepoint U+00E9",
			combined[0], (UCS4)0x00E9);
	expect_eq_str("combined content is correctly formed UTF-8 \"\xC3\xA9 more\"",
			combined, "\xC3\xA9 more");

	test_group("Mixed: prepending a RawBinary fragment onto a UTF8 string");
	StrVal	cafeBase("caf\xC3\xA9");			// café, proper UTF8
	StrVal	prepended = cafeBase;
	prepended.prepend(StrVal("\xE9", StrRawBinary));	// prepend raw e-acute
	expect_eq_int("prepended length is 5 characters", (long)prepended.length(), 5);
	expect_eq_ch("prepended[0] is the correct Latin-1 codepoint U+00E9",
			prepended[0], (UCS4)0x00E9);
	expect_eq_str("prepended content is correctly formed UTF-8",
			prepended, "\xC3\xA9" "caf\xC3\xA9");

	test_group("Mixed: appending proper UTF8 (multi-byte) content onto a RawBinary receiver");
	StrVal	rawAscii("num=", StrRawBinary);		// 4 ASCII bytes, RawBinary-tagged
	StrVal	cafe("caf\xC3\xA9");			// café: 4 chars, 5 bytes, proper UTF8
	StrVal	appended = rawAscii;
	appended.append(cafe);
	StrVal	want = StrVal("num=") + cafe;		// both plain UTF8, so this concatenation is not itself buggy
	expect_eq_int("appended char length is 8 (4 ASCII + 4 from café)", (long)appended.length(), 8);
	expect_eq_int("appended byte length is 9 (4 ASCII + 5 UTF8 bytes)", (long)appended.numBytes(), 9);
	expect_eq_ch("appended[7] is the correct codepoint U+00E9 (e-acute)",
			appended[7], (UCS4)0x00E9);
	expect_eq_str("appended content equals \"num=caf\xC3\xA9\"", appended, want.asUTF8());

	test_group("Mixed: find(StrVal) must find a character across differing encodings of the same text");
	StrVal	haystackUtf8("caf\xC3\xA9 shop");		// UTF8 "café shop"
	StrVal	needleRaw("\xE9", StrRawBinary);		// same character, RawBinary
	expect_eq_int("UTF8 haystack finds a RawBinary needle representing the same character",
			(long)haystackUtf8.find(needleRaw), 3);

	StrVal	haystackRaw("caf\xE9 shop", StrRawBinary);	// same text, entirely RawBinary
	StrVal	needleUtf8("\xC3\xA9");			// same character, proper UTF8
	expect_eq_int("RawBinary haystack finds a UTF8 needle representing the same character",
			(long)haystackRaw.find(needleUtf8), 3);

	test_group("Mixed: operator==/compare() must treat logically identical text as equal across encodings");
	StrVal	rawE("\xE9", StrRawBinary);			// U+00E9, one raw byte
	StrVal	utf8E("\xC3\xA9");				// U+00E9, proper UTF8
	expect("RawBinary 'e-acute' == UTF8 'e-acute' (same logical character)", rawE == utf8E);

	test_group("Mixed: toLower/toUpper on RawBinary data must not lose or corrupt characters");
	// Two independent Latin-1 characters: U+00C3 (A-tilde) and U+0089 (a C1 control code).
	// Byte 0xC3 looks like a UTF-8 2-byte lead, and 0x89 looks like a valid UTF-8
	// continuation byte, which is exactly the pattern that trips up transform()'s
	// UTF8Len()-based pointer stepping when the source is really RawBinary.
	StrVal	rawTwoChars("\xC3\x89", StrRawBinary);
	expect_eq_int("rawTwoChars starts with 2 characters", (long)rawTwoChars.length(), 2);
	StrVal	lowered = rawTwoChars;
	lowered.toLower();
	expect_eq_int("toLower() must preserve the character count (2)", (long)lowered.length(), 2);
	expect_eq_ch("lowered[0] is U+00E3 (a-tilde, lowercased)", lowered[0], (UCS4)0x00E3);
	expect_eq_ch("lowered[1] is U+0089 (unchanged, no lowercase mapping)", lowered[1], (UCS4)0x0089);
}
