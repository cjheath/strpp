/*
 * Px PEG parser generator defined using pegular expression rules
 *
 * Copyright 2025 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
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

using	Source = GuardedUTF8Ptr;
using	PegMemorySourceSup = PegexpPointerSource<GuardedUTF8Ptr>;

// We need some help to extract captured data from PegexpPointerSource:
class PegMemorySource : public PegexpPointerSource<GuardedUTF8Ptr>
{
	using PegMemorySourceSup = PegexpPointerSource<GuardedUTF8Ptr>;
public:
	PegMemorySource() : PegMemorySourceSup() {}
	PegMemorySource(const char* cp) : PegMemorySourceSup(cp) {}
	PegMemorySource(const PegMemorySource& pi) : PegMemorySourceSup(pi) {}

	const UTF8*	peek() const { return data; }
};

class PegFailure {
public:
	PegexpPC	atom;		// Start of the Pegexp literal we failed to match
	int		atom_len;	// Length of the literal (for reporting)
};
// A Failure for each atom tried at furthermost_success location
typedef Array<PegFailure>	PegFailures;

class PegMatch
{
public:
	using Source = PegMemorySource;

	PegMatch()
	{}

	// Capture matched text:
	PegMatch(Source from, Source to)
	{
		if (to.is_null())
			var = Variant();	// type=None -> failure
		else
			var = StrVal(from.peek(), (int)(to - from));
	}

	PegMatch(Variant _var)
	: var(_var)
	{ }

	// Final result constructor with termination point:
	PegMatch(Variant _var, Source reached, PegFailures f)
	: var(_var)
	, furthermost_success(reached)
	, failures(f)
	{ }

	bool		is_failure() const
			{ return var.get_type() == Variant::None; }

	Variant		var;

	// Furthermost failure and reasons, only populated on return from parse() (outermost Context)
	Source		furthermost_success;
	PegFailures	failures;
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

	bool is_captured(const char* label)	// label maybe not nul-terminated!
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
	using	Source = PegMemorySource;
	using	Match = PegMatch;
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
		Variant		value(r.var);
		Variant		existing;

		if (value.get_type() == Variant::String && value.as_strval().length() == 0)
			return num_captures;

		// If this rule captures one item only, collapse the AST:
		if (rule->captures && rule->captures[0] && rule->captures[1] == 0)
		{	// Only one thing is being captured here. Don't nest it, return it.
#if 0
			if (value.get_type() == Variant::StrVarMap
			 && value.as_variant_map().size() == 1)
			{	// This map has only one entry. Return that
				auto	entry = value.as_variant_map().begin();
				printf(
					"eliding %s in favour of %s\n",
					key.asUTF8(),
					StrVal(entry->first).asUTF8()
				);
				value = entry->second;
				key = entry->first;

#ifdef FLATTEN_ARRAYS
				if (value.get_type() == Variant::VarArray
				 && value.as_variant_array().length() == 1)
				{
					printf("flattening array\n");
					value = value.as_variant_array()[0];
				}
#endif

			}
#endif
		}

		if (ast.contains(key))
		{		// There are previous captures under this name
			existing = ast[key];
			if (existing.get_type() != Variant::VarArray)
				existing = Variant(&existing, 1);	// Convert it to an array
			VariantArray va = existing.as_variant_array();
			va += value;		// This Unshares va from the entry stored in the map, so
			ast.put(key, va);	// replace it with this value
			existing = ast[key];
		}
		else	// Insert the match as the first element in an array, or just as itself:
			ast.insert(key, in_repetition ? Variant(&value, 1) : value);

		num_captures++;
		return 0;
	}

	int		capture_count() const
	{
		return num_captures;
	}

	// This grammar should not capture anything that rolls back except to zero, unless on failure:
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
			return Match(ast);
		else
			return Match(from, to);
	}
	Match		match_failure(Source at)
	{ return Match(at, Source()); }

#if 0
	int		depth()
	{ return parent ? parent->depth()+1 : 0; }

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
#endif

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
	PegFailures	failures;	// A Failure for each atom tried at furthermost_success location
};

typedef	Peg<PegMemorySource, PegMatch, PegContext>	PxParser;

void usage()
{
	fprintf(stderr, "Usage: px peg.px\n");
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
	  // "*<space>*<rule>:rule",
	  "*<space><rule>:rule",		// Parse one rule at a time
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
parse_rule(PxParser::Source source)
{
	PxParser	peg(rules, sizeof(rules)/sizeof(rules[0]));

	return peg.parse(source);
}

StrVal generate_literal(StrVal literal)
{
	StrVal	s = literal.substr(1, literal.length()-2);

	// Escape special characters:
	s.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4    ch = UTF8Get(cp);       // Get UCS4 character

			// If the String is Unicode or a control-char, emit \u{...}
			if (ch > 0xFF || ch < ' ')
			{
				StrVal	u("\\u{");
				while (ch)
				{
					u.insert(3, "0123456789ABCDEF"[ch&0xF]);
					ch >>= 4;
				}
				u += "}";
				return u;
			}

			// If char is 8-bit but not special, return it unmolested:
			if (ch >= ' '
			 && 0 == strchr(PxParser::PegexpT::special, (char)ch))
				return StrVal(ch);

			// Backslash the string
			return StrVal("\\")+ch;
		}
	);

	return s;
}

StrVal generate_re(Variant re)
{
	if (re.get_type() == Variant::StrVarMap)
	{
		assert(re.as_variant_map().size() == 1);

		StrVariantMap	map = re.as_variant_map();
		auto		entry = map.begin();
		StrVal		node_type = entry->first;
		Variant		element = entry->second;
		if (node_type == "sequence")
		{
			return generate_re(element);
		}
		else if (node_type == "repetition")
		{
			// Each repetition has {optional repeat_count, atom, optional label}
			VariantArray	repetitions = element.as_variant_array();

			StrVal	ret;
			for (int i = 0; i < repetitions.length(); i++)
			{
				StrVariantMap	repetition = repetitions[i].as_variant_map();
				Variant		atom = repetition["atom"];
				Variant		repeat_count = repetition["repeat_count"]; // Maybe None
				if (repeat_count.get_type() != Variant::None)
					ret += repeat_count.as_variant_map()["limit"].as_strval();
				ret += generate_re(atom);
				Variant		label = repetition["label"];	// Maybe None
				if (label.get_type() != Variant::None)
					ret += StrVal(":")+label.as_variant_map()["name"].as_strval()+":";
			}
			return ret;
		}
		else if (node_type == "name")
		{
			return StrVal("<")+element.as_strval()+">";
		}
		else if (node_type == "class")
		{
			return element.as_strval();
		}
		else if (node_type == "group")
		{
			return StrVal("(")+generate_re(element)+")";
		}
		else if (node_type == "alternates")
		{
			VariantArray	alternates = element.as_variant_array();
			if (alternates.length() == 1)
				return generate_re(alternates[0]);

			return	"REVISIT: unexpected alternates count";
		}
		else if (node_type == "literal")
		{
			return generate_literal(element.as_strval());
		}
		else if (node_type == "property")
		{
			return element.as_strval();
		}
		else if (node_type == "atom")
		{
			// atom can be:
			// . (any character)
			// 'string' (literal)
			// name (subroutine call)
			// \p Property
			// [class] a character class
			// a map containing an array of atoms in a group
			if (element.get_type() == Variant::String)
			{
				StrVal	s = element.as_strval();
				switch (s[0])
				{
				case '\'':
					return generate_literal(s);
				case '.': case '[': case '\\':
					return s;
				default:
					return StrVal('<')+element.as_strval()+">";
				}
			}
			else if (element.get_type() == Variant::StrVarMap)
				return StrVal("(")+generate_re(element)+")";
			else
				assert(!"Unexpected atom type");
		}
	/* REVISIT: Complete these
		else if (node_type == "param")
		{
		}
	*/
		else	// Report incomplete node type:
			return StrVal("INCOMPLETE<") + node_type + ">=" + element.as_json(-2);
	}
	else if (re.get_type() == Variant::VarArray)
	{
		VariantArray	va = re.as_variant_array();
		StrVal	ret;
		for (int i = 0; i < va.length(); i++)
			ret += StrVal("|")+generate_re(va[i]);
		return ret;
	}
	else
	{
		// This indicates the code above is incomplete. Dump the unhandled structure:
		return StrVal(re.type_name()) + "=" + re.as_json(-2);
	}
}

StrVal
generate_parameter(Variant parameter_map)
{
	StrVal		dquot("\"");
	Variant		name_v = parameter_map.as_variant_map()["name"];

	if (name_v.get_type() == Variant::String)
		return dquot+name_v.as_strval()+dquot;

	VariantArray	joiners = parameter_map.as_variant_map()["joiner"].as_variant_array();
	VariantArray	names = name_v.as_variant_array();
	StrVal		p(dquot);
	for (int i = 0; i < names.length(); i++)
	{
		if (i)
			p += joiners[i-1].as_strval();
		p += names[i].as_strval();
	}
	p += dquot;
	return p;
}

StrVal
generate_parameters(Variant parameters)
{
	StrVal	p("{ ");
	StrVal	dquot("\"");

	// parameters might be an individual StrVariantMap or an array of them?
	if (parameters.get_type() == Variant::VarArray)
	{	// Extract the "parameter" value from each element
		VariantArray	a = parameters.as_variant_array();
		for (int i = 0; i < a.length(); i++)
		{
			if (i)
				p += ", ";
			Variant	pn = a[i].as_variant_map()["parameter"];
			p += generate_parameter(pn);
		}
	}
	else	// type is Variant::StrVarMap
	{
		// E.g. foo.bar*baz yields:
		// { "parameter": { "joiner": [ ".", "*" ], "name": [ "foo", "bar", "baz" ] } }
		p += generate_parameter(parameters.as_variant_map()["parameter"]);
	}
	p += " }";
	return p;
}

void emit_rule(Variant _rule)
{
	// printf("Parse Tree:\n%s\n", _rule.as_json(-2).asUTF8());

	StrVariantMap	rule = _rule.as_variant_map()["rule"].as_variant_map();
	Variant		vr = rule["name"];
	Variant		va = rule["alternates"]; // Variant(StrVariantMap)
	Variant		vact = rule["action"];

	// Generate the pegular expression for this rule:
	StrVal		re = generate_re(va);

	printf("Rule: %s =\n\t%s\n", vr.as_strval().asUTF8(), re.asUTF8());

	if (vact.get_type() != Variant::None)
	{
		// REVISIT: parameters are requested as "list" but saved as "parameter" - why?
		StrVal		parameters = generate_parameters(vact.as_variant_map()["parameter"]);
		Variant		function = vact.as_variant_map()["name"];	// StrVal or None

		printf(
			"\t-> %s%s\n",
			function.is_null() ? "" : (function.as_strval()+": ").asUTF8(),
			parameters.asUTF8()
		);
	}
}

typedef	Array<StrVal>	StringArray;
typedef	CowMap<bool> 	StringSet;

/*
 * Descend into the AST of a regular expression, saving any new rule names that are called
 */
void accumulate_called_rules(StringSet& called, Variant re)
{
	if (re.get_type() == Variant::StrVarMap)
	{
		assert(re.as_variant_map().size() == 1);	// There is only one entry in the map
		StrVariantMap	map = re.as_variant_map();
		auto		entry = map.begin();
		StrVal		node_type = entry->first;	// Extract the node_type and value
		Variant		element = entry->second;

		if (node_type == "name")
			called.put(element.as_strval(), true);
		else if (node_type == "sequence")	// Process the VarArray
			accumulate_called_rules(called, element);
		else if (node_type == "group")
			return accumulate_called_rules(called, element);
		else if (node_type == "alternates")
			generate_re(element.as_variant_array()[0]);
		else if (node_type == "repetition")
		{		// Each repetition has {optional repeat_count, atom, optional label}
			VariantArray	repetitions = element.as_variant_array();

			for (int i = 0; i < repetitions.length(); i++)
			{
				StrVariantMap	repetition = repetitions[i].as_variant_map();
				Variant		atom = repetition["atom"];
				accumulate_called_rules(called, atom);
			}
		}
	}
	else if (re.get_type() == Variant::VarArray)
	{
		VariantArray	va = re.as_variant_array();
		for (int i = 0; i < va.length(); i++)
			accumulate_called_rules(called, va[i]);
	}
}

bool check_rules(VariantArray rules)
{
	StringSet	defined_rules;
	StringSet	called_rules;

	// Find names of all rules that are called:
	for (int i = 0; i < rules.length(); i++)
	{
		StrVariantMap	rule = rules[i].as_variant_map()["rule"].as_variant_map();
		StrVal		rule_name = rule["name"].as_strval();
		Variant		va = rule["alternates"];	// Definition of the rule's RE

		// record that this rule is defined:
		defined_rules.put(rule_name, true);

#if 0
	// REVISIT: Ensure that actions only request available captures
		// record which rules this rule calls and what labels it has, for checking actions:
		StringSet	local_called_rules;
		accumulate_called_rules(local_called_rules, va);
		Variant		vact = rule["action"];		// Ensure that capture names are defined
#endif

		// accumulate all called rules:
		accumulate_called_rules(called_rules, va);
	}

	// Ensure that all called rules exist:
	bool ok = true;
	for (auto i = called_rules.begin(); i != called_rules.end(); i++)
	{
		if (!defined_rules[i->first])
		{
			ok = false;
			printf("Rule %s is called but not defined\n", StrVal(i->first).asUTF8());
		}
	}

	return ok;
}

void emit(VariantArray rules)
{
	for (int i = 0; i < rules.length(); i++)
		emit_rule(rules[i]);
}

bool
parse_and_emit(const char* filename, VariantArray& rules)
{
	off_t		file_size;
	char*		text = slurp_file(filename, &file_size);
	PxParser::Source source(text);
	int		bytes_parsed = 0;
	int		rules_parsed = 0;

	do {
		PxParser::Match match = parse_rule(source);

		if (match.is_failure())
		{
			printf("Parse failed at line %lld column %lld (byte %lld of %d) after %d rules. Next tokens anticipated were:\n",
				source.current_line()+match.furthermost_success.current_line()-1,
				source.current_column()+match.furthermost_success.current_column()-1,
				source.current_byte()+match.furthermost_success.current_byte(),
				(int)file_size,
				rules_parsed
			);

			for (int i = 0; i < match.failures.length(); i++)
			{
				PegFailure	f = match.failures[i];
				printf("\t%.*s\n", f.atom_len, f.atom);
			}
			break;
		}

		bytes_parsed = match.furthermost_success.peek() - text;

		// printf("===== Rule %d:\n", rules_parsed+1);
		// printf("Parse Tree:\n%s\n", match.var.as_json(0).asUTF8());

		rules.append(match.var);

		// Start again at the next rule:
		source = match.furthermost_success;
		rules_parsed++;
	} while (bytes_parsed < file_size);

	printf("Parsed %d bytes of %d\n", bytes_parsed, (int)file_size);

	delete text;

	bool	success = bytes_parsed == file_size;

	if (!success)
	{
		rules.clear();
		return false;
	}

	if (!check_rules(rules))
		return false;

	emit(rules);

	return true;
}

int
main(int argc, const char** argv)
{
	if (argc < 2)
		usage();

	/*
	start_recording_allocations();
	*/

	VariantArray	rules;
	if (!parse_and_emit(argv[1], rules))
		exit(1);

	/*
	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();
	*/

	return 1;
}
