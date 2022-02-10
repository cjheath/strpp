/*
 * Unicode Strings
 * Unit test driver for the Regular Expression compiler
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>

#define	protected	public
#include	<strregex.h>

#include	"memory_monitor.h"

bool	verbose = false;
bool	show_all_expectations = false;
int	automated_tests();
void	show_expectation(const char* regex, const char* nfa, const char* error_message);

int
main(int argc, char** argv)
{
	while (argc > 1 && *argv[1] == '-')
	{
		if (0 == strcmp("-v", argv[1]))
			verbose = true;
		else if (0 == strcmp("-e", argv[1]))
			show_all_expectations = true;
		argc--; argv++;
	}

	if (argc == 1)
		return automated_tests();

	for (--argc, ++argv; argc > 0; argc--, argv++)
	{
		RxCompiler	rx(*argv, (RxFeature)((int32_t)RxFeature::AllFeatures | (int32_t)RxFeature::ExtendedRE));
		char*		nfa;
		bool		scanned_ok;

		printf("Compiling '%s'\n", *argv);

		start_recording_allocations();
		scanned_ok = rx.compile(nfa);
		if (scanned_ok && unfreed_allocation_count() > 1)
			report_allocations();

		if (!scanned_ok)
		{
			printf("Regex scan failed: %s\n", rx.errorMessage());
			continue;
		}
		if (show_all_expectations)
			show_expectation(*argv, nfa, rx.errorMessage());

		if (nfa)
		{
			rx.dump(nfa);
			delete[] nfa;
		}

		// RxMatcher	rm(rx);
	}
	return 0;
}

struct	compiler_test {
	const char*	regex;			// the regex to compile
	const char*	expected_nfa;		// The NFA string which is expected from the compiler
	const char*	expected_message;	// If non-zero, the test should fail with this message
};
compiler_test	compiler_tests[] =
{
	{ "a",					"S\x16\x0A\x04\x01\x01\x01(\x01""Ca#A\x0D.J\x09", 0 },
	{ "abc",				"S\x1E\x0A\x08\x01\x01\x01(\x01""CaCbCc#A\x15.J\x09", 0 },
	{ "a b c",				"S\x1E\x0A\x08\x01\x01\x01(\x01""CaCbCc#A\x15.J\x09", 0 },

 { 0,	"--- Escapes; also check simple repetitions of them ---", 0 },
	{ "\\a",				"S\x16\x0A\x04\x01\x01\x01(\x01""Ca#A\x0D.J\x09", 0 },
	{ "\\b",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x08#A\x0D.J\x09", 0 },
	{ "\\e",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x1B#A\x0D.J\x09", 0 },
	{ "\\f",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x0C#A\x0D.J\x09", 0 },
	{ "\\n",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x0A#A\x0D.J\x09", 0 },
	{ "\\t",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x09#A\x0D.J\x09", 0 },
	{ "\\r",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x0D#A\x0D.J\x09", 0 },
	{ "ab\\f?",				"S\"\x0A\x08\x01\x01\x01(\x01""CaCbA\x06""C\x0C#A\x19.J\x09", 0 },
	{ "ab\\f*",				"S&\x0A\x08\x01\x01\x01(\x01""CaCbA\x0A""C\x0CJ\x0B#A\x1D.J\x09", 0 },
	{ "ab\\f+",				"S\"\x0A\x08\x01\x01\x01(\x01""CaCbC\x0C""A\x07#A\x19.J\x09", 0 },

 { 0,	"--- Octal character escapes ---", 0 },
	{ "\\0",				"S\x16\x0A\x04\x01\x01\x01(\x01""C", 0 },
	{ "\\7",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x07#A\x0D.J\x09", 0 },
	{ "\\07",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x07#A\x0D.J\x09", 0 },
	{ "\\08",				"S\x1A\x0A\x06\x01\x01\x01(\x01""C", 0 },
	{ "\\077",				"S\x16\x0A\x04\x01\x01\x01(\x01""C?#A\x0D.J\x09", 0 },
	{ "\\0777",				"S\x1A\x0A\x06\x01\x01\x01(\x01""C?C7#A\x11.J\x09", 0 },
	{ "\\077*",				"S\x1E\x0A\x04\x01\x01\x01(\x01""A\x0A""C?J\x0B#A\x15.J\x09", 0 },

 { 0,	"--- Hex character escapes ---", 0 },
	{ "\\x7",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x07#A\x0D.J\x09", 0 },
	{ "\\x7G",				"S\x1A\x0A\x06\x01\x01\x01(\x01""C\x07""CG#A\x11.J\x09", 0 },
	{ "\\x77",				"S\x16\x0A\x04\x01\x01\x01(\x01""Cw#A\x0D.J\x09", 0 },
	{ "\\x7E7",				"S\x1A\x0A\x06\x01\x01\x01(\x01""C~C7#A\x11.J\x09", 0 },
	{ "\\x77*",				"S\x1E\x0A\x04\x01\x01\x01(\x01""A\x0A""CwJ\x0B#A\x15.J\x09", 0 },
	{ "\\u77*",				"S\x1E\x0A\x04\x01\x01\x01(\x01""A\x0A""CwJ\x0B#A\x15.J\x09", 0 },

 { 0,	"--- Unicode escapes ---", 0 },
	{ "\\u7",				"S\x16\x0A\x04\x01\x01\x01(\x01""C\x07#A\x0D.J\x09", 0 },
	{ "\\u77",				"S\x16\x0A\x04\x01\x01\x01(\x01""Cw#A\x0D.J\x09", 0 },
	{ "\\u777",				"S\x18\x0A\x06\x01\x01\x01(\x01""C\xDD\xB7#A\x0F.J\x09", 0 },
	{ "\\u7777",				"S\x1A\x0A\x08\x01\x01\x01(\x01""C\xE7\x9D\xB7#A\x11.J\x09", 0 },
	{ "\\uFFFFF",				"S\x1C\x0A\x0A\x01\x01\x01(\x01""C\xF3\xBF\xBF\xBF#A\x13.J\x09", 0 },
	{ "\\uFFFFF1",				"S\x20\x0A\x0C\x01\x01\x01(\x01""C\xF3\xBF\xBF\xBF""C1#A\x17.J\x09", 0 },
	{ "\\u77*",				"S\x1E\x0A\x04\x01\x01\x01(\x01""A\x0A""CwJ\x0B#A\x15.J\x09", 0 },

 { 0,	"--- Character properties: ---", 0 },
	{ "\\sA",				"S\x1C\x0A\x06\x01\x01\x01(\x01P\x01sCA#A\x13.J\x09", 0 },
	{ "a\\s*",				"S$\x0A\x06\x01\x01\x01(\x01""CaA\x0CP\x01sJ\x0D#A\x1B.J\x09", 0 },
	{ "\\dA",				"S\x1C\x0A\x06\x01\x01\x01(\x01P\x01""dCA#A\x13.J\x09", 0 },
	{ "a\\d*",				"S$\x0A\x06\x01\x01\x01(\x01""CaA\x0CP\x01""dJ\x0D#A\x1B.J\x09", 0 },
	{ "\\hA",				"S\x1C\x0A\x06\x01\x01\x01(\x01P\x01hCA#A\x13.J\x09", 0 },
	{ "a\\h*",				"S$\x0A\x06\x01\x01\x01(\x01""CaA\x0CP\x01hJ\x0D#A\x1B.J\x09", 0 },
	{ "\\p{foo}A",				"S\x20\x0A\x06\x01\x01\x01(\x01P\x03""fooCA#A\x17.J\x09", 0 },
	{ "a\\p{foo}*",				"S(\x0A\x06\x01\x01\x01(\x01""CaA\x10P\x03""fooJ\x11#A\x1F.J\x09", 0 },

 { 0,	"--- Anchors ---", 0 },
	{ "^a",					"S\x18\x0A\x06\x01\x01\x01(\x01^Ca#A\x0F.J\x09", 0 },
	{ "a^",					"S\x18\x0A\x06\x01\x01\x01(\x01""Ca^#A\x0F.J\x09", 0 },
	{ "$",					"S\x14\x0A\x04\x01\x01\x01(\x01$#A\x0B.J\x09", 0 },
	{ "$a",					"S\x18\x0A\x06\x01\x01\x01(\x01$Ca#A\x0F.J\x09", 0 },

 { 0,	"--- Any ---", 0 },
	{ ".",					"S\x14\x0A\x04\x01\x01\x01(\x01.#A\x0B.J\x09", 0 },
	{ ".*",					"S\x1C\x0A\x04\x01\x01\x01(\x01""A\x08.J\x09#A\x13.J\x09", 0 },
	{ ".+",					"S\x18\x0A\x04\x01\x01\x01(\x01.A\x05#A\x0F.J\x09", 0 },
	{ ".{2,5}",				"S\x1E\x0A\x08\x01\x01\x01(\x01Z.R\x03\x06\x09#A\x15.J\x09", 0 },
	{ "a.b",				"S\x1C\x0A\x08\x01\x01\x01(\x01""Ca.Cb#A\x13.J\x09", 0 },

 { 0,	"--- Non-capture groups ---", 0 },
	{ "(a)",				"S\x16\x0A\x04\x02\x01\x01(\x01""Ca#A\x0D.J\x09", 0 },
	{ "(ab)",				"S\x1A\x0A\x06\x02\x01\x01(\x01""CaCb#A\x11.J\x09", 0 },
	{ "(a)b",				"S\x1A\x0A\x06\x02\x01\x01(\x01""CaCb#A\x11.J\x09", 0 },
	{ "(a)(b)",				"S\x1A\x0A\x06\x02\x01\x01(\x01""CaCb#A\x11.J\x09", 0 },

 { 0,	"--- Alternates ---", 0 },
	{ "a|b",				"S\"\x0A\x08\x01\x01\x01(\x01""A\x0A""CaJ\x06""Cb#A\x19.J\x09", 0 },
	{ "a|b|c",				"S.\x0A\x0A\x01\x01\x01(\x01""A\x0A""CaJ\x12""A\x0A""CbJ\x06""Cc#A%.J\x09", 0 },
	{ "a|b|cde|f",				"SB\x0A\x10\x01\x01\x01(\x01""A\x0A""CaJ&A\x0A""CbJ\x1A""A\x12""CcCdCeJ\x06""Cf#A9.J\x09", 0 },
	{ "(a|b)(c)",				"S&\x0A\x0A\x02\x01\x01(\x01""A\x0A""CaJ\x06""CbCc#A\x1D.J\x09", 0 },
	{ "(a|b|c)(d)",				"S2\x0A\x0C\x02\x01\x01(\x01""A\x0A""CaJ\x12""A\x0A""CbJ\x06""CcCd#A).J\x09", 0 },
	{ "(a|b)|(c)",				"S.\x0A\x0C\x02\x01\x01(\x01""A\x16""A\x0A""CaJ\x06""CbJ\x06""Cc#A%.J\x09", 0 },

 { 0,	"--- Long offsets ---", 0 },
	{ "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|b",	"S\xC8\xA4\x0C\xC4\x86\x01\x01\x01(\x01""A\xC8\x88""CACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACAJ\x06""Cb#A\xC8\x97.J\x0B", 0 },
	{ "(a|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA)",	"S\xC8\xA4\x0C\xC4\x86\x02\x01\x01(\x01""A\x0C""CaJ\xC8\x84""CACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACA#A\xC8\x97.J\x0B", 0 },
	{ "(ab|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|cd)",	"S\xC8\xBA\x0C\xC4\x8C\x02\x01\x01(\x01""A\x10""CaCbJ\xC8\x96""A\xC8\x88""CACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACAJ\x0A""CcCd#A\xC8\xAD.J\x0B", 0 },
	{ "(ab|cd|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|ef|gh)",	"S\xC9\x9C\x0C\xC4\x94\x02\x01\x01(\x01""A\x10""CaCbJ\xC8\xB8""A\x10""CcCdJ\xC8\xA6""A\xC8\x88""CACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACACAJ\x1A""A\x0E""CeCfJ\x0A""CgCh#A\xC9\x8F.J\x0B", 0 },

 { 0,	"--- Character classes ---", 0 },
	{ "[a-c]",				"S\x1A\x0A\x04\x01\x01\x01(\x01L\x02""ac#A\x11.J\x09", 0 },
	{ "[a\\-c]",				"S\"\x0A\x04\x01\x01\x01(\x01L\x06""aa--cc#A\x19.J\x09", 0 },
	{ "[a-c\\]]",				"S\x1E\x0A\x04\x01\x01\x01(\x01L\x04""ac]]#A\x15.J\x09", 0 },
	{ "[a-c\\n]",				"S\x1E\x0A\x04\x01\x01\x01(\x01L\x04""acnn#A\x15.J\x09", 0 },
	{ "[a-cdef-g]",				"S&\x0A\x04\x01\x01\x01(\x01L\x08""acddeefg#A\x1D.J\x09", 0 },
	{ "[-]",				"S\x1A\x0A\x04\x01\x01\x01(\x01L\x02--#A\x11.J\x09", 0 },
	{ "[-a]",				"S\x1E\x0A\x04\x01\x01\x01(\x01L\x04--aa#A\x15.J\x09", 0 },
	{ "[a-]",				"S\x1E\x0A\x04\x01\x01\x01(\x01L\x04""aa--#A\x15.J\x09", 0 },
	{ "[^-ab-c]",				"S\"\x0A\x04\x01\x01\x01(\x01N\x06--aabc#A\x19.J\x09", 0 },
	{ "[^-]",				"S\x1A\x0A\x04\x01\x01\x01(\x01N\x02--#A\x11.J\x09", 0 },
	{ "[a-c",	0, "Bad character class" },
	{ "[\\",	0, "Bad character class" },

 { 0,	"--- REVISIT: Add support for Posix groups or Unicode Properties ---", 0 },
 { 0,	"--- Repetition: ---", 0 },
	{ "a?",					"S\x1A\x0A\x04\x01\x01\x01(\x01""A\x06""Ca#A\x11.J\x09", 0 },
	{ "a*",					"S\x1E\x0A\x04\x01\x01\x01(\x01""A\x0A""CaJ\x0B#A\x15.J\x09", 0 },
	{ "a+",					"S\x1A\x0A\x04\x01\x01\x01(\x01""CaA\x07#A\x11.J\x09", 0 },
	{ "abc?",				"S\"\x0A\x08\x01\x01\x01(\x01""CaCbA\x06""Cc#A\x19.J\x09", 0 },
	{ "abc*",				"S&\x0A\x08\x01\x01\x01(\x01""CaCbA\x0A""CcJ\x0B#A\x1D.J\x09", 0 },
	{ "abc+",				"S\"\x0A\x08\x01\x01\x01(\x01""CaCbCcA\x07#A\x19.J\x09", 0 },
	{ "(abc)?",				"S\"\x0A\x08\x02\x01\x01(\x01""A\x0E""CaCbCc#A\x19.J\x09", 0 },
	{ "(abc)*",				"S&\x0A\x08\x02\x01\x01(\x01""A\x12""CaCbCcJ\x13#A\x1D.J\x09", 0 },
	{ "(abc)+",				"S\"\x0A\x08\x02\x01\x01(\x01""CaCbCcA\x0F#A\x19.J\x09", 0 },
	{ "(a|b?|c*|d+)*",			"SR\x0A\x0C\x02\x01\x01(\x01""A>A\x0A""CaJ.A\x0E""A\x06""CbJ\x1E""A\x12""A\x0A""CcJ\x0BJ\x0A""CdA\x07J?#AI.J\x09", 0 },

	{ "((((a*)*)*)*)*",			"S>\x0A\x04\x05\x01\x01(\x01""A*A\"A\x1A""A\x12""A\x0A""CaJ\x0BJ\x13J\x1BJ#J+#A5.J\x09", 0},
	{ "(((((a*)*)*)*)*)*",			"SF\x0A\x04\x06\x01\x01(\x01""A2A*A\"A\x1A""A\x12""A\x0A""CaJ\x0BJ\x13J\x1BJ#J+J3#A=.J\x09", 0 },
 
	{ "?",	0, "Repeating a repetition is disallowed" },
	{ "*",	0, "Repeating a repetition is disallowed" },
	{ "+",	0, "Repeating a repetition is disallowed" },
	{ "abc*+def",	0, "Repeating a repetition is disallowed" },
	{ "(abc)??",	0, "Repeating a repetition is disallowed" },
	{ "(abc)?+",	0, "Repeating a repetition is disallowed" },
	{ "(abc)?*",	0, "Repeating a repetition is disallowed" },
	{ "(a|b)*?",	0, "Repeating a repetition is disallowed" },
	{ "(abc)**",	0, "Repeating a repetition is disallowed" },
	{ "(a|b)*+",	0, "Repeating a repetition is disallowed" },
	{ "(a|b)+?",	0, "Repeating a repetition is disallowed" },
	{ "(a|b)+*",	0, "Repeating a repetition is disallowed" },
	{ "(a|b)++",	0, "Repeating a repetition is disallowed" },

 { 0,	"--- Repetition ranges ---", 0 },
	{ "a{2,3}",				"S\x20\x0A\x08\x01\x01\x01(\x01ZCaR\x03\x04\x0B#A\x17.J\x09", 0 },
	{ "a{ 2 , 3 }",				"S\x20\x0A\x08\x01\x01\x01(\x01ZCaR\x03\x04\x0B#A\x17.J\x09", 0 },
	{ "a{2,}",				"S\x20\x0A\x08\x01\x01\x01(\x01ZCaR\x03\x01\x0B#A\x17.J\x09", 0 },
	{ "a{2,	}",				"S\x20\x0A\x08\x01\x01\x01(\x01ZCaR\x03\x01\x0B#A\x17.J\x09", 0 },
	{ "a{,3}",				"S\x20\x0A\x04\x01\x01\x01(\x01ZCaR\x01\x04\x0B#A\x17.J\x09", 0 },
	{ "a{ ,3}",				"S\x20\x0A\x04\x01\x01\x01(\x01ZCaR\x01\x04\x0B#A\x17.J\x09", 0 },
	{ "a{254,254}",				"S\"\x0C\xC8\x80\x01\x01\x01(\x01ZCaR\xFF\xFF\x0B#A\x17.J\x09", 0 },

	{ "a{255,255}",	0, "Min and Max repetition are limited to 254" },
	{ "a{254,255}",	0, "Min and Max repetition are limited to 254" },
	{ "a{-1,}",	0, "Bad repetition count" },

 { 0,	"--- Named groups ---", 0 },
	{ "(?<a>b)",				"S\"\x0E\x04\x02\x02\x02\x01""a(\x01(\x02""Cb)\x02#A\x15.J\x09", 0 },
	{ "(?<foo>a|b?|c*|d+){2,5}",		"Sd\x12\x10\x02\x02\x02\x03""foo(\x01Z(\x02""A\x0A""CaJ.A\x0E""A\x06""CbJ\x1E""A\x12""A\x0A""CcJ\x0BJ\x0A""CdA\x07)\x02R\x03\x06G#AS.J\x09", 0 },

 { 0,	"--- Function calls ---", 0 },
	{ "(?&a)(?<a>b)",			"S&\x0E\x06\x02\x02\x02\x01""a(\x01U\x01(\x02""Cb)\x02#A\x19.J\x09", 0 },
	{ "(?<a>b)(?&a)",			"S&\x0E\x06\x02\x02\x02\x01""a(\x01(\x02""Cb)\x02U\x01#A\x19.J\x09", 0 },
	{ "(?<a>b(?<c>d)e(?&a)?)(?&c)",		"SB\x12\x0C\x03\x03\x03\x01""a\x01""c(\x01(\x02""Cb(\x03""Cd)\x03""CeA\x06U\x01)\x02U\x02#A1.J\x09", 0 },

	{ "(?&a)",	0, "Function call to undeclared group" },
	{ "(?<a>b(?<c>d)e(?&a))(?&b)",	0, "Function call to undeclared group" },
};



void	show_expectation(const char* regex, const char* nfa, const char* error_message)
{		// Print the expected output line:
	StrVal	double_backslash("\\\\");
	StrVal	re(regex);
	re.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4	ch = UTF8Get(cp);	// Get UCS4 character
			StrVal	charval(ch);
			if (ch == '\\')
				return double_backslash;
			return charval;
		}
	);
	printf("\t{ \"%s\",\t", re.asUTF8());
	for (int i = (32-(int)re.length()+2)/8; i > 0; i--)
		printf("\t");	// Make most of the NFAs line up
	if (nfa)
	{
		printf("\"");
		bool	last_was_hex = false;
		for (const char* cp = nfa; *cp; cp++)
		{
			if (last_was_hex && ((*cp >= 'A' && *cp <= 'F') || (*cp >= 'a' && *cp <= 'f')))
				printf("\"\"");	// Terminate the previous hex and re-open the strin
			last_was_hex = false;
			if (*cp == '"')
				printf("\\\"");	// Escape a string-closing quote
			else if (isgraph(*cp))
				printf("%c", *cp&0xFF);
			else
			{
				printf("\\x%02X", *cp&0xFF);
				last_was_hex = true;
			}
		}
		printf("\"");
	}
	else
		printf("0");
	if (error_message)
		printf(", \"%s\" },\n", error_message);
	else
		printf(", 0 },\n");
}

int automated_tests()
{
	auto 	wrapper = [&](compiler_test* ct, int leaky_allocations = 0) -> bool
	{
		if (!ct->regex)
		{
			if (show_all_expectations)
				printf(" { 0,\t\"%s\", 0 },\n", ct->expected_nfa);
			return true;
		}

		RxCompiler	rx(ct->regex, (RxFeature)((int32_t)RxFeature::AllFeatures | (int32_t)RxFeature::ExtendedRE));
		char*		nfa = 0;
		bool		scanned_ok;

		start_recording_allocations();
		scanned_ok = rx.compile(nfa);

		// Check expectations
		bool	return_code_pass = (ct->expected_message == 0) == scanned_ok;	// Failure was reported if expected
		bool	nfa_presence_pass = (ct->expected_nfa != 0) == (nfa != 0);	// NFA was returned if one was expected
		bool	nfa_pass = !ct->expected_nfa || 0 == strcmp(nfa, ct->expected_nfa);	// NFA was correct if expected
		bool	error_message_presence_pass = (ct->expected_message != 0) == (rx.errorMessage() != 0);	// Message was returned if expected
		bool	error_message_pass = !ct->expected_message || 0 == strcmp(ct->expected_message, rx.errorMessage());	// Message was correct if expected
		bool	test_pass = return_code_pass && nfa_presence_pass && nfa_pass && error_message_presence_pass && error_message_pass;

		// Report Pass/Fixed/Pending/Fail:
		if (test_pass && !ct->expected_message)
			printf("Pass: %s\n", ct->regex);
		else if (test_pass)
			printf("Pass (with expected error): %s\n", ct->regex);
		else if (ct->expected_message && scanned_ok && nfa_pass && rx.errorMessage() == 0)
			printf("Fixed: %s\n", ct->regex);
		else if (ct->expected_message)
			printf("Pending: %s\n", ct->regex);
		else
			printf(
				"Fail (%s)%s, (returned %s): %s\n",
				rx.errorMessage() ? rx.errorMessage() : "<no message returned>",
				error_message_presence_pass ? "" : " unexpected",
				scanned_ok ? "ok" : "fail",
				ct->regex
			);

		if (!test_pass || show_all_expectations)
			show_expectation(ct->regex, nfa, rx.errorMessage());

		// On incorrect NFA, dump what we got:
		if (nfa && (!nfa_pass || verbose))
			rx.dump(nfa);

		// Clean up and check for leaks:
		if (nfa)
			delete[] nfa;
		if (scanned_ok && unfreed_allocation_count() > leaky_allocations)
		{
			printf("Unfreed allocations after compiling \"%s\":\n", ct->regex);
			report_allocations();
		}

		return test_pass;
	};
	size_t		num_tests = sizeof(compiler_tests)/sizeof(compiler_tests[0]);
	int		failures = 0;
	for (compiler_test* ct = compiler_tests; ct < compiler_tests+num_tests; ct++)
		if (!wrapper(ct))
			failures++;
	printf("\n%d tests run with %d failures\n", (int)num_tests, failures);
	return failures > 0 ? 1 : 0;
}

