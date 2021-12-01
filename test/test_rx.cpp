/*
 * Unit test driver for Regular Expressions
 */
#include	<stdio.h>

#define	protected	public
#include	<strregex.h>

#include	"memory_monitor.h"

int	automated_tests();

int
main(int argc, char** argv)
{
	if (argc == 1)
		return automated_tests();

	for (--argc, ++argv; argc > 0; argc--, argv++)
	{
		RxCompiled	rx(*argv, (RxFeature)(RxFeature::AllFeatures | RxFeature::ExtendedRE));
		char*		nfa;
		bool		scanned_ok;

		printf("Compiling '%s'\n", *argv);

		start_recording_allocations();
		scanned_ok = rx.compile(nfa);
		if (scanned_ok && unfreed_allocation_count() > 1)
			report_allocations();

		if (!scanned_ok)
		{
			printf("Regex scan failed: %s\n", rx.ErrorMessage());
			continue;
		}

		rx.dump(nfa);
		if (nfa)
			delete[] nfa;

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
	{ "a",		"\x01\x06\x01\x02\x01\x61\x10", 0 },
	{ "abc",	"\x01\x08\x01\x02\x03\x61\x62\x63\x10", 0 },
	{ "a b c",	"\x01\x08\x01\x02\x03\x61\x62\x63\x10", 0 },

	// Escapes; also check simple repetitions of them
	{ "\\a",	"\x01\x06\x01\x02\x01\x61\x10", 0 },
	{ "\\b",	"\x01\x06\x01\x02\x01\x08\x10", 0 },
	{ "\\e",	"\x01\x06\x01\x02\x01\x1B\x10", 0 },
	{ "\\f",	"\x01\x06\x01\x02\x01\x0C\x10", 0 },
	{ "\\n",	"\x01\x06\x01\x02\x01\x0A\x10", 0 },
	{ "\\t",	"\x01\x06\x01\x02\x01\x09\x10", 0 },
	{ "\\r",	"\x01\x06\x01\x02\x01\x0D\x10", 0 },
	{ "ab\\f?",	"\x01\x0D\x01\x02\x02\x61\x62\x09\x01\x02\x02\x01\x0C\x10", 0 },
	{ "ab\\f*",	"\x01\x0D\x01\x02\x02\x61\x62\x09\x01\x01\x02\x01\x0C\x10", 0 },
	{ "ab\\f+",	"\x01\x0D\x01\x02\x02\x61\x62\x09\x02\x01\x02\x01\x0C\x10", 0 },

	// Octal character escapes
	{ "\\0",	"\x01\x06\x01\x02\x01", 0 },
	{ "\\7",	"\x01\x06\x01\x02\x01\x07\x10", 0 },
	{ "\\07",	"\x01\x06\x01\x02\x01\x07\x10", 0 },
	{ "\\08",	"\x01\x07\x01\x02\x02", 0 },
	{ "\\077",	"\x01\x06\x01\x02\x01\x3F\x10", 0 },
	{ "\\0777",	"\x01\x07\x01\x02\x02\x3F\x37\x10", 0 },
	{ "\\077*",	"\x01\x09\x01\x09\x01\x01\x02\x01\x3F\x10", 0 },

	// Hex character escapes
	{ "\\x7",	"\x01\x06\x01\x02\x01\x07\x10", 0 },
	{ "\\x7G",	"\x01\x07\x01\x02\x02\x07\x47\x10", 0 },
	{ "\\x77",	"\x01\x06\x01\x02\x01\x77\x10", 0 },
	{ "\\x7E7",	"\x01\x07\x01\x02\x02\x7E\x37\x10", 0 },
	{ "\\x77*",	"\x01\x09\x01\x09\x01\x01\x02\x01\x77\x10", 0 },

	// Unicode escapes
	{ "\\u7",	"\x01\x06\x01\x02\x01\x07\x10", 0 },
	{ "\\u77",	"\x01\x06\x01\x02\x01\x77\x10", 0 },
	{ "\\u777",	"\x01\x07\x01\x02\x02\xDD\xB7\x10", 0 },
	{ "\\u7777",	"\x01\x08\x01\x02\x03\xE7\x9D\xB7\x10", 0 },
	{ "\\uFFFFF",	"\x01\x09\x01\x02\x04\xF3\xBF\xBF\xBF\x10", 0 },
	{ "\\uFFFFF1",	"\x01\x0A\x01\x02\x05\xF3\xBF\xBF\xBF\x31\x10", 0 },
	{ "\\u77*",	"\x01\x09\x01\x09\x01\x01\x02\x01\x77\x10", 0 },

	// Character properties:
	{ "\\sA",	"\x01\x09\x01\x03\x01\x20\x02\x01\x41\x10", 0 },
	{ "a\\s*",	"\x01\x0C\x01\x02\x01\x61\x09\x01\x01\x03\x01\x20\x10", 0 },
	{ "\\p{foo}A",	"\x01\x0B\x01\x03\x03\x66\x6F\x6F\x02\x01\x41\x10", 0 },
	{ "a\\p{foo}*",	"\x01\x0E\x01\x02\x01\x61\x09\x01\x01\x03\x03\x66\x6F\x6F\x10", 0 },

	// Anchors
	{ "^a",		"\x01\x07\x01\x04\x02\x01\x61\x10", 0 },
	{ "a^",		"\x01\x07\x01\x02\x01\x61\x04\x10", 0 },
	{ "$",		"\x01\x04\x01\x05\x10", 0 },
	{ "$a",		"\x01\x07\x01\x05\x02\x01\x61\x10", 0 },

	// Any
	{ ".",		"\x01\x04\x01\x08\x10", 0 },
	{ "a.b",	"\x01\x0A\x01\x02\x01\x61\x08\x02\x01\x62\x10", 0 },

	// Non-capture groups
	{ "(a)",	"\x01\x09\x01\x0A\x05\x02\x01\x61\x0F\x10", 0 },
	{ "(ab)",	"\x01\x0A\x01\x0A\x06\x02\x02\x61\x62\x0F\x10", 0 },
	{ "(a)b",	"\x01\x0C\x01\x0A\x05\x02\x01\x61\x0F\x02\x01\x62\x10", 0 },
	{ "(a)(b)",	"\x01\x0F\x01\x0A\x05\x02\x01\x61\x0F\x0A\x05\x02\x01\x62\x0F\x10", 0 },

	// Alternates
	{ "a|b",	"\x01\x06\x01\x02\x01\x61\x0D\x05\x02\x01\x62\x10", 0 },
	{ "a|b|c",	"\x01\x06\x01\x02\x01\x61\x0D\x05\x02\x01\x62\x0D\x05\x02\x01\x63\x10", 0 },
	{ "a|b|cde|f",	"\x01\x06\x01\x02\x01\x61\x0D\x05\x02\x01\x62\x0D\x07\x02\x03\x63\x64\x65\x0D\x05\x02\x01\x66\x10", 0 },
	{ "(a|b)(c)",	"\x01\x14\x01\x0A\x05\x02\x01\x61\x0D\x05\x02\x01\x62\x0F\x0A\x05\x02\x01\x63\x0F\x10", 0 },
	{ "(a|b|c)(d)",	"\x01\x19\x01\x0A\x05\x02\x01\x61\x0D\x05\x02\x01\x62\x0D\x05\x02\x01\x63\x0F\x0A\x05\x02\x01\x64\x0F\x10", 0 },
	{ "(a|b)|(c)",	"\x01\x0E\x01\x0A\x05\x02\x01\x61\x0D\x05\x02\x01\x62\x0F\x0D\x08\x0A\x05\x02\x01\x63\x0F\x10", 0 },

	// Long offsets
	{ "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|b",	"\x01\xC2\x87\x01\x02\xC2\x80\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x0D\x05\x02\x01\x62\x10", 0 },
	{ "(AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA)",	"\x01\xC2\x8B\x01\x0A\xC2\x86\x02\xC2\x80\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x0F\x10", 0 },
	{ "(a|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA)",	"\x01\xC2\x90\x01\x0A\x05\x02\x01\x61\x0D\xC2\x86\x02\xC2\x80\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x0F\x10", 0 },
	{ "(ab|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|cd)",	"\x01\xC2\x97\x01\x0A\x06\x02\x02\x61\x62\x0D\xC2\x86\x02\xC2\x80\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x0D\x06\x02\x02\x63\x64\x0F\x10", 0 },
	{ "(ab|cd|AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA|ef|gh)",	"\x01\xC2\xA3\x01\x0A\x06\x02\x02\x61\x62\x0D\x06\x02\x02\x63\x64\x0D\xC2\x86\x02\xC2\x80\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x41\x0D\x06\x02\x02\x65\x66\x0D\x06\x02\x02\x67\x68\x0F\x10", 0 },

	// Character classes
	{ "[a-c]",	"\x01\x07\x01\x06\x02\x61\x63\x10", 0 },
	{ "[a\\-c]",	"\x01\x0B\x01\x06\x06\x61\x61\x2D\x2D\x63\x63\x10", 0 },
	{ "[a-c\\]]",	"\x01\x09\x01\x06\x04\x61\x63\x5D\x5D\x10", 0 },
	{ "[a-c\\n]",	"\x01\x09\x01\x06\x04\x61\x63\x6E\x6E\x10", 0 },
	{ "[a-cdef-g]",	"\x01\x0D\x01\x06\x08\x61\x63\x64\x64\x65\x65\x66\x67\x10", 0 },
	{ "[-]",	"\x01\x07\x01\x06\x02\x2D\x2D\x10", 0 },
	{ "[-a]",	"\x01\x09\x01\x06\x04\x2D\x2D\x61\x61\x10", 0 },
	{ "[a-]",	"\x01\x09\x01\x06\x04\x61\x61\x2D\x2D\x10", 0 },
	{ "[^-ab-c]",	"\x01\x0B\x01\x07\x06\x2D\x2D\x61\x61\x62\x63\x10", 0 },
	{ "[^-]",	"\x01\x07\x01\x07\x02\x2D\x2D\x10", 0 },
	{ "[a-c",	0,	"Bad character class" },
	{ "[\\",	0,	"Bad character class" },

	// REVISIT: Add support for Posix groups or Unicode Properties

	// Repetition:
	{ "a?",		"\x01\x09\x01\x09\x01\x02\x02\x01\x61\x10", 0 },
	{ "a*",		"\x01\x09\x01\x09\x01\x01\x02\x01\x61\x10", 0 },
	{ "a+",		"\x01\x09\x01\x09\x02\x01\x02\x01\x61\x10", 0 },
	{ "abc?",	"\x01\x0D\x01\x02\x02\x61\x62\x09\x01\x02\x02\x01\x63\x10", 0 },
	{ "abc*",	"\x01\x0D\x01\x02\x02\x61\x62\x09\x01\x01\x02\x01\x63\x10", 0 },
	{ "abc+",	"\x01\x0D\x01\x02\x02\x61\x62\x09\x02\x01\x02\x01\x63\x10", 0 },
	{ "?",		0,	"Repeating a repetition is disallowed" },
	{ "*",		0,	"Repeating a repetition is disallowed" },
	{ "+",		0,	"Repeating a repetition is disallowed" },
	{ "abc*+def",	0,	"Repeating a repetition is disallowed" },
	{ "(abc)?",	"\x01\x0E\x01\x09\x01\x02\x0A\x07\x02\x03\x61\x62\x63\x0F\x10", 0 },
	{ "(abc)??",	0,	"Repeating a repetition is disallowed" },
	{ "(abc)?+",	0,	"Repeating a repetition is disallowed" },
	{ "(abc)?*",	0,	"Repeating a repetition is disallowed" },
	{ "(abc)*",	"\x01\x0E\x01\x09\x01\x01\x0A\x07\x02\x03\x61\x62\x63\x0F\x10", 0 },
	{ "(a|b)*?",	0,	"Repeating a repetition is disallowed" },
	{ "(abc)**",	0,	"Repeating a repetition is disallowed" },
	{ "(a|b)*+",	0,	"Repeating a repetition is disallowed" },
	{ "(abc)+",	"\x01\x0E\x01\x09\x02\x01\x0A\x07\x02\x03\x61\x62\x63\x0F\x10", 0 },
	{ "(a|b)+?",	0,	"Repeating a repetition is disallowed" },
	{ "(a|b)+*",	0,	"Repeating a repetition is disallowed" },
	{ "(a|b)++",	0,	"Repeating a repetition is disallowed" },
	{ "(a|b?|c*|d+)*",	"\x01\x24\x01\x09\x01\x01\x0A\x05\x02\x01\x61\x0D\x08\x09\x01\x02\x02\x01\x62\x0D\x08\x09\x01\x01\x02\x01\x63\x0D\x08\x09\x02\x01\x02\x01\x64\x0F\x10", 0 },

	// Repetition ranges
	{ "a{2,3}",	"\x01\x09\x01\x09\x03\x04\x02\x01\x61\x10", 0 },
	{ "a{ 2 , 3 }",	"\x01\x09\x01\x09\x03\x04\x02\x01\x61\x10", 0 },
	{ "a{2,}",	"\x01\x09\x01\x09\x03\x01\x02\x01\x61\x10", 0 },
	{ "a{2,	}",	"\x01\x09\x01\x09\x03\x01\x02\x01\x61\x10", 0 },
	{ "a{,3}",	"\x01\x09\x01\x09\x01\x04\x02\x01\x61\x10", 0 },
	{ "a{ ,3}",	"\x01\x09\x01\x09\x01\x04\x02\x01\x61\x10", 0 },
	{ "a{254,254}",	"\x01\x09\x01\x09\xFF\xFF\x02\x01\x61\x10", 0 },
	{ "a{255,255}",	0,	"Min and Max repetition are limited to 254" },
	{ "a{254,255}",	0,	"Min and Max repetition are limited to 254" },
	{ "a{-1,}",	0,	"Bad repetition count" },

	// Named groups
	{ "(?<a>b)",	"\x01\x0C\x02\x01\x61\x0B\x06\x01\x02\x01\x62\x0F\x10", 0 },
	{ "(?<foo>a|b?|c*|d+){2,5}",	"\x01\x29\x02\x03\x66\x6F\x6F\x09\x03\x06\x0B\x06\x01\x02\x01\x61\x0D\x08\x09\x01\x02\x02\x01\x62\x0D\x08\x09\x01\x01\x02\x01\x63\x0D\x08\x09\x02\x01\x02\x01\x64\x0F\x10", 0 },

	// Function calls
	{ "(?&a)",	0,	"Function call to undeclared group" },
	{ "(?&a)(?<a>b)",	"\x01\x0E\x02\x01\x61\x0E\x01\x0B\x06\x01\x02\x01\x62\x0F\x10", 0 },
	{ "(?<a>b)(?&a)",	"\x01\x0E\x02\x01\x61\x0B\x06\x01\x02\x01\x62\x0F\x0E\x01\x10", 0 },
	{ "(?<a>b(?<c>d)e(?&a))(?&b)",	0,	"Function call to undeclared group" },
	{ "(?<a>b(?<c>d)e(?&a)?)(?&c)",	"\x01\x1F\x03\x01\x61\x01\x63\x0B\x15\x01\x02\x01\x62\x0B\x06\x02\x02\x01\x64\x0F\x02\x01\x65\x09\x01\x02\x0E\x01\x0F\x0E\x02\x10", 0 },
};

int automated_tests()
{
	auto 	wrapper = [&](compiler_test* ct, int leaky_allocations = 0) -> bool
	{
		RxCompiled	rx(ct->regex, (RxFeature)(RxFeature::AllFeatures | RxFeature::ExtendedRE));
		char*		nfa = 0;
		bool		scanned_ok;

		start_recording_allocations();
		scanned_ok = rx.compile(nfa);

		// Check expectations
		bool	return_code_pass = (ct->expected_message == 0) == scanned_ok;	// Failure was reported if expected
		bool	nfa_presence_pass = (ct->expected_nfa != 0) == (nfa != 0);	// NFA was returned if one was expected
		bool	nfa_pass = !ct->expected_nfa || 0 == strcmp(nfa, ct->expected_nfa);	// NFA was correct if expected
		bool	error_message_presence_pass = (ct->expected_message != 0) == (rx.ErrorMessage() != 0);	// Message was returned if expected
		bool	error_message_pass = !ct->expected_message || 0 == strcmp(ct->expected_message, rx.ErrorMessage());	// Message was correct if expected
		bool	test_pass = return_code_pass && nfa_presence_pass && nfa_pass && error_message_presence_pass && error_message_pass;

		// Report Pass/Fixed/Pending/Fail:
		if (test_pass && !ct->expected_message)
			printf("Pass: %s\n", ct->regex);
		else if (test_pass)
			printf("Pass (with expected error): %s\n", ct->regex);
		else if (ct->expected_message && scanned_ok && nfa_pass)
			printf("Fixed: %s\n", ct->regex);
		else if (ct->expected_message)
			printf("Pending: %s\n", ct->regex);
		else
			printf(
				"Fail (%s)%s, (returned %s): %s\n",
				rx.ErrorMessage() ? rx.ErrorMessage() : "<no message returned>",
				error_message_presence_pass ? "" : " unexpected",
				scanned_ok ? "ok" : "fail",
				ct->regex
			);

		// On unexpected NFA, show it:
		if (!ct->expected_nfa && nfa)
		{
			printf("Expected NFA unknown for \"%s\". Got:\n\"", ct->regex);
			for (const char* cp = nfa; *cp; cp++)
				printf("\\x%02X", *cp&0xFF);
			printf("\"\n");
			nfa_pass = false;	// Force a dump
		}

		// On incorrect NFA, dump what we got:
		if (!nfa_pass)
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
	bool		all_ok = true;
	for (compiler_test* ct = compiler_tests; ct < compiler_tests+num_tests; ct++)
		if (!wrapper(ct))
			all_ok = false;
	return all_ok ? 0 : 1;
}
