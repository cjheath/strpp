/*
 * Test program using the Px PEG parser, defined using pegular expression rules
 *
 * Copyright 2025 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<refcount.h>
#include	<strval.h>
#include	<variant.h>

#include	<peg.h>
#include	<peg_ast.h>
#include	<utf8_ptr.h>

#include	<cstdio>
#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>

#include	"memory_monitor.h"

typedef	Peg<PegMemorySource, PegMatch, PegContext>	PxParser;

void usage()
{
	fprintf(stderr, "Usage: peg_test peg.px\n");
	exit(1);
}

char* slurp_file(const char* filename, off_t* size_p)
{
	// Open the file and get its size
	int		fd;
	struct	stat	stat;
	char*		px;
	if ((fd = open(filename, O_RDONLY)) < 0		// Can't open
	 || fstat(fd, &stat) < 0			// Can't fstat
	 || (stat.st_mode&S_IFMT) != S_IFREG		// Not a regular file
	 || (px = new char[stat.st_size+1]) == 0	// Can't get memory
	 || read(fd, px, stat.st_size) < stat.st_size)	// Can't read entire file
	{
		perror(filename);
		usage();
	}
	if (size_p)
		*size_p = stat.st_size;
	px[stat.st_size] = '\0';

	return px;
}

// It's a pity that C++ has no way to initialise these string arrays inline:
const char*	TOP_captures[] = { "rule", 0 };
const char*	rule_captures[] = { "name", "alternates", "action" };
const char*	action_captures[] = { "function", "parameter", 0 };
const char*	parameter_captures[] = { "parameter", 0 };
const char*	reference_captures[] = { "name", "joiner", 0 };
const char*	alternates_captures[] = { "sequence", 0 };
const char*	sequence_captures[] = { "repetition", 0 };
const char*	repeat_count_captures[] = { "limit", 0 };
const char*	count_captures[] = { "val", 0 };
const char*	repetition_captures[] = { "repeat_count", "atom", "label", 0 };
const char*	label_captures[] = { "name", 0 };
const char*	atom_captures[] = { "atom", 0 };
const char*	group_captures[] = { "alternates", 0 };

PxParser::Rule	rules[] =
{
	{ "blankline",				// A line containing no printing characters
	  "\\n*[ \\t\\r](|\\n|!.)",
	  0
	},
	{ "space",				// Any single whitespace
	  "|[ \\t\\r\\n]"
	  "|//*[^\\n]",				// including a comment to end-of-line
	  0
	},
	{ "s",					// Any whitespace but not a blankline
	  "*(!<blankline><space>)",
	  0
	},
	{ "TOP",				// Start; a repetition of zero or more rules
	  "*<space>*<rule>:rule",
	  // "*<space><rule>:rule",		// Parse one rule at a time
	  TOP_captures
	  // { "rule" }				// -> rule
	},
	{ "rule",				// Rule: name of a rule that matches one or more alternates
	  "<name><s>=<s>"
	  "<alternates>?<action>"		// and perhaps has an action
	  "<blankline>*<space>",		// it ends in a blank line
	  rule_captures
	},
	{ "action",				// Looks like "-> function: param, ..."
	  "-><s>?(<name>:function\\:<s>)<parameter>*(,<s><parameter>)<s>",
	  action_captures
	},
	{ "parameter",				// A parameter to an action
	  "(|<reference>:parameter|<literal>:parameter)<s>",
	  parameter_captures
	},
	{ "reference",				// A reference (name sequence) to descendents of a match.
	  "<name><s>*([.*]:joiner<s><name>)",	// . means only one, * means "all"
	  reference_captures
	},
	{ "alternates",				// Alternates:
	  "|+(\\|<s><sequence>)"		// either a list of alternates each prefixed by |
	  "|<sequence>",			// or just one alternate
	  alternates_captures
	},
	{ "sequence",				// Alternates:
	  "*<repetition>",			// either a list of alternates each prefixed by |
	  sequence_captures
	},
	{ "repeat_count",			// How many times must the following be seen:
	  "(|[?*+!&]:limit<s>"			// zero or one, zero or more, one or more, none, and
	  "|<count>:limit",
	  repeat_count_captures
	},
	{ "count",
	  "\\{(|(+\\d):val|<name>:val)<s>\\}",	// {literal count or a reference to a captured variable}
	  count_captures
	},
	{ "repetition",				// a repeated atom perhaps with label
	  "?<repeat_count><atom>?<label><s>",
	  repetition_captures
	},
	{ "label",				// A name for the previous atom
	  "\\:<name>",
	  label_captures
	},
	{ "atom",
	  "|\\.:atom"				// Any character
	  "|<name>:atom"			// call to another rule
	  "|<property>:atom"			// A character property
	  "|<literal>:atom"			// A literal
	  "|<class>:atom"			// A character class
	  "|<group>:atom",
	  atom_captures
	},
	{ "group",
	  "\\(<s>+<alternates>\\)",		// A parenthesised group
	  group_captures
	},
	{ "name",
	  "[\\a_]*[\\w_]",
	  0
	},
	{ "literal",
	  "'*(!'<lit_char>)'",
	  0
	},
	{ "lit_char",
	  "|\\\\(|?[0-3][0-7]?[0-7]"		// octal character constant
	      "|x\\h?\\h"			// hexadecimal constant \x12
	      "|x{+\\h}"			// hexadecimal constant \x{...}
	      "|u?[01]\\h?\\h?\\h?\\h"		// Unicode character \u1234
	      "|u{+\\h}"			// Unicode character \u{...}
	      "|[^\\n]"				// Other special escape except newline
	     ")"
	  "|[^\\\\\\n]",				// any normal character except backslash or newline
	  0
	},
	{ "property",				// alpha, digit, hexadecimal, whitespace, word (alpha or digit)
	  "\\\\[adhsw]",
	  0
	},
	{ "class",				// A character class
	  "\\[?\\^?-+<class_part>]",
	  0
	},
	{ "class_part",
	  "!]<class_char>?(-!]<class_char>)",
	  0
	},
	{ "class_char",
	  "![-\\]]<lit_char>",
	  0
	},
};

PxParser::Match
parse_file(char* text)
{
	PxParser		peg(rules, sizeof(rules)/sizeof(rules[0]));

	PxParser::Source	source(text);
	return peg.parse(source);
}

int
parse_and_report(const char* filename)
{
	off_t	file_size;
	char*	text = slurp_file(filename, &file_size);

	PxParser::Match match = parse_file(text);

	int		bytes_parsed;

	bytes_parsed = match.is_failure() ? 0 : match.furthermost_success.peek() - text;

	if (bytes_parsed < file_size)
	{
		printf("Parse %s at line %lld column %lld (byte %lld of %d). Next tokens anticipated were:\n",
			bytes_parsed > 0 ? "finished early" : "failed",
			match.furthermost_success.current_line(),
			match.furthermost_success.current_column(),
			match.furthermost_success.current_byte(),
			(int)file_size
		);

		for (int i = 0; i < match.failures.length(); i++)
		{
			PegFailure	f = match.failures[i];
			printf("\t%.*s\n", f.atom_len, f.atom);
		}
	}
	else
		printf("Parsed %d bytes of %d\n", bytes_parsed, (int)file_size);

	if (bytes_parsed > 0)
		printf("Parse Tree:\n%s\n", match.var.as_json(0).asUTF8());

	delete text;

	return bytes_parsed == file_size ? 0 : 1;
}

int
main(int argc, const char** argv)
{
	if (argc < 2)
		usage();

	start_recording_allocations();

	int code = parse_and_report(argv[1]);

	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();

	return code;
}
