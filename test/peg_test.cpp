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
	PegTestSource() : PegTestSourceSup() {}
	PegTestSource(const Source cp) : PegTestSourceSup(cp) {}
	PegTestSource(const PegTestSource& pi) : PegTestSourceSup(pi) {}

	const UTF8*	peek() const { return data; }
};

class PegTestFailure {
public:
	PegexpPC	atom;		// Start of the Pegexp literal we failed to match
	int		atom_len;	// Length of the literal (for reporting)
};
typedef Array<PegTestFailure>	PegTestFailures;	// A Failure for each atom tried at furthermost_success location

class PegTestMatch
{
public:
	using Source = PegTestSource;

	PegTestMatch()
	{}

	// Capture matched text:
	PegTestMatch(Source from, Source to)
	{
		if (!to.is_null())
			var = StrVal(from.peek(), (int)(to - from));
	}

	PegTestMatch(Variant _var)
	: var(_var)
	{ }

	PegTestMatch(Variant _var, Source reached, PegTestFailures f)
	: var(_var)
	, furthermost_success(reached)
	, failures(f)
	{ }

	bool		is_failure() const
			{ return var.get_type() == Variant::None; }

	Variant		var;

	// Furthermost failure and reasons, only populated on return from parse() (outermost Context)
	Source		furthermost_success;	// Source location of the farthest location the parser reached
	PegTestFailures	failures;	// A Failure for each atom tried at furthermost_success location
};

template<
	typename PegexpT
>
class PegCaptureRule
: public PegRuleNoCapture<PegexpT>
{
public:
	PegCaptureRule(const char* _name, PegexpT _pegexp, const char** _captures)
	: PegRuleNoCapture<PegexpT>(_name, _pegexp)
	, captures(_captures)
	{}

	// Labelled atoms or rules matching these capture names should be returned in the parse match:
	const char**	captures;	// Pointer to zero-terminated array of string pointers.

	bool is_captured(const char* label)	// label maybe not nul-terminated
	{
		if (!captures)
			return false;
		for (int i = 0; captures[i]; i++)
			if (0 == strncmp(captures[i], label, strlen(captures[i])))
				return true;
		return false;
	}
};

/*
 * PegContext saves:
 * - Captured StrVal and Variant StrVariantMap text from the parse
 * - The location beyond which the parse cannot proceed
 * - What tokens would allow it to get further
 */
class	PegContext
{
public:
	using	Source = PegTestSource;
	using	Match = PegTestMatch;
	using	PegT = Peg<Source, Match, PegContext>;
	using	PegexpT = PegPegexp<PegContext>;
	using	Rule = PegCaptureRule<PegexpT>;

	PegContext(PegT* _peg, PegContext* _parent, Rule* _rule, Source _origin)
	: peg(_peg)
	, capture_disabled(_parent ? _parent->capture_disabled : 0)
	, repetition_nesting(0)
	, parent(_parent)
	, rule(_rule)
	, origin(_origin)
	, num_captures(0)
	, furthermost_success(_origin)	// Farthest Source location we reached
	{}

	int		capture(PegexpPC name, int name_len, Match r, bool in_repetition)
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
			ast.put(key, va);	// replace it with this value
			existing = ast[key];
		}
		else	// Insert the match as the first element in an array, or just as itself:
			ast.insert(key, in_repetition ? Variant(&r.var, 1) : r.var);


#ifdef PEG_CAPTURES
		StrVal	rep_display;
		if (existing.get_type() == Variant::VarArray)
		{				// repetition display
			char	buf[16];
			snprintf(buf, sizeof(buf), "[%d]", existing.as_variant_array().length());
			rep_display = StrVal(buf);
		}
		printf(
			"%s%s[%d]: %s%s=%s\n",
			(StrVal("  ")*depth()).asUTF8(),
			rule->name,
			num_captures,
			key.asUTF8(),
			rep_display.asUTF8(),
			r.var.as_json(-2).asUTF8()	// Compact JSON
		);
#endif
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
		if (count == 0)
		{		// We can rollback to zero, but not partway
			ast.clear();
			num_captures = 0;
			return;
		}
		printf("REVISIT: Not rolling back to %d from %d\n", count, num_captures);
	}

	void		record_failure(PegexpPC op, PegexpPC op_end, Source location)
	{
		if (location < furthermost_success)
			return;	// We got further previously

		if (capture_disabled)
			return;	// Failure in lookahead isn't interesting

		// We only need to know about failures of literals, not pegexp operators:
		static	const char*	pegexp_ops = "~@#%_;<`)^$.\\?*+(|&!";
		if (0 != strchr(pegexp_ops, *op))
			return;

		// Record furthermost failure only on the TOP context:
		if (parent)
			return parent->record_failure(op, op_end, location);

		if (furthermost_success < location)
			failures.clear();	// We got further this time, previous failures don't matter

		// Don't double-up failures of the same pegexp against the same text:
		for (int i = 0; i < failures.length(); i++)
			if (failures[i].atom == op)
				return;		// Nothing new here, move along.

		furthermost_success = location;	// We couldn't get past here
		failures.append({op, (int)(op_end-op)});
	}

	Match		match_result(Source from, Source to)
			{
				if (parent == 0)
					return Match(Variant(ast), furthermost_success, failures);
				else if (capture_count() > 0)
					return Match(Variant(ast));
				else
					return Match(from, to);
			}
	Match		match_failure(Source at)
			{ return Match(at, Source()); }

	int		depth()
			{
				return parent ? parent->depth()+1 : 0;
			}

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

	Rule*		rule;		// The Rule this context applies to
	PegT*		peg;		// Place to look up subrules
	PegContext* 	parent;		// Context of the parent (calling) rule
	Source		origin;		// Location where this rule started, for detection of left-recursion

	int		repetition_nesting;	// Number of nested repetition groups, so we know if a capture might be repeated
	int		capture_disabled;	// Counter that bumps up from zero to disable captures

protected:
	int		num_captures;
	StrVariantMap	ast;

	// The next two are populated only on the outermost Context, to be returned from the parse
	Source		furthermost_success;	// Source location of the farthest location the parser reached
	PegTestFailures	failures;	// A Failure for each atom tried at furthermost_success location
};

typedef	Peg<PegTestSource, PegTestMatch, PegContext>	PxParser;

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
			PegTestFailure	f = match.failures[i];
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
