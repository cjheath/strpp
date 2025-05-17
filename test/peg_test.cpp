/*
 * Px PEG parser defined using pegular expression rules
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<peg.h>
#include	<utf8_ptr.h>

#include	<stdio.h>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>

#include	<refcount.h>
#include	<strval.h>

#if	defined(PEG_UNICODE)
using	PegChar = UCS4;
using	PegText = PegexpPointerInput<GuardedUTF8Ptr>;
#else
using	PegChar = char;
using	PegText = PegexpPointerInput<>;
#endif
using	PegexpT = Pegexp<PegText, PegChar>;

class	PegCaptureBody
: public RefCounted
{
	StrVal		s;
public:
	PegCaptureBody() {}
	PegCaptureBody(const PegCaptureBody& c) : s(c.s) {}
};

class	PegCapture
{
	Ref<PegCaptureBody>	body;
	void		Unshare()	// Get our own copy that we can safely mutate
	{
		if (body->GetRefCount() > 1)
			body = new PegCaptureBody(*body);
	}
public:
	void            save(PegexpPC name, PegText from, PegText to)
	{
		printf("Capture '%.20s': '%.*s'\n", name, (int)to.bytes_from(from), from.peek());
	}
};

typedef	Peg<PegText, PegCapture>	TestPeg;

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

int
main(int argc, const char** argv)
{
	using R = TestPeg::Rule;
	TestPeg::Rule	rules[] = {
		{ "blankline",				// A line containing no printing characters
		  "\\n*[ \\t\\r](|\\n|!.)"
		},
		{ "space",				// Any single whitespace
		  "|[ \\t\\r\\n]"
		  "|//*[^\\n]"				// including a comment to end-of-line
		},
		{ "s",					// Any whitespace but not a blankline
		  "*(!<blankline><space>)"
		},
		{ "TOP",				// Start; a repetition of zero or more rules
		  "*<space>*<rule>"
		},
		{ "rule",				// Rule: name of a rule that matches one or more alternates
		  "<rule_name><s>=<s>"
		  "<alternates>?<action>"		// and perhaps has an action
		  "<blankline>*<space>"			// it ends in a blank line
		},
		{ "rule_name",				// Rule: a name for a rule
		  "<name>:rule_name"
		},
		{ "action",				// Looks like "-> function_call(param, ...)"
		  "-><s><name>\\(<s>?<parameter_list>\\)<s>"
		},
		{ "parameter_list",			// A list of parameters to an action
		  "<parameter>*(,<s><parameter>)"
		},
		{ "parameter",				// A parameter to an action
		  "(|<reference>|<literal>)<s>"
		},
		{ "reference",				// A reference (name sequence) to descendents of a result.
		  "<name><s>*([.*]<s><name>)"		// . means only one, * means "all"
		},
		{ "alternates",				// Alternates:
		  "|+(\\|<s>*<repetition>)"		// either a list of alternates each prefixed by |
		  "|*<repetition>"			// or just one alternate
		},
		{ "repeat_count",			// How many times must the following be seen:
		  "(|[?*+!&]"				// zero or one, zero or more, one or more, none, and
		  "|{(|+\\d|<name><s>)})"		// {literal count or a reference to a saved variable}
		},
		{ "repetition",				// a repeated atom perhaps with label
		  "?<repeat_count><atom>?<label><s>"
		},
		{ "label",				// A name for the previous atom
		  "\\:<name>"
		},
		{ "atom",
		  "|\\."				// Any character
		  "|<name>"				// call to another rule
		  "|<property>"				// A character property
		  "|<literal>"				// A literal
		  "|<class>"				// A character class
		  "|\\(<s>+<alternates>\\)"		// A parenthesised group
		},
		{ "name",
		  "[\\a_]*[\\w_]"
		},
		{ "literal",
		  "'*(!'<lit_char>)'"
		},
		{ "lit_char",
		  "|\\\\(|?[0-3][0-7]?[0-7]"		// octal character constant
		      "|x\\h?\\h"			// hexadecimal constant \x12
		      "|x{+\\h}"			// hexadecimal constant \x{...}
		      "|u?[01]\\h?\\h?\\h?\\h"		// Unicode character \u1234
		      "|u{+\\h}"			// Unicode character \u{...}
		      "|[^\\n]"				// Other special escape except newline
		     ")"
		  "|[^\\\\\\n]"				// any normal character except backslash or newline
		},
		{ "property",				// alpha, digit, hexadecimal, whitespace, word (alpha or digit)
		  "\\\\[adhsw]"
		},
		{ "class",				// A character class
		  "\\[?\\^?-+<class_part>]"
		},
		{ "class_part",
		  "!]<class_char>?(-!]<class_char>)"
		},
		{ "class_char",
		  "![-\\]]<lit_char>"
		},
	};

	if (argc < 2)
		usage();

	off_t	file_size;
	char*	px = slurp_file(argv[1], &file_size);

	TestPeg		peg(rules, sizeof(rules)/sizeof(rules[0]));
	typename TestPeg::State	result = peg.parse(px);

	int		bytes_parsed = result ? result.text.bytes_from(px) : 0;
	printf("Parsed %d bytes of %d\n", bytes_parsed, (int)file_size);

	return bytes_parsed == file_size ? 0 : 1;
}
