/*
 * Test program for the character encodings and classifications.
 * Only UTF-8 implemented at present.
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<cstring>
#include	<char_encoding.h>

bool		show_passes = false;
int		test_count;
int		failure_count;
const char*	new_group;

void		utf8_char_sizes();
void		utf8_get_backup();
void		utf8_invalid();
void		utf8_eof();
void		utf8_alphabetic();
void		utf8_numeric();
void		utf8_case_conversions();

UCS4		max_1byte = (0x1<<7)-1;				// 0x7F
UCS4		max_2byte = (0x1<<11)-1;			// 0x7FF
UCS4		max_3byte = (0x1<<16)-1;			// 0xFFFF
UCS4		max_4byte = (0x1<<21)-1;			// 0x1FFFFF
UCS4		max_5byte = (0x1<<26)-1;			// 0x3FFFFFF
UCS4		max_6byte = ((unsigned long)0x1<<32)-2;		// 0xFFFFFFFE

int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	utf8_char_sizes();	// Test the different encoded character lengths
	utf8_get_backup();	// Test get and backup
	utf8_invalid();		// Test isolated leading and trailing encoded bytes
	utf8_eof();		// Test \0
	utf8_alphabetic();
	utf8_numeric();
// 	utf8_case_conversions();	// None yet
//	utf16_encoding();	//

	printf("Completed %d tests with %d failures\n", test_count, failure_count);
	return failure_count == 0 ? 0 : 1;
}

void
test_group(const char* group)
{
	new_group = group;
}

void
expect(const char* when, uint32_t result, uint32_t wanted = 1)
{
	test_count++;
	if (result != wanted)
	{
		if (new_group)
			printf("%s:\n", new_group);
		if (wanted != 1)
			printf("%d:\t%s: FAIL (wanted %d=0x%X got %d=0x%X)\n", test_count, when, wanted, wanted, result, result);
		else
			printf("%d:\t%s: FAIL\n", test_count, when);
		failure_count++;
		new_group = 0;
	}
	else if (show_passes)
	{
		if (new_group)
			printf("%s:\n", new_group);
		printf("%d:\t%s: PASS\n", test_count, when);
		new_group = 0;
	}

}

void
utf8_char_sizes()
{
	test_group("encoded size for UCS4 chars");

	expect("max length 1", UTF8Len(max_1byte), 1);

	expect("min length 2", UTF8Len(max_1byte+1), 2);
	expect("max length 2", UTF8Len(max_2byte), 2);

	expect("min length 3", UTF8Len(max_2byte+1), 3);
	expect("max length 3", UTF8Len(max_3byte), 3);

	expect("min length 4", UTF8Len(max_3byte+1), 4);
	expect("max length 4", UTF8Len(max_4byte), 4);

	expect("min length 5", UTF8Len(max_4byte+1), 5);
	expect("max length 5", UTF8Len(max_5byte), 5);

	expect("min length 6", UTF8Len(max_5byte+1), 6);
	expect("max length 6", UTF8Len(max_6byte), 6);

	// Illegal UTF8 characters appear inside the range that would allow 6-byte encoding:
	UCS4	min_illegal = 0x80000000;
	UCS4	max_illegal = 0x800000FF;
	expect("min illegal-1", UTF8Len(min_illegal-1), 6);
	expect("min illegal", UTF8Len(min_illegal), 1);
	expect("max illegal", UTF8Len(max_illegal), 1);
	expect("max illegal+1", UTF8Len(max_illegal+1), 6);

	test_group("Size limits of UTF8 1stOf2 and 2ndOf2");

	expect("maximum 1st-of-two", UTF8Len('\x7F'), 1);
	expect("maximum 1st-of-two", UTF8Is1st('\xDF'), 1);
	expect("maximum 2nd-of-two", UTF8Is2nd('\xBF'), 1);	// \xBF = '\x80'+'\x3F'
}

void
utf8_get_backup()
{
	// test_group("encoded size for UCS4 chars");
	test_group("decoding UTF8 to UCS4, advancing and backing up");

	const UTF8*	cp;

	const UTF8*	min_one = "\0";
	cp = min_one;
	expect("min one char get", UTF8Get(cp), 0);
	expect("min one char advance", cp == min_one+1);
	expect("min one char backup", UTF8Backup(cp) == min_one);

	const UTF8*	one = "A";
	cp = one;
	expect("one char get", UTF8Get(cp), 'A');
	expect("one char advance", cp == one+1);
	expect("one char backup", UTF8Backup(cp) == one);

	const UTF8*	max_one = "\x7F";
	cp = max_one;
	expect("max one char get", UTF8Get(cp), 0x7F);
	expect("max one char advance", cp == max_one+1);
	expect("max one char backup", UTF8Backup(cp) == max_one);

	const UTF8*	min_two = "\xC0\x80";		// Minimum 1st-of-2, Minimum 2nd-of-2
	cp = min_two;
	expect("min two char get", UTF8Get(cp), 0);
	expect("min two char advance", cp == min_two+2);
	expect("min two char backup", UTF8Backup(cp) == min_two);

	const UTF8*	max_two = "\xDF\xBF";		// Maximum 1st-of-2, maximum 2nd-of-2
	cp = max_two;
	expect("max two char get", UTF8Get(cp), max_2byte);
	expect("max two char advance", cp == max_two+2);

	const UTF8*	min_three = "\xE0\x80\x80";
	cp = min_three;
	expect("min three char get", UTF8Get(cp), 0);
	expect("min three char advance", cp == min_three+3);
	expect("min three char backup", UTF8Backup(cp) == min_three);

	const UTF8*	max_three = "\xEF\xBF\xBF";
	cp = max_three;
	expect("max three char get", UTF8Get(cp), max_3byte);
	expect("max three char advance", cp == max_three+3);
	expect("max three char backup", UTF8Backup(cp) == max_three);

	const UTF8*	min_four = "\xF0\x80\x80\x80";
	cp = min_four;
	expect("min four char get", UTF8Get(cp), 0);
	expect("min four char advance", cp == min_four+4);
	expect("min four char backup", UTF8Backup(cp) == min_four);

	const UTF8*	max_four = "\xF7\xBF\xBF\xBF";
	cp = max_four;
	expect("max four char get", UTF8Get(cp), max_4byte);
	expect("max four char advance", cp == max_four+4);
	expect("max four char backup", UTF8Backup(cp) == max_four);

	const UTF8*	min_five = "\xF8\x80\x80\x80\x80";
	cp = min_five;
	expect("min five char get", UTF8Get(cp), 0);
	expect("min five char advance", cp == min_five+5);
	expect("min five char backup", UTF8Backup(cp) == min_five);

	const UTF8*	max_five = "\xFB\xBF\xBF\xBF\xBF";
	cp = max_five;
	expect("max five char get", UTF8Get(cp), max_5byte);
	expect("max five char advance", cp == max_five+5);
	expect("max five char backup", UTF8Backup(cp) == max_five);

	const UTF8*	min_six = "\xFC\x80\x80\x80\x80\x80";
	cp = min_six;
	expect("min six char get", UTF8Get(cp), 0);
	expect("min six char advance", cp == min_six+6);
	expect("min six char backup", UTF8Backup(cp) == min_six);

	const UTF8*	max_six = "\xFF\xBF\xBF\xBF\xBF\xBE";
	cp = max_six;
	expect("max six char get", UTF8Get(cp), max_6byte);
	expect("max six char advance", cp == max_six+6);
	expect("max six char backup", UTF8Backup(cp) == max_six);
}

void
utf8_invalid()
{
	test_group("Invalid UTF8 encoding");

	const UTF8*	cp;

	// We expect to advance and backup past the prefix of broken UTF8 codes:

	const UTF8*	illegal_second = "\xC0\x01";		// Minimum 1st-of-2, illegal 2nd-of-2
	cp = illegal_second;
	expect("stray 1st of 2", UTF8Get(cp), UTF8EncodeIllegal(0xC0));
	expect("stray 1st of 2 advance", cp == illegal_second+1);
	expect("stray 1st of 2 backup", UTF8Backup(cp) == illegal_second);

	const UTF8*	illegal_three = "\xE0\xBF\x01";		// Minimum 1st-of-3, ok 2nd, illegal 3rd-of-3
	cp = illegal_three;
	expect("stray 1st of 3", UTF8Get(cp), UTF8EncodeIllegal(0xE0));
	expect("stray 1st of 3 advance", cp == illegal_three+1);
	expect("stray 1st of 3 backup", UTF8Backup(cp) == illegal_three);

	const UTF8*	illegal_four = "\xF0\xBF\xBF\x01";	// Minimum 1st-of-4, ok 2nd and 3rd, illegal 4rd-of-4
	cp = illegal_four;
	expect("stray 1st of 4", UTF8Get(cp), UTF8EncodeIllegal(0xF0));
	expect("stray 1st of 4 advance", cp == illegal_four+1);
	expect("stray 1st of 4 backup", UTF8Backup(cp) == illegal_four);

	const UTF8*	illegal_five = "\xF8\xBF\xBF\xBF\x01";	// Minimum 1st-of-5, ok 2nd, 3rd&4th, illegal 5rd-of-5
	cp = illegal_five;
	expect("stray 1st of 5", UTF8Get(cp), UTF8EncodeIllegal(0xF8));
	expect("stray 1st of 5 advance", cp == illegal_five+1);
	expect("stray 1st of 5 backup", UTF8Backup(cp) == illegal_five);

	const UTF8*	illegal_six = "\xFC\xBF\xBF\xBF\xBF\x01"; // Minimum 1st-of-5, ok 2nd-5th, illegal 6rd-of-6
	cp = illegal_six;
	expect("stray 1st of 6", UTF8Get(cp), UTF8EncodeIllegal(0xFC));
	expect("stray 1st of 6 advance", cp == illegal_six+1);
	expect("stray 1st of 6 backup", UTF8Backup(cp) == illegal_six);

	// We expect to advance and backup past an stray trailing byte UTF8 code:
	// BEWARE: We inserted an initial character here that isn't a valid 1st byte, otherwise Backup might fail
	const UTF8*	min_illegal_trailing = "A\x80";		// Minimum trailing byte
	cp = min_illegal_trailing+1;
	expect("stray min trailing byte", UTF8Get(cp), UTF8EncodeIllegal(0x80));
	expect("stray min trailing byte advance", cp == min_illegal_trailing+2);
	expect("stray min trailing byte backup", UTF8Backup(cp) == min_illegal_trailing+1);

	const UTF8*	max_illegal_trailing = "A\xBF";		// Maximum trailing byte
	cp = max_illegal_trailing+1;
	expect("stray max trailing byte", UTF8Get(cp), UTF8EncodeIllegal(0xBF));
	expect("stray max trailing byte advance", cp == max_illegal_trailing+2);
	expect("stray max trailing byte backup", UTF8Backup(cp) == max_illegal_trailing+1);
}

void
utf8_eof()
{
	test_group("UTF8 NUL-terminator");

	const UTF8*	one = "1";
	const UTF8*	cp = one;

	// We expect to advance past a single char:
	expect("one char get", UTF8Get(cp) == '1');
	expect("one char advance", cp == one+1);

	// and to advance past a single \0
	expect("NUL char get", UTF8Get(cp) == 0);
	expect("NUL char advance", cp == one+2);
}

void
utf8_classify()
{
}

void
utf8_alphabetic()
{
	test_group("UCS4 alphabetics");

	// ASCII alphabetics:
	expect("!UCS4IsAlphabetic('a'-1)", !UCS4IsAlphabetic('a'-1));
	expect("UCS4IsAlphabetic('a')", UCS4IsAlphabetic('a'));
	expect("UCS4IsAlphabetic('z')", UCS4IsAlphabetic('z'));
	expect("!UCS4IsAlphabetic('z'+1)", !UCS4IsAlphabetic('z'+1));
	expect("!UCS4IsAlphabetic('A'-1)", !UCS4IsAlphabetic('A'-1));
	expect("UCS4IsAlphabetic('A')", UCS4IsAlphabetic('A'));
	expect("UCS4IsAlphabetic('Z')", UCS4IsAlphabetic('Z'));
	expect("!UCS4IsAlphabetic('Z'+1)", !UCS4IsAlphabetic('Z'+1));

	// Latin1 lower case, 0xC0..0xD6 and 0xD8..0xDE. 0xDF is also alphabetic but doesn't have a case-conversion
	UCS4	ch;
	ch = 0xC0;
	expect("Latin1 first lower-1 !UCS4IsAlphabetic('\xC0'-1)", !UCS4IsAlphabetic(ch-1));
	expect("Latin1 first lower UCS4IsAlphabetic('\xC0')", UCS4IsAlphabetic(ch));
	ch = 0xD6;
	expect("Latin1 last lower UCS4IsAlphabetic('\xD6')", UCS4IsAlphabetic(ch));
	expect("Latin1 last lower+1 !UCS4IsAlphabetic('\xD6'+1)", !UCS4IsAlphabetic(ch+1));
	ch = 0xDF;
	expect("Latin1 no-conv UCS4IsAlphabetic('\xDF')", UCS4IsAlphabetic(ch));

	// Latin1 upper case, 0xE0..0xF6 and 0xF8..0xFE
	ch = 0xE0;
	expect("Latin1 first upper-1 UCS4IsAlphabetic('\xDF')", UCS4IsAlphabetic(ch-1)); // 0xDF is also Alphabetic
	expect("Latin1 first upper UCS4IsAlphabetic('\xE0')", UCS4IsAlphabetic(ch));
	ch = 0xF6;
	expect("Latin1 last upper UCS4IsAlphabetic('\xF6')", UCS4IsAlphabetic(ch));
	expect("Latin1 last upper+1 !UCS4IsAlphabetic('\xF6'+1)", !UCS4IsAlphabetic(ch+1));

      	// 0x1F10..0x1F15
	ch = 0x1F10;
	// expect("Unicode range bottom !UCS4IsAlphabetic(0x1F10-1)", !UCS4IsAlphabetic(ch-1)); // 0x1F0F is also Alphabetic
	expect("Unicode range bottom IsAlphabetic(0x1F10)", UCS4IsAlphabetic(ch));
	ch = 0x1F15;
	expect("Unicode range top IsAlphabetic(0x1F15)", UCS4IsAlphabetic(ch));
	expect("Unicode range top !IsAlphabetic(0x1F15+1)", !UCS4IsAlphabetic(ch+1));

	ch = 0x2170;
	// expect("Unicode range bottom !UCS4IsAlphabetic(0x2170-1)", !UCS4IsAlphabetic(ch-1)); // 0x216F is also Alphabetic
	expect("Unicode range bottom IsAlphabetic(0x2170)", UCS4IsAlphabetic(ch));
	ch = 0x217F;
	expect("Unicode range top IsAlphabetic(0x217F)", UCS4IsAlphabetic(ch));
	expect("Unicode range top !IsAlphabetic(0x217F+1)", !UCS4IsAlphabetic(ch+1));

	ch = 0x24B6;
	expect("Unicode range bottom !UCS4IsAlphabetic(0x24B6-1)", !UCS4IsAlphabetic(ch-1));
	expect("Unicode range bottom IsAlphabetic(0x24B6)", UCS4IsAlphabetic(ch));
	ch = 0x24CF;
	expect("Unicode range top IsAlphabetic(0x24CF)", UCS4IsAlphabetic(ch));
	// expect("Unicode range top !IsAlphabetic(0x24CF+1)", !UCS4IsAlphabetic(ch+1)); // 0x24D0 is also Alphabetic

	// 0xFF41..0xFF5A
	ch = 0xFF41;
	expect("Unicode range bottom !UCS4IsAlphabetic(0xFF41-1)", !UCS4IsAlphabetic(ch-1));
	expect("Unicode range bottom IsAlphabetic(0xFF41)", UCS4IsAlphabetic(ch));
	ch = 0xFF5A;
	expect("Unicode range top IsAlphabetic(0xFF5A)", UCS4IsAlphabetic(ch));
	// expect("Unicode range top !IsAlphabetic(0xFF5A+1)", !UCS4IsAlphabetic(ch+1)); // 0xFF5B is also Alphabetic
}

void
utf8_numeric()
{
	test_group("UCS4 numerics");

	UCS4	ch;

	// ASCII digits. Outside 0..9, the expected value is -1, so we also test '0'-2:
	expect("ASCIIDigit('0'-2) == -1", ASCIIDigit('0'-2), -1);
	expect("ASCIIDigit('0'-1) == -1", ASCIIDigit('0'-1), -1);
	expect("ASCIIDigit('0') == 0", ASCIIDigit('0'), 0);
	expect("ASCIIDigit('9') == 9", ASCIIDigit('9'), 9);
	expect("ASCIIDigit('9'+1) == -1", ASCIIDigit('9'+1), -1);

	// Unicode digits, test some cases. Outside 0..9, the expected value is -1, so we also test '0'-2:
	// Arabic-Indic: { 0x0660, 0x0669 }, 
	ch = 0x0660;
	expect("UCS4Digit(0x065F) == -1", UCS4Digit(ch-1), -1);
	expect("UCS4Digit(0x0660) == 0", UCS4Digit(ch), 0);
	ch = 0x0669;
	expect("UCS4Digit(0x0669) == 9", UCS4Digit(ch), 9);
	expect("UCS4Digit(0x066A) == -1", UCS4Digit(ch+1), -1);

	// Gujarati: { 0x0AE7, 0x0AEF }, 
	ch = 0x0AE7;
	expect("UCS4Digit(0x0AE6) == -1", UCS4Digit(ch-1), -1);
	expect("UCS4Digit(0x0AE7) == 1", UCS4Digit(ch), 1);
	ch = 0x0AEF;
	expect("UCS4Digit(0x0AEF) == 9", UCS4Digit(ch), 9);
	expect("UCS4Digit(0x0AF0) == -1", UCS4Digit(ch+1), -1);

	// Fullwidth: { 0xFF10, 0xFF19 }
	ch = 0xFF10;
	expect("UCS4Digit(0xFF0F) == -1", UCS4Digit(ch-1), -1);
	expect("UCS4Digit(0xFF10) == 0", UCS4Digit(ch), 0);
	ch = 0xFF19;
	expect("UCS4Digit(0xFF19) == 9", UCS4Digit(ch), 9);
	expect("UCS4Digit(0xFF1A) == -1", UCS4Digit(ch+1), -1);
}

void
utf8_case_conversions()
{
	test_group("UCS4 case conversions");

/*
UCS4		UCS4ToUpper(UCS4 ch);		// To upper case
UCS4		UCS4ToLower(UCS4 ch);		// To lower case
UCS4		UCS4ToTitle(UCS4 ch);		// To Title or upper case
*/
}
