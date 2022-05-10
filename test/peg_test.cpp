/*
 * Px PEG parser defined using pegular expression rules
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<peg.h>
#include	<utf8pointer.h>

#include	<stdio.h>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>

#include	<refcount.h>
#include	<strpp.h>

#if	defined(PEG_UNICODE)
typedef	UTF8P	PegText;
typedef	UCS4	PegChar;
#else
typedef	TextPtrChar	PegText;
typedef	char	PegChar;
#endif

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
	void            save(PegexpPC name, PegText from, PegText to) {}
};

typedef	Peg<PegText, PegChar, PegCapture>	TestPeg;

int
main(int argc, const char** argv)
{
	using R = TestPeg::Rule;
	TestPeg::Rule	rules[] = {
		{ "blankline",	"\\n*[ \\t\\r](|\\n|!.)"			},	// A line containing no printing characters
		{ "space",	"|[ \\t\\r\\n]|//*[^\\n]"			},	// Any single whitespace
		{ "s",		"*(!<blankline><space>)"			},	// Any whitespace but not a blankline
		{ "TOP",	"*<space>*<rule>"				},	// Start; a repetition of zero or more rules
		{ "rule",	"<name><s>=<s><alternates><blankline>*<space>"	},	// Rule: a name assigned one or more alternates
		{ "alternates",	"|+(\\|<s>*<repetition>)"				// Alternates: either a list of alternates each prefixed by |
				"|*<repetition>"				},	// or just one alternate
		{ "repetition",	"?[?*+!&]<s><atom>"				},	// zero or one, zero or more, one or more, none, and
		{ "atom",	"|\\."							// Any character
				"|<name>"						// call to another rule
				"|<property>"						// A character property
				"|<literal>"						// A literal
				"|<class>"						// A character class
				"|\\(<s>+<alternates>\\)<s>"				// A parenthesised group
		},
		{ "name",	"[\\a_]*[\\w_]<s>"				},
		{ "literal",	"'*(!'<lit_char>)'<s>"				},
		{ "lit_char",
				"|\\\\(|?[0-3][0-7]?[0-7]"				// octal character constant
				      "|x\\h?\\h"					// hexadecimal constant \x12
				      "|x{+\\h}"					// hexadecimal constant \x{...}
				      "|u?[01]\\h?\\h?\\h?\\h"				// Unicode character \u1234
				      "|u{+\\h}"					// Unicode character \u{...}
				      "|[^\\n]"						// Other special escape except newline
				     ")"
				"|[^\\\\\\n]"						// any normal character except backslash or newline
		},
		{ "property",	"\\\\[adhsw]<s>"				},	// alpha, digit, hexadecimal, whitespace, word (alpha or digit)
		{ "class",	"\\[?\\^?-+<class_part>]<s>"			},	// A character class
		{ "class_part",	"!]<class_char>?(-!]<class_char>)"		},
		{ "class_char",	"![-\\]]<lit_char>"				},
	};

	// Open the file and get its size
	int		fd;
	struct	stat	stat;
	char*		px;
	if (argc < 2					// No file given
	 || (fd = open(argv[1], O_RDONLY)) < 0		// Can't open
	 || fstat(fd, &stat) < 0			// Can't fstat
	 || (stat.st_mode&S_IFMT) != S_IFREG		// Not a regular file
	 || (px = new char[stat.st_size+1]) == 0	// Can't get memory
	 || read(fd, px, stat.st_size) < stat.st_size)	// Can't read entire file
	{
		perror(argv[1]);
		fprintf(stderr, "Usage: %s peg.px\n", argv[0]);
		return 1;
	}
	px[stat.st_size] = '\0';

	TestPeg		peg(rules, sizeof(rules)/sizeof(rules[0]));
	typename TestPeg::State	result = peg.parse(px);

	int		bytes_parsed = result ? result.text-px : 0;
	printf("Parsed %d bytes of %d\n", bytes_parsed, (int)stat.st_size);

	return bytes_parsed == stat.st_size ? 0 : 1;
}
