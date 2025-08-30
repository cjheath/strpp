/*
 * Px PEG parser generator defined using pegular expression rules
 *
 * Copyright 2025 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<refcount.h>
#include	<strval.h>
#include	<variant.h>

#include	<peg.h>
#include	<peg_ast.h>

#include	<cstdio>
#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>

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
	{ "EOF",
	  "!.",
	  0
	},
	{ "space",				// Any single whitespace
	  "|[ \\t\\r\\n]|//*[^\\n]",		// including a comment to end-of-line
	  0
	},
	{ "blankline",				// A line containing no printing characters
	  "\\n*[ \\t\\r](|\\n|<EOF>)",
	  0
	},
	{ "s",					// Any whitespace but not a blankline
	  "*(!<blankline><space>)",
	  0
	},
	{ "TOP",				// Start; a repetition of zero or more rules
	  // "*<space>*<rule>",
	  "*<space><rule>",			// Parse one rule at a time
	  TOP_captures
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
	  "|[?*+!&]:limit<s>"			// zero or one, zero or more, one or more, none, and
	  "|<count>:limit",
	  repeat_count_captures
	},
	{ "count",
	  "\\{(|(+\\d):val|<name>:val)<s>}",	// {literal count or a reference to a captured variable}
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
	  "'*(!'<literal_char>)'",
	  0
	},
	{ "literal_char",
	  "|\\\\(|?[0-3][0-7]?[0-7]"		// octal character constant
	      "|x\\h?\\h"			// hexadecimal constant \x12
	      "|x{+\\h}"			// hexadecimal constant \x{...}
	      "|u?[01]\\h?\\h?\\h?\\h"		// Unicode character \u1234
	      "|u{+\\h}"			// Unicode character \u{...}
	      "|[^\\n]"				// Other special escape except newline
	     ")"
	  "|[^\\\\\\n]",			// any normal character except backslash or newline
	  0
	},
	{ "property",				// alpha, digit, hexadecimal, whitespace, word (alpha or digit), lower, upper
	  "\\\\[adhswLU]",
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
	  "![-\\]]<literal_char>",
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
	if (re.type() == Variant::StrVarMap)
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
				if (repeat_count.type() != Variant::None)
					ret += repeat_count.as_variant_map()["limit"].as_strval();
				ret += generate_re(atom);
				Variant		label = repetition["label"];	// Maybe None
				if (label.type() != Variant::None)
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
			if (element.type() == Variant::String)
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
			else if (element.type() == Variant::StrVarMap)
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
	else if (re.type() == Variant::VarArray)
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

	if (name_v.type() == Variant::String)
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
	if (parameters.type() == Variant::VarArray)
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

void emit_rule(
	Variant		_rule,
	StrVal&		capture_arrays,
	StrVal&		rules
)
{
	// printf("Parse Tree:\n%s\n", _rule.as_json(-2).asUTF8());

	StrVariantMap	rule = _rule.as_variant_map()["rule"].as_variant_map();
	Variant		vr = rule["name"];
	Variant		va = rule["alternates"]; // Variant(StrVariantMap)
	Variant		vact = rule["action"];

	// Append the capture array for this rule:
	capture_arrays += StrVal("const char*\t")+vr.as_strval()+"_captures[] = ";
	if (vact.type() != Variant::None)
	{
		StrVal		parameters = generate_parameters(vact.as_variant_map()["parameter"]);
		capture_arrays += parameters.asUTF8();
	}
	else
		capture_arrays += "{}";
	capture_arrays += ";";
	// The Parser doesn't yet use the function, if any:
	if (!vact.is_null())
	{
		Variant	function = vact.as_variant_map()["name"];	// StrVal or None
		if (!function.is_null())
			capture_arrays += StrVal("\t\t// ")+function.as_strval();
	}
	capture_arrays += "\n";

	// Generate the pegular expression for this rule:
	StrVal		re = generate_re(va);
	re.transform(		// Double each backslash for the C++ compiler to revert
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4    ch = UTF8Get(cp);       // Get UCS4 character
			if (ch == '\\')
				return "\\\\";
			return ch;
		}
	);

	rules += StrVal("\t{ \"")
		+ vr.as_strval()
		+ "\",\n\t  \""
		+ re
		+ "\",\n\t  "+vr.as_strval()+"_captures\n\t}";
}

typedef	Array<StrVal>	StringArray;
typedef	CowMap<bool> 	StringSet;

/*
 * Descend into the AST of a regular expression, saving any new rule names that are called
 */
void accumulate_called_rules(StringSet& called, Variant re)
{
	if (re.type() == Variant::StrVarMap)
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
	else if (re.type() == Variant::VarArray)
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

void emit(const char* parser_name, VariantArray rules)
{
	StrVal	capture_arrays;
	StrVal	rules_text;

	for (int i = 0; i < rules.length(); i++)
	{
		if (i)
			rules_text += ",";
		rules_text += "\n";
		emit_rule(rules[i], capture_arrays, rules_text);
	}

	printf(
		"typedef Peg<PegMemorySource, PegMatch, PegContext>      %sParser;\n\n"
		"%s\n"
		"%sParser::Rule\trules[] =\n{"
		"%s\n"
		"};\n",
		parser_name,
		capture_arrays.asUTF8(),
		parser_name,
		rules_text.asUTF8()
	);
}

bool
parse_and_emit(const char* filename, VariantArray& rules)
{
	off_t		file_size;
	char*		text = slurp_file(filename, &file_size);
	char*		basename;
	PxParser::Source source(text);
	int		bytes_parsed = 0;
	int		rules_parsed = 0;

	// Figure out the basename, isolate and null-terminate it:
	if ((basename = (char*)strrchr(filename, '/')) != 0)
		basename++;
	else
		basename = (char*)filename;
	basename = strdup(basename);
	if (strchr(basename, '.'))
		*strchr(basename, '.') = '\0';
	*basename = toupper(*basename);

	do {
		PxParser::Match match = parse_rule(source);

		if (match.is_failure())
		{
			printf("Parse failed at line %lld column %lld (byte %lld of %d) after %d rules. Possible next %d tokens were:\n",
				source.current_line()+match.furthermost_success.current_line()-1,
				source.current_column()+match.furthermost_success.current_column()-1,
				source.current_byte()+match.furthermost_success.current_byte(),
				(int)file_size,
				rules_parsed,
				match.failures.length()
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

	// printf("Parsed %d bytes of %d\n", bytes_parsed, (int)file_size);

	delete text;

	bool	success = bytes_parsed == file_size;

	if (!success)
	{
		rules.clear();
		return false;
	}

	if (!check_rules(rules))
		return false;

	emit(basename, rules);

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
