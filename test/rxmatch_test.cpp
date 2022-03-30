/*
 * Unicode Strings
 * Unit test driver for the Regular Expression matcher
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<stdio.h>
#include	<string.h>

#define	protected	public
#include	<strregex.h>

#include	"memory_monitor.h"

bool	verbose = false;
bool	show_all_expectations = false;
int	automated_tests();
void	show_expectation(const char* regex, const char* target, int match_offset, int match_length);

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

	RxProgram*	program = 0;
	char*		nfa = 0;
	for (--argc, ++argv; argc > 0; argc--, argv++)
	{
		if (**argv == '/')
		{
			StrVal		re(*argv+1);

			if (re.tail(1) == "/")	// Remove the trailing /
				re = re.shorter(1);
			printf("Compiling \"%s\"\n", re.asUTF8());

			RxCompiler	rx(re, (RxFeature)((int32_t)RxFeature::AllFeatures | (int32_t)RxFeature::ExtendedRE));
			bool		scanned_ok;
			delete nfa;
			nfa = 0;
			if (!rx.compile(nfa))
			{
				printf("Regex scan failed: %s\n", rx.errorMessage());
				program = 0;
				continue;
			}
			rx.dump(nfa);

			delete program;
			program = new RxProgram(nfa);
		}
		else if (program)
		{
			StrVal		target(*argv);
			RxResult	result = program->matchAfter(target);

			if (result.succeeded())
			{
				StrVal	matched_substr = target.substr(result.offset(), result.length());
				printf("\t\"%s\" matched at [%d, %d]: \"%s\"\n", target.asUTF8(), result.offset(), result.length(), matched_substr.asUTF8());
				continue;
			}
			printf("\t\"%s\" failed\n", *argv);
		}
		else
			printf("\"%s\" has no regex to match\n", *argv);
	}
	if (program)
		delete program;
	if (nfa)
		delete[] nfa;
	return 0;
}

struct	matcher_test {
	const char*	regex;			// the regex to compile
	const char*	target;			// The NFA string which is expected from the compiler
	int		offset;
	int		length;
};
matcher_test	matcher_tests[] =
{
	{ 0,	"Null expressions", 0, 0 },
	{ "",		"a",			0, 0 },
	{ "|",		"a",			0, 0 },		// FAILS with more active threads than budgeted
	{ "|a|",	"a",			0, 1 },
	{ "a||",	"a",			0, 1 },

	{ 0,	"Escaped special characters", 0, 0 },
	{ "\\|",	"|",			0, 1 },
	{ "\\(",	"(",			0, 1 },
	{ "\\)",	")",			0, 1 },
	{ "\\*",	"*",			0, 1 },
	{ "\\+",	"+",			0, 1 },
	{ "\\?",	"?",			0, 1 },
	{ "{",		0,			0, 1 },		// "}"
	{ "}",		"}",			0, 1 },
	{ "\\.",	".",			0, 1 },
	{ "\\^",	"^",			0, 1 },
	{ "\\$",	"$",			0, 1 },
	{ "\\\\",	"\\",			0, 1 },
	{ "\\-",	"-",			0, 1 },
	{ "-",		"-",			0, 1 },
	{ "\\_",	"_",			0, 1 },

	{ 0,	"Literals", 0, 0 },
	{ "a",		"a",			0, 1 },
	{ "ab",		"babc",			1, 2 },
	{ "abc",	"babcd",		1, 3 },

	{ 0,	"Start and End of line", 0, 0 },
	{ "^",		"ba",			0, 0 },
	{ "^a",		"ba",			-1, 0 },
	{ "^a",		"ab",			0, 1 },
	{ "^a",		"\na",			1, 1 },
	{ "^a",		"b\na",			2, 1 },
	{ "$",		"ba",			2, 0 },
	{ "a$",		"ab",			-1, 0 },
	{ "a$",		"ba",			1, 1 },
	{ "a$",		"bab",			-1, 0 },
	{ "a$",		"ba\n",			1, 1 },
	{ "a$",		"a\nb",			0, 1 },
	{ "a|^",	"b\nc",			0, 0 },
	{ "a|^c",	"b\nc",			2, 1 },

	{ 0,	"Any character", 0, 0 },
	{ ".",		"ba",			0, 1 },
	{ "a.",		"bac",			1, 2 },
	{ "a.c",	"dabcd",		1, 3 },
	{ "a.b.c",	"dadbacd",		1, 5 },

	{ 0,	"Character classes", 0, 0 },
	{ "[ace]",	"a",			0, 1 },
	{ "[ace]",	"d",			-1, 0 },
	{ "[abc]",	"b",			0, 1 },
	{ "[abc]",	"c",			0, 1 },
	{ "[abc]",	"d",			-1, 0 },
	{ "[a-z]",	"m",			0, 1 },
	{ "[a]",	"a",			0, 1 },
	{ "[^ace]",	"b",			0, 1 },
	{ "[^ace]",	"c",			-1, 0 },
	{ "[^0-9]",	"c",			0, 1 },
	{ "[^0-9]+",	"049cb012",		3, 2 },
	{ "[-ace]",	"-",			0, 1 },
	{ "[ace-]",	"-",			0, 1 },
	{ "[a\\-e]",	"-",			0, 1 },
	{ "[a\\]e]",	"]",			0, 1 },
	{ "[ace",	0,			0, 0 },
	{ "[ace\\",	0,			0, 0 },
	{ "[a-\\",	0,			0, 0 },

	{ 0,	"Character properties", 0, 0 },
	{ "\\s",	"#",			-1, 0 },
	{ "\\s",	"a b",			1, 1 },
	{ "\\d",	"a0b",			1, 1 },
	{ "\\d",	"a9b",			1, 1 },
	{ "\\d",	"a\u0660b",		1, 1 },	// U+0660 Arabic-Indic Digit Zero 
	{ "\\d",	"a\u0669b",		1, 1 },	// U+0669 Arabic-Indic Digit Nine
	{ "\\h",	"x0z",			1, 1 },
	{ "\\h",	"x9z",			1, 1 },
	{ "\\h",	"x\u0660z",		1, 1 },
	{ "\\h",	"x\u0660z",		1, 1 },
	{ "\\h",	"xaz",			1, 1 },
	{ "\\h",	"xAz",			1, 1 },
	{ "\\h",	"xfz",			1, 1 },
	{ "\\h",	"xFz",			1, 1 },
	{ "\\h",	"xgz",			-1, 0 },
	{ "\\h",	"xGz",			-1, 0 },
	// REVISIT: Implement property callbacks and add test cases here
	// "\\p{Braille}",	

	{ 0,	"Repetition", 0, 0 },
	{ "a?",		"b",			0, 0 },
	{ "a?",		"a",			0, 1 },
	{ "a?",		"aa",			0, 1 },
	{ "a?",		"ba",			0, 0 },

	{ "a*",		"b",			0, 0 },
	{ "a*",		"a",			0, 1 },
	{ "a*",		"aa",			0, 2 },
	{ "a*",		"ba",			0, 0 },
	{ "a*",		"baac",			0, 0 },

	{ "a+",		"b",			-1, 0 },
	{ "a+",		"a",			0, 1 },
	{ "a+",		"aa",			0, 2 },
	{ "a+",		"ba",			1, 1 },
	{ "a+",		"baac",			1, 2 },
	{ "(x{2}){2}",	"axxxxb",		1, 4 },
	{ "((x{2}){2}){2}",	"axxxxxxxxb",	1, 8 },
	{ "(((x{2}){2}){2}){2}",	"axxxxxxxxxxxxxxxxxxb",	1, 16 },
//	{ "((((x{2}){2}){2}){2}){2}",	"axxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxb",	1, 32 },	// Fails. Probably a duplicate thread issue. Care factor?
	{ "(((((((x{2}){2}){2}){2}){2}){2}){2}){2}",	"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",	-1, 256 },	// Counters 8 deep
	{ "((((((((x{2}){2}){2}){2}){2}){2}){2}){2}){2}",	0,	-1, 0 },	// Nesting 9 deep, Counters 9 deep

	{ "a{2,4}",	"b",			-1, 0 },
	{ "a{2,4}",	"aa",			0, 2 },
	{ "a{2,4}",	"aaa",			0, 3 },
	{ "a{2,4}",	"aaaa",			0, 4 },
	{ "a{2,4}",	"aaaaa",		0, 4 },
	{ "a{2,4}",	"a",			-1, 1 },
	{ "a{2,4}",	"ba",			-1, 0 },
	{ "a{2,4}",	"baa",			1, 2 },
	{ "a{2,4}",	"baac",			1, 2 },
	{ "a{2,4}",	"baaaac",		1, 4 },
	{ "ba*b",	"baaab",		0, 5 },
	{ "a{2}b",	"baaab",		2, 3 },
	{ "a{2,}b",	"baaab",		1, 4 },
	{ "ba{2}b",	"baaab",		-1, 0 },
	{ "ba{2}b",	"baab",			0, 4 },
	{ "ba{2}a",	"baaa",			0, 4 },
	{ "ba{2,4}a",	"baaa",			0, 4 },
	{ "ba{2,4}a",	"baaaa",		0, 5 },
	{ "ba{2,3}a{2,3}",	"baaaaa",	0, 6 },
	{ "b(a){2,3}a{2,3}",	"baaaaa",	0, 6 },
	{ "b(a{2,3})(a{2,3})",	"baaaaa",	0, 6 },

	/* REVISIT: Non-greedy repetition (not implemented)
	"a*?"
	"a+?"
	"a??"
	"a{2}?"
	"a{2,3}?"
	"a{2,}?"
	*/

	{ 0,	"Resolving ambiguous paths", 0, 0 },
	{ "a{2}a",	"baaaa",		1, 3 },
	{ "ba*a",	"baaaab",		0, 5 },
	{ "a(bc|b)c",	"abcc",			0, 4 },
	{ "a*aa",	"baaaab",		1, 4},
	{ "a(bc|b)c",	"abc",			0, 3 },
	{ "a?a",	"a",			0, 1},
	{ "a(bc|b)+c",	"abcbc",		0, 5 },
	{ "a(b|bc)+c",	"abcbc",		0, 5 },

	{ "()*c",	"abcbc",		2, 1 },
	{ "((a)*)*",	"aaaaa",		0, 5 },

	// { 0,	"Alternates", 0, 0 },
	{ "b|c",	"abc",			1, 1 },

	// { 0,	"Non-capturing groups", 0, 0 },
	{ "(a)",	"a",			0, 1 },
	{ "(b)",	"ab",			1, 1 },
	{ "(a)|b",	"ab",			0, 1 },

	// { 0,	"Named groups", 0, 0 },

	// { 0,	"Negative lookahead", 0, 0 },
	{ "((?!a)[a-z])*",	"abcd",		0, 0 },
	{ "((?!a)[a-z])+",	"abcd",		1, 3 },
	{ "((?!ac)[a-z])+",	"abcdbacd",	0, 5 },
	{ "((?!cd)[a-e][c-f])+",	"abcdeefdccdf", 1, 8 },		// Note the occurrence of the stopping expression 'cd' out of alignment with the pairs
	{ "((?!dc)[a-e][c-f])+",	"abcdeefcdcddcf", 1, 10 },
};

StrVal
escape(StrVal str)
{
	StrVal	double_backslash("\\\\");
	str.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4    ch = UTF8Get(cp);       // Get UCS4 character
			switch (ch)
			{
			case '\\':	return double_backslash;
			case '\b':	return "\\b";
			case '\e':	return "\\e";
			case '\f':	return "\\f";
			case '\n':	return "\\n";
			case '\t':	return "\\t";
			case '\r':	return "\\r";
			case 0x1F:	return "^~";
			default:
				if (ch < ' ')
					return StrVal("^")+char32_t(ch+'@');
				return ch;
			}
		}
	);
	return str;
}

int automated_tests()
{
	auto 	wrapper = [&](matcher_test* test_case, int leaky_allocations = 0) -> bool
	{
		if (!test_case->regex)
		{
			if (show_all_expectations)
				printf(" { 0,\t\"%s\",\t0, 0 },\n", test_case->target);
			return true;
		}

		RxCompiler	rx(test_case->regex, (RxFeature)((int32_t)RxFeature::AllFeatures | (int32_t)RxFeature::ExtendedRE));
		char*		nfa = 0;
		bool		scanned_ok;
		bool		test_passed;

		if (show_all_expectations)
			printf("\t{ %s\t\"%s\",\t%d, %d },\n", test_case->regex, test_case->target, test_case->offset, test_case->length);

		scanned_ok = rx.compile(nfa);
		if (!scanned_ok)
		{
			printf("%s: /%s/: %s\n", test_case->target != 0 ? "Fail" : "Pass", test_case->regex, rx.errorMessage());
			return test_case->target == 0;	// Bad news if we expected it to succeed
		}

		start_recording_allocations();
		{
			RxProgram	program(nfa);
			StrVal		target(test_case->target);
			StrVal		target_escaped = escape(target);
			RxResult	result = program.matchAfter(target);

			bool		success_ok = result.succeeded() == (test_case->offset >= 0);	// Is success status as expected?
			bool		offset_ok = !result.succeeded() || result.offset() == test_case->offset;
			bool		length_ok = !result.succeeded() || result.length() == test_case->length;
			test_passed = success_ok && offset_ok && length_ok;

			printf("%s: /%s/ =~ \"%s\"", test_passed ? "Pass" : "Fail", test_case->regex, target_escaped.asUTF8());
			if (result.succeeded())
				printf(" -> (%d, %d)", result.offset(), result.length());
			else
				printf(" (no match)");
			if (!test_passed)
			{
				if (test_case->offset < 0)
					printf(", unexpected match");
				else
					printf(", expected (%d, %d)", test_case->offset, test_case->length);
			}
			printf("\n");

			if (nfa)
				delete[] nfa;
		}

		if (scanned_ok && unfreed_allocation_count() > leaky_allocations)
		{
			printf("Unfreed allocations after matching \"%s\":\n", test_case->regex);
			report_allocations();
		}

		return test_passed;
	};

	size_t		num_tests = sizeof(matcher_tests)/sizeof(matcher_tests[0]);
	int		failures = 0;
	for (matcher_test* test_case = matcher_tests; test_case < matcher_tests+num_tests; test_case++)
		if (!wrapper(test_case))
			failures++;
	printf("\n%d tests run with %d failures\n", (int)num_tests, failures);
	return failures > 0 ? 1 : 0;
}

void	show_expectation(const char* regex, const char* target, int match_offset, int match_length)
{
}
