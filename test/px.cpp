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
const char*	atom_captures[] = { "any", "call", "property", "literal", "class", "group", 0 };
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
	  "(|<reference>:parameter|'<literal>:parameter')<s>",
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
	  "\\{(|(+\\d):val|<name>:val)<s>}",	// {numeric count or a reference to a captured variable}
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
	  "|\\.:any"				// Any character
	  "|<name>:call"			// call to another rule
	  "|\\\\<property>"			// A character by property
	  "|'<literal>'"			// A literal string
	  "|\\[<class>\\]"			// A character class
	  "|\\(<group>\\)",			// A parenthesized group
	  atom_captures
	},
	{ "group",
	  "<s>+<alternates>",			// Contents of a parenthesised group
	  group_captures
	},
	{ "name",
	  "[\\a_]*[\\w_]",
	  0
	},
	{ "literal",				// Contents of a literal string
	  "*(!'<literal_char>)",
	  0
	},
	{ "literal_char",
	  "|\\\\(|?[0-3][0-7]?[0-7]"		// octal character constant
	      "|x\\h?\\h"			// hexadecimal constant \x12
	      "|x{+\\h}"			// hexadecimal constant \x{...}
	      "|u\\h?\\h?\\h?\\h"		// Unicode character \u1234
	      "|u{+\\h}"			// Unicode character \u{...}
	      "|[^\\n]"				// Other special escape except newline
	     ")"
	  "|[^\\\\\\n]",			// any normal character except backslash or newline
	  0
	},
	{ "property",				// Contents of a property
	  "[adhswLU]",
	  0
	},
	{ "class",				// Contents of a character class
	  "?\\^?-+<class_part>",
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

// Turn the characters that make up a Px literal string into the encoded characters they represent:
StrVal generate_literal(StrVal literal)
{
	// Escape special characters:
	literal.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4    ch = UTF8Get(cp);       // Get UCS4 character

			if (ch == '\\')
			{
				int	digit;
				UCS4	out = 0;
				bool	curly;
				int	max_digits;

				// Turn Px escaped chars into raw chars
				ch = UTF8Get(cp);
				switch (ch)
				{
				case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7':
					out = UCS4Digit(ch);		// First Octal digit. Accept 2 more.
					for (int i = 0; i < 2; i++)
					{
						ch = UTF8Peek(cp);
						if ((digit = UCS4Digit(ch)) < 0 || digit > 7)
							break;
						out = (out<<3) + digit;
						UTF8Get(cp);
					}
					ch = out;
					break;

				case 'x':			// 2-digit hexadecimal
					max_digits = 2;
					goto read_hex;

				case 'u':			// 2-digit hexadecimal
					max_digits = 4;
					if (UTF8Peek(cp) == '{')
						max_digits = 5;
			read_hex:
					ch = UTF8Peek(cp);
					curly = ch == '{';
					if (curly)
						ch = UTF8Get(cp);

					for (int i = 0; i < max_digits; i++)
					{
						ch = UTF8Get(cp);
						if ((digit = UCS4HexDigit(ch)) < 0)
							break;
						out = (out << 4) + digit;
					}
					if (curly && ch != '}' && UTF8Peek(cp) == '}')
						UTF8Get(cp);	// Eat the anticipated closing curly, don't complain otherwise
					ch = out;
					break;

				// C special character escapes:
				case 'n': ch = '\n'; break;	// newline
				case 't': ch = '\t'; break;	// tab
				case 'r': ch = '\r'; break;	// return
				case 'b': ch = '\b'; break;	// backspace
				case 'e': ch = '\033'; break;	// escape
				case 'f': ch = '\f'; break;	// formfeed

				default:	// Backslashed ordinary character, keep the backslash
					return StrVal("\\")+ch;
				}
				return ch;
			}

			if (ch < 0x7F
                         && 0 != strchr(PxParser::PegexpT::special, (char)ch))
				return StrVal("\\")+ch;

			// otherwise no backslash is needed
			return ch;
		}
	);

	return literal;
}

/*
 * For the Variant node passed, generate the Pegexp expression which corresponds
 *
 * Conversion into a valid literal for e.g. C++ will happen later, this function returns the value which should be used at runtime.
 */
StrVal generate_pegexp(Variant re)
{
	if (re.type() == Variant::StrVarMap)
	{
		StrVariantMap	map = re.as_variant_map();
		assert(map.size() == 1);

		auto		entry = map.begin();
		StrVal		node_type = entry->first;
		Variant		element = entry->second;
		if (node_type == "sequence")
		{
			return generate_pegexp(element);
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
				ret += generate_pegexp(atom);
				Variant		label = repetition["label"];	// Maybe None
				if (label.type() != Variant::None)
					ret += StrVal(":")+label.as_variant_map()["name"].as_strval()+":";
			}
			return ret;
		}
		else if (node_type == "any")
		{
			return StrVal(".");
		}
		else if (node_type == "call")
		{		// name of a rule to be called
			return StrVal("<")+element.as_strval()+">";
		}
		else if (node_type == "property")
		{		// a property character, just backslash it:
			return StrVal("\\")+element.as_strval();
		}
		else if (node_type == "literal")
		{		// body of a literal string
			return generate_literal(element.as_strval());
		}
		else if (node_type == "class")
		{		// body of a character class (backslashed properties keep their backslash)
			return StrVal("[") + generate_literal(element.as_strval()) + "]";
		}
		else if (node_type == "group")
		{
			VariantArray	alternates = element.as_variant_map()["alternates"].as_variant_array();
			if (alternates.length() != 1)
			{
				// This happens e.g. on (a|b) when it should be (|a|b)
				assert(!"REVISIT: unexpected alternates count");
			}
			return StrVal("(") + generate_pegexp(alternates[0]) + ")";
		}
		else	// Report incomplete node type:
			return StrVal("INCOMPLETE<") + node_type + ">=" + element.as_json(-2);
	}
	else if (re.type() == Variant::VarArray)
	{
		VariantArray	va = re.as_variant_array();
		StrVal	ret;
		for (int i = 0; i < va.length(); i++)
			ret += StrVal("|")+generate_pegexp(va[i]);
		return ret;
	}
	else
	{
		// This indicates the code above is incomplete. Dump the unhandled structure:
		return StrVal("INCOMPLETE CODE for ") + re.type_name() + "=" + re.as_json(-2);
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
	StrVal	p;
	StrVal	dquot("\"");

	// parameters might be an individual StrVariantMap or an array of them?
	if (parameters.type() == Variant::VarArray)
	{	// Extract the "parameter" value from each element
		VariantArray	a = parameters.as_variant_array();
		if (a.length() == 0)
			return "";	// There are no captured parameters

		for (int i = 0; i < a.length(); i++)
		{
			Variant	pn = a[i].as_variant_map()["parameter"];
			p += generate_parameter(pn) + ", ";
		}
	}
	else	// type is Variant::StrVarMap
	{
		// E.g. foo.bar*baz yields:
		// { "parameter": { "joiner": [ ".", "*" ], "name": [ "foo", "bar", "baz" ] } }
		p += generate_parameter(parameters.as_variant_map()["parameter"]) + ", ";
	}
	return p;
}

StrVal transform_literal_to_cpp(StrVal str)
{
	static	auto	c_escapes = "\\\n\t\r\b\f\"\'";	// C also defines \a, \v but they're not well-known
	static	auto	c_esc_chars = "\\ntrbf\"\'";	// Match the character positions
	static	auto	hex = "0123456789ABCDEF";
	static	char	ubuf[20];
	auto	u4 = [&](char* cp, UTF16 ch) -> void
	{
		strcpy(cp, "\\\\u{");
		cp += 4;
		*cp++ = hex[(ch>>12)&0xF];
		*cp++ = hex[(ch>>8)&0xF];
		*cp++ = hex[(ch>>4)&0xF];
		*cp++ = hex[ch&0xF];
		*cp++ = '}';
		*cp = '\0';
	};

	/*
	 * Double each backslash for the C++ compiler to revert.
	 * Convert Unicode characters to \\u1234 (or a surrogate pair)
	 * Convert \n \t \r \b \e \f to the backslashed form
	 */
	str.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4    ch = UTF8Get(cp);       // Get UCS4 character
			if (ch > 0xF7)
			{
				if (!UCS4IsUnicode(ch))
					ch = UCS4_REPLACEMENT;	// Substitute, we can't put this into UTF16

				if (UCS4IsUTF16(ch))
				{	// Return single-char UTF16 
					// if (UTF16IsSurrogate((UTF16)ch)) ERROR;
					u4(ubuf, ch);
					return ubuf;
				}

				// REVISIT: It would also be possible to use \U12345678 (since which C standard though?)
				// Return a UTF-16 surrogate pair
				u4(ubuf, UCS4HighSurrogate(ch));
				u4(ubuf+9, UCS4LowSurrogate(ch));
				return ubuf;
			}

			// Check for C backslash escapes:
			const char*	sp = strchr(c_escapes, (char)ch);
			if (sp)				// C special escapes
			{
				int c = c_esc_chars[sp-c_escapes];
				return StrVal("\\")+StrVal(c);
			}
			if (ch >= ' ' && ch < 0x7F)
				return ch;		// Safe ASCII

			// return a \xHH hex character
			ubuf[0] = '\\';
			ubuf[1] = '\\';
			ubuf[2] = 'x';
			ubuf[3] = hex[(ch>>4)&0xF];
			ubuf[4] = hex[ch&0xF];
			ubuf[5] = '\0';
			return ubuf;
		}
	);
	// printf("Cooked pegular-expression: '%s'\n", str.asUTF8());
	return str;
}

void emit_rule_cpp(
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
	StrVal		parameters;
	if (vact.type() != Variant::None)
		parameters = generate_parameters(vact.as_variant_map()["parameter"]);
	if (parameters != "")
	{
		capture_arrays +=
			StrVal("const char*\t")
			+ vr.as_strval()
			+ "_captures[] = { "
			+ parameters
			+ "0 };\n";
	}

	// The Parser doesn't yet use the function, if any:
	if (!vact.is_null())
	{
		Variant	function = vact.as_variant_map()["name"];	// StrVal or None
		if (!function.is_null())
			capture_arrays += StrVal("\t\t// FUNCTION: ")+function.as_strval()+"\n";
	}

	// Generate the pegular expression for this rule:
	StrVal		re = generate_pegexp(va);
	// printf("Pegexp: '%s'\n", re.asUTF8());
	StrVal		re_cpp = transform_literal_to_cpp(re);

	rules += StrVal("\t{ \"")
		+ vr.as_strval()
		+ "\",\n\t  \""
		+ re_cpp
		+ "\",\n\t  "
		+ (parameters != "" ? vr.as_strval()+"_captures" : "0")
		+ "\n\t}";
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
		{
			VariantArray	repetitions = element.as_variant_array();

			for (int i = 0; i < repetitions.length(); i++)
			{
				StrVariantMap	repetition = repetitions[i].as_variant_map();
				Variant		atom = repetition["atom"];
				accumulate_called_rules(called, atom);
			}
		}
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

void emit_cpp(const char* parser_name, VariantArray rules)
{
	StrVal	capture_arrays;
	StrVal	rules_text;

	for (int i = 0; i < rules.length(); i++)
	{
		if (i)
			rules_text += ",";
		rules_text += "\n";
		emit_rule_cpp(rules[i], capture_arrays, rules_text);
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

	emit_cpp(basename, rules);

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
