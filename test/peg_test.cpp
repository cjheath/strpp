/*
 * Px PEG parser defined using pegular expression rules
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<peg.h>
#include	<utf8_ptr.h>

#include	<cstdio>
#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>

#include	<refcount.h>
#include	<strval.h>
#include	<variant.h>

#include	"memory_monitor.h"

#if	defined(PEG_UNICODE)
using	PegChar = UCS4;
using	Source = GuardedUTF8Ptr;
using	PegTestSourceSup = PegexpPointerSource<GuardedUTF8Ptr>;
#else
using	PegChar = char;
using	Source = PegChar*;
using	PegTestSourceSup = PegexpPointerSource<>;
#endif

// We need some help to extract captured data from PegexpPointerSource:
class PegTestSource : public PegTestSourceSup
{
public:
	PegTestSource(const Source cp) : PegTestSourceSup(cp) {}
	PegTestSource(const PegTestSource& pi) : PegTestSourceSup(pi) {}

	const UTF8*	peek() const { return data; }
};

class PegTestResult
{
public:
	using Source = PegTestSource;

	// Any subclasses must have this constructor, to capture matched text:
	PegTestResult(Source _from, Source _to)
	: var(StrVal(_from.peek(), (int)_to.bytes_from(_from)))
	{ }
	PegTestResult(Variant _var)
	: var(_var)
	{ }

	Variant		var;
};

class	PegContext
{
public:
	static const int MaxCaptureNames = 3;	// Sufficient for the Px grammar below

	using	Source = PegTestSource;
	using	Result = PegTestResult;
	using	PegT = Peg<Source, Result, PegContext>;
	using	Rule = PegRule<PegPegexp<PegContext>, MaxCaptureNames>;

	PegContext(PegT* _peg, PegContext* _parent, Rule* _rule, Source _origin)
	: peg(_peg)
	, capture_disabled(_parent ? _parent->capture_disabled : 0)
	, repetition_nesting(0)
	, parent(_parent)
	, rule(_rule)
	, origin(_origin)
	, num_captures(0)
	{}

	int		capture(PegexpPC name, int name_len, Result r, bool in_repetition)
	{
		StrVal		key(name, name_len);
		Variant		existing;

		if (ast.contains(key))
		{		// There are previous captures under this name
			existing = ast[key];
			if (existing.get_type() != Variant::VarArray)
				existing = Variant(&existing, 1);	// Convert it to an array
			VariantArray va = existing.as_variant_array();
			va += r.var;		// This Unshares va from the entry stored in the map, so
			ast.erase(key);		// Remove that
			ast.insert(key, va);	// and save the new version
			existing = ast[key];
		}
		else	// Insert the result as the first element in an array, or just as itself:
			ast.insert(key, in_repetition ? Variant(&r.var, 1) : r.var);


	//	printf("Capture "); print_path();
		printf(
			"%p (capture %d): Saving %d'th %s'%s' = type %s\n",
			this,
			num_captures,
			existing.get_type() == Variant::VarArray ? existing.as_variant_array().length() : 1,
			in_repetition ? "(in rep) " : "",
			key.asUTF8(),
			r.var.type_name()
		);
		num_captures++;

		return 0;
	}

	int		capture_count() const
	{
		return num_captures;
	}
	void		rollback_capture(int count)
	{
		if (count >= num_captures)
			return;
		printf("REVISIT: Not rolling back to %d from %d\n", count, num_captures);
	}
	int		capture_disabled;
	int		repetition_nesting;

	Result		all_captures()
	{
		return Result(ast);
	}

	PegT*		peg;
	PegContext* 	parent;
	Rule*		rule;
	Source		origin;		// Location where this rule started, for detection of left-recursion

	void		print_path(int depth = 0) const
	{
		if (parent)
		{
			parent->print_path(depth+1);
			printf("->");
		}
		else
			printf("@depth=%d: ", depth);
		printf("%s", rule->name);
	}

protected:
	int		num_captures;
	StrVariantMap	ast;
};

typedef	Peg<PegTestSource, PegTestResult, PegContext>	TestPeg;

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

int	parse_file(char* text)
{
	using R = TestPeg::Rule;
	TestPeg::Rule	rules[] =
	{
		{ "blankline",				// A line containing no printing characters
		  "\\n*[ \\t\\r](|\\n|!.)",
		  {}
		},
		{ "space",				// Any single whitespace
		  "|[ \\t\\r\\n]"
		  "|//*[^\\n]",				// including a comment to end-of-line
		  {}
		},
		{ "s",					// Any whitespace but not a blankline
		  "*(!<blankline><space>)",
		  {}
		},
		{ "TOP",				// Start; a repetition of zero or more rules
		  "*<space>*<rule>:rule",
		  { "rule" }				// -> rule
		},
		{ "rule",				// Rule: name of a rule that matches one or more alternates
		  "<name><s>=<s>"
		  "<alternates>?<action>"		// and perhaps has an action
		  "<blankline>*<space>",		// it ends in a blank line
		  { "name", "alternates", "action" }
		},
		{ "action",				// Looks like "-> function: param, ..."
		  "-><s>?(<name>:function\\:<s>)<parameter>:list*(,<s><parameter>:list)<s>",
		  { "function", "list" }
		},
		{ "parameter",				// A parameter to an action
		  "(|<reference>:param|<literal>:param)<s>",
		  { "param" }
		},
		{ "reference",				// A reference (name sequence) to descendents of a result.
		  "<name><s>*([.*]<s><name>)",		// . means only one, * means "all"
		  {}
		},
		{ "alternates",				// Alternates:
		  "|+(\\|<s><sequence>)"		// either a list of alternates each prefixed by |
		  "|<sequence>",			// or just one alternate
		  { "sequence" }
		},
		{ "sequence",				// Alternates:
		  "*<repetition>",			// either a list of alternates each prefixed by |
		  { "repetition" }
		},
		{ "repeat_count",			// How many times must the following be seen:
		  "(|[?*+!&]:limit<s>"			// zero or one, zero or more, one or more, none, and
		  "|<count>:limit",
		  { "limit" }
		},
		{ "count",
		  "\\{(|(+\\d):val|<name>:val)<s>\\}",	// {literal count or a reference to a saved variable}
		  { "val" }
		},
		{ "repetition",				// a repeated atom perhaps with label
		  "?<repeat_count><atom>?<label><s>",
		  { "repeat_count", "atom", "label" }
		},
		{ "label",				// A name for the previous atom
		  "\\:<name>",
		  { "name" }
		},
		{ "atom",
		  "|\\."				// Any character
		  "|<name>"				// call to another rule
		  "|<property>"				// A character property
		  "|<literal>"				// A literal
		  "|<class>"				// A character class
		  "|<nested_alternates>",
		  {}
		},
		{ "nested_alternates",
		  "\\(<s>+<alternates>\\)",		// A parenthesised group
		  {}
		  // -> alternates
		},
		{ "name",
		  "[\\a_]*[\\w_]",
		  {}
		},
		{ "literal",
		  "'*(!'<lit_char>)'",
		  {}
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
		  {}
		},
		{ "property",				// alpha, digit, hexadecimal, whitespace, word (alpha or digit)
		  "\\\\[adhsw]",
		  {}
		},
		{ "class",				// A character class
		  "\\[?\\^?-+<class_part>]",
		  {}
		},
		{ "class_part",
		  "!]<class_char>?(-!]<class_char>)",
		  {}
		},
		{ "class_char",
		  "![-\\]]<lit_char>",
		  {}
		},
	};

	TestPeg		peg(rules, sizeof(rules)/sizeof(rules[0]));

	TestPeg::State	result = peg.parse(text);

	if (result.text.peek() > text)
		return result.text.peek() - text;
	return 0;
}

int
main(int argc, const char** argv)
{
	if (argc < 2)
		usage();

	start_recording_allocations();

	off_t	file_size;
	char*	px = slurp_file(argv[1], &file_size);

	int		bytes_parsed;
	bytes_parsed = parse_file(px);
	delete px;

	printf("Parsed %d bytes of %d\n", bytes_parsed, (int)file_size);

	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();

	return bytes_parsed == file_size ? 0 : 1;
}
