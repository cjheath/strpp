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

void usage()
{
	fprintf(stderr, "Usage: px peg.px\n");
	exit(1);
}

typedef	Peg<PegMemorySource, PegMatch, PegContext>	PxParser;

const char*	TOP_captures[] = { "rule", 0 };
const char*	rule_captures[] = { "name", "alternates", "action", 0 };
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
	{ "space",
	  "|[ \\t\\r\\n]|//*[^\\n]",
	  0
	},
	{ "blankline",
	  "\\n*[ \\t\\r](|\\n|<EOF>)",
	  0
	},
	{ "s",
	  "*(!<blankline><space>)",
	  0
	},
	{ "TOP",
	  "*<space><rule>",
	  TOP_captures
	},
	{ "rule",
	  "<name><s>=<s><alternates>?<action><blankline>*<space>",
	  rule_captures
	},
	{ "action",
	  "-><s>?(<name>:function:\\:<s>)<parameter>*(,<s><parameter>)<s>",
	  action_captures
	},
	{ "parameter",
	  "(|<reference>:parameter:|\\\'<literal>:parameter:\\\')<s>",
	  parameter_captures
	},
	{ "reference",
	  "<name><s>*([.*]:joiner:<s><name>)",
	  reference_captures
	},
	{ "alternates",
	  "|+(\\|<s><sequence>)|<sequence>",
	  alternates_captures
	},
	{ "sequence",
	  "*<repetition>",
	  sequence_captures
	},
	{ "repeat_count",
	  "|[?*+!&]:limit:<s>|<count>:limit:",
	  repeat_count_captures
	},
	{ "count",
	  "\\{(|(+\\d):val:|<name>:val:)<s>\\}<s>",
	  count_captures
	},
	{ "repetition",
	  "?<repeat_count><atom>?<label><s>",
	  repetition_captures
	},
	{ "label",
	  "\\:<name>",
	  label_captures
	},
	{ "atom",
	  "|\\.:any:|<name>:call:|\\\\<property>|\\\'<literal>\\\'|\\[<class>\\]|\\(<group>\\)",
	  atom_captures
	},
	{ "group",
	  "<s>+<alternates>",
	  group_captures
	},
	{ "name",
	  "[\\a_]*[\\w_]",
	  0
	},
	{ "literal",
	  "*(![\']<literal_char>)",
	  0
	},
	{ "literal_char",
	  "|\\\\(|?[0-3][0-7]?[0-7]|x\\h?\\h|x\\{+\\h\\}|u\\h?\\h?\\h?\\h|u\\{+\\h\\}|[^\\n])|[^\\\\\\n]",
	  0
	},
	{ "property",
	  "[adhswLU]",
	  0
	},
	{ "class",
	  "?\\^?-+<class_part>",
	  0
	},
	{ "class_part",
	  "!\\]<class_char>?(-!\\]<class_char>)",
	  0
	},
	{ "class_char",
	  "![-\\]]<literal_char>",
	  0
	}
};

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

PxParser::Match
parse_rule(PxParser::Source source)
{
	PxParser	peg(rules, sizeof(rules)/sizeof(rules[0]));

	return peg.parse(source);
}

/*
 * This function converts the Px text into what Pegexp requires as input (at runtime).
 *
 * Px accepts Unicode characters in literals. Pegexp only accepts printable ASCII.
 * Characters that are not ASCII printable must be converted for Pegexp, into
 * either c-style character escapes, \x or \u escape sequences.
 *
 * Both Px and Pegexp accept \x and \u escapes, with the same syntax, no translation required.
 *
 * Px and Pegexp accept backslashed ASCII printable characters in literals, character classes and (in Px) bare.
 * These sequences should be copied unchanged.
 *
 * A number of characters which don't require a backslash in Px do require them in Pegexp,
 * unless inside a character class. Add those here.
 *
 * A code generator requires a subsequent transformation to a source-code form.
 */
StrVal generate_literal(StrVal literal, bool leave_specials = false)
{
	static	auto	c_escaped = "\n\t\r\b\f";	// C also defines \a, \v but they're not well-known
	static	auto	c_esc_chars = "ntrbf";		// Match the character positions
	static	auto	hex = "0123456789ABCDEF";

	// printf("generate_literal from %s", literal.asUTF8());
	// Escape special characters:
	literal.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4    ch = UTF8Get(cp);       // Get UCS4 character

			if (ch == '\\')
			{
				ch = UTF8Get(cp);
				if (UCS4IsASCIIPrintable(ch))
					return StrVal("\\")+ch;	// Backslashed printable
				// Otherwise the backslash may be ignored.
			}
			
			if (!UCS4IsASCIIPrintable(ch))
			{		// Convert to c-escape, \x or \u sequence
				const char*	sp;

				// Translate C special escapes
				if (ch < ' ' && (sp = strchr(c_escaped, (char)ch)))
					return StrVal("\\") + (UCS4)c_esc_chars[sp-c_escaped];

				// Latin1 can be a \xHH escape
				if (UCS4IsLatin1(ch))
					return StrVal("\\x") + (UCS4)hex[(ch>>4)&0xF] + (UCS4)hex[ch&0xF];

				static	char	ubuf[20];
				auto	cp = ubuf;
				*cp++ = '\\';
				*cp++ = 'u';
				if (UCS4IsUTF16(ch))
				{		// 4-byte Unicode escape without braces
					*cp++ = hex[(ch>>12)&0xF];
					*cp++ = hex[(ch>>8)&0xF];
					*cp++ = hex[(ch>>4)&0xF];
					*cp++ = hex[ch&0xF];
					*cp = '\0';
					return ubuf;
				}
				// Unicode with braces. Sorry for printf, but StrVal::format doesn't exist yet
				snprintf(cp, sizeof(ubuf)-(cp-ubuf), "%lX}", (long)(ch & 0xFFFFFFFF));
				return ubuf;
			}

			if (!leave_specials				// Don't escape specials inside a char class
			 && ch < 0x7F
                         && 0 != strchr(PxParser::PegexpT::special, (char)ch))	// Char is special to Pegexp
				return StrVal("\\")+ch;

			// otherwise no backslash is needed
			return ch;
		}
	);
	// printf(" to %s\n", literal.asUTF8());

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
			return StrVal("[") + generate_literal(element.as_strval(), true) + "]";
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
	// printf("Pegexp:\t %s\n", re.asUTF8());
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
		"typedef\tPeg<PegMemorySource, PegMatch, PegContext>\t%sParser;\n\n"
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
