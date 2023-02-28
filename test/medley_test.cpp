/*
 * Unicode Strings
 * A medley of String tests
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	"strpp.h"

#include	<string.h>
#include	<stdio.h>
#include	"memory_monitor.h"

void tests();

int main(int argc, const char** argv)
{
	start_recording_allocations();
	tests();
	if (unfreed_allocation_count() > 0)	// No allocation should remain unfreed
		report_allocations();
	return 0;
}

void tests()
{
	StrVal	foobar("foo bar");

	int c = foobar.compare("foo bar");
	printf("foobar.compare() == %d\n", c);

	if (foobar == StrVal("foo bar"))
		printf("foobar == \"foo bar\"\n");

	printf("foobar =\n");
	for (int i = 0; i <= foobar.length(); i++)
		printf("\t%d: 0x%02X '%c'\n", i, foobar[i], foobar[i]);

	StrVal	foo = foobar.substr(0, 3);
	printf("foobar[0,3] =\n");
	for (int i = 0; i <= foo.length(); i++)
		printf("\t%d: 0x%02X '%c'\n", i, foo[i], foo[i]);

	StrVal	bar = foobar.substr(4, 3);
	printf("foobar[4,3] =\n");
	for (int i = 0; i <= bar.length(); i++)
		printf("\t%d: 0x%02X '%c'\n", i, bar[i], bar[i]);

	// Static string test. The literal string must outlive the body, which must outlive the StrVal
	StrBody	const_body("Borrow this data but don't fudge it\n", false, 0, 1);
	StrVal	cstr(&const_body);
	fputs(cstr.substr(3).asUTF8(), stdout);

	// printf("CompareCI = %d\n", StrVal::CompareCI);

	// Unicode substitution characters:
	StrVal	galley("�☐");	// "Replacement character" = U-FFFD, "Ballot Box" = U-2610
	printf("galley = U-%04X U-%04X '%s'\n", galley[0], galley[1], galley.asUTF8());

	StrVal	emoji("\xF0\x9F\x8E\x89\xF0\x9F\x8D\xBE");	// Party Popper U-1F389, Bottle with Popping Cork U-1F37E
	printf("emoji = U-%04X U-%04X '%s'\n", emoji[0], emoji[1], emoji.asUTF8());

	// In Mandarin: "It is possible that some Person speaks more than one Language."
	StrVal	multilingual("某一个人讲多过一种语言可 能的。");
	printf("multilingual.length: %d\n", multilingual.length());
	printf("multilingual = %s\n", multilingual.asUTF8());

	// Test the single-character constructor:
	StrVal	each((UCS4)0x4E2A);		// 个 is Mandarin quantifier, more-or-less "each"
	printf("each = %s, length: %d chars, utf8: %d bytes\n", each.asUTF8(), each.length(), each.numBytes());

	// Test UTF8 compaction.
	// "个" \u4E2A encoded optimally as 3 bytes: \xE4\xB8\xAA, or 1110-0100 10-111000 10-101010
	// same but encoded non-optimally as 4 bytes: \xF0\x84\xB8\xAA or 1111-0000 10-000100 10-111000 10-101010
	// "The shape of each square is rectangular."
	// StrVal	square_is_rectangle("每一个四方体的形状是 长方形。");
	const char*	raw = "每一\xF0\x84\xB8\xAA四方体的形状是 长方形。";
	StrVal  square_is_rectangle(raw);
	printf("square_is_rectangle.length: %d chars, raw=%d bytes, compacted=%d bytes\n",
		square_is_rectangle.length(), (int)strlen(raw), square_is_rectangle.numBytes());
	printf("square_is_rectangle = %s\n", square_is_rectangle.asUTF8());
	for (int i = 0; i <= square_is_rectangle.length(); i++)
		printf("\t%d: U-%04X (%d bytes)\n", i, square_is_rectangle[i], UTF8Len(square_is_rectangle[i]));
}
