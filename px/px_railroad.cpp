/*
 * Javascript/SVG generator for railroad diagrams for a parser
 */
#include	<px_parser.h>

#include	<strval.h>
#include	<variant.h>
#include	<char_encoding.h>

#include	<cstdio>
#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>

#include	<px_pegexp.h>
#include	<px_railroad.h>

/*
 * This function converts Px literal text into a string to display in a Railroad Terminal node.
 * A subsequent function should escape characters as needed, so Javascript passes them.
 */
StrVal generate_railroad_literal(StrVal literal, bool leave_specials = false)
{
	static	auto	c_escaped = "\n\t\r\b\f";	// C also defines \a, \v but they're not well-known
	static	auto	c_esc_chars = "ntrbf";		// Match the character positions
	static	auto	hex = "0123456789ABCDEF";

	// printf("generate_railroad_literal from %s", literal.asUTF8());
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

/*
			if (!leave_specials				// Don't escape specials inside a char class
			 && ch < 0x7F
                         && 0 != strchr(PxParser::PegexpT::special, (char)ch))	// Char is special to Pegexp
				return StrVal("\\")+ch;
*/

			// otherwise no backslash is needed
			return ch;
		}
	);
	// printf(" to %s\n", literal.asUTF8());

	return literal;
}

// a repetition operator applies to this atom. Ensure it is wrapped in () if necessary
bool railroad_is_single_atom(Variant atom)
{
	if (atom.type() == Variant::StrVarMap)
	{
		auto	element = atom.as_variant_map().begin();
		StrVal	map_content_type = element->first;
		if (map_content_type == "literal")
			return element->second.as_strval().length() <= 1;
		return map_content_type != "sequence";
	}
	else if (atom.type() == Variant::VarArray)
	{
		return false;
	}
	return true;
}

/*
 * For the Variant node passed, generate the railroad.js expression which corresponds
 */
StrVal generate_railroad(Variant re)
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
			return StrVal("Sequence(") + generate_railroad(element) + ")";
		}
		else if (node_type == "repetition")
		{		// Each repetition has {optional repeat_count, atom, optional label}
			VariantArray	repetitions = element.as_variant_array();

			// Go through each item in the sequence:
			StrVal	ret;
			for (int i = 0; i < repetitions.length(); i++)
			{
				StrVariantMap	repetition = repetitions[i].as_variant_map();
				Variant		atom = repetition["atom"];
				Variant		repeat_count = repetition["repeat_count"]; // Maybe None
				bool		repeating = repeat_count.type() != Variant::None;

				StrVal rep;
				if (repeating)
				{
					UCS4 repeat_op = repeat_count.as_variant_map()["limit"].as_strval()[0];
					switch (repeat_op)
					{
					default:
					case '&': case '!':
						continue;
					case '?':
						rep = StrVal("Optional(")+generate_railroad(atom)+")";
						break;
					case '*':
						rep =  StrVal("ZeroOrMore(")+generate_railroad(atom)+")";
						break;
					case '+':
						rep =  StrVal("OneOrMore(")+generate_railroad(atom)+")";
						break;
					}
					// Variant		label = repetition["label"]; // Ignore the label:
				}
				else
					rep =  generate_railroad(atom);

				if (i > 0)
					ret += ", ";

				ret += rep;
			}
			return ret;
		}
		else if (node_type == "group")
		{
			VariantArray	alternates = element.as_variant_map()["alternates"].as_variant_array();
			if (alternates.length() != 1)
			{
				// This happens e.g. on (a|b) when it should be (|a|b)
				assert(!"REVISIT: unexpected alternates count");
			}
			return StrVal("Sequence(") + generate_railroad(alternates[0]) + ")";
		}
		else if (node_type == "any")
		{
			return StrVal("Terminal('any char')");
		}
		else if (node_type == "call")
		{		// name of a rule to be called
			StrVal	e = element.as_strval();
			return StrVal("NonTerminal('") + e + "', {href: '#" + e + "'})";
		}
		else if (node_type == "property")
		{		// a property character, just backslash it:
			return StrVal("Terminal('\\\\") + element.as_strval() + "')";
		}
		else if (node_type == "literal")
		{		// body of a literal string
			return StrVal("Terminal('\\\\")
				+ generate_railroad_literal(element.as_strval())
				+ "')";
		}
		else if (node_type == "class")
		{		// body of a character class (backslashed properties keep their backslash)
			return StrVal("Terminal('[")
				+ generate_railroad_literal(element.as_strval(), true)
				+ "]')";
		}
		else	// Report incomplete node type:
			return StrVal("INCOMPLETE<") + node_type + ">=" + element.as_json(-2);
	}
	else if (re.type() == Variant::VarArray)
	{
		VariantArray	va = re.as_variant_array();
		StrVal	ret("Choice(0");
		for (int i = 0; i < va.length(); i++)
		{
			ret += ", ";
			ret += generate_railroad(va[i]);
		}
		ret += ")";
		return ret;
	}
	else
	{
		// This indicates the code above is incomplete. Dump the unhandled structure:
		return StrVal("INCOMPLETE CODE for ") + re.type_name() + "=" + re.as_json(-2);
	}
}

void emit_rule_railroad(
	Variant		_rule
)
{
	StrVariantMap	rule = _rule.as_variant_map()["rule"].as_variant_map();
	Variant		rulename_v = rule["name"];
	Variant		alternates_vmap = rule["alternates"]; // Variant(StrVariantMap)

#if 0
	Variant		action_v = rule["action"];

	// Append the capture array for this rule:
	StrVal		parameters;
	if (action_v.type() != Variant::None)
		parameters = generate_parameters(action_v.as_variant_map()["parameter"]);
	if (parameters != "")
	{
	}

	// The Parser doesn't yet use the function, if any:
	if (!action_v.is_null())
	{
		Variant	function = action_v.as_variant_map()["name"];	// StrVal or None
		if (!function.is_null())
			capture_arrays += StrVal("\t\t// FUNCTION: ")+function.as_strval()+"\n";
	}
#endif

	// Generate the Railroad diagram for this rule:
	printf(
		"<dt id='%s'>%s</dt>\n"
		"<dd><script>ComplexDiagram(\n"
		"%s\n"
		").addTo();</script>\n"
		"</dd>\n\n",
		rulename_v.as_strval().asUTF8(),
		rulename_v.as_strval().asUTF8(),
		generate_railroad(alternates_vmap).asUTF8()
	);
}

void emit_railroad(const char* base_name, VariantArray rules)
{
	StrVal		parser_name = StrVal((UCS4)base_name[0]).asUpper()+(base_name+1);
	const UTF8*	parser_name_u = parser_name.asUTF8();

	printf(
		"<html xmlns='http://www.w3.org/1999/xhtml'>\n"
		"<head>\n"
		"<title>%s Grammar</title>\n"
		"<link rel='stylesheet' href='railroad-diagrams.css'>\n"
/*
		"<link rel='stylesheet' href='local.css' media='screen' type='text/css' />\n"
		"<style>\n"
		"body svg.railroad-diagram {\n"
		"	background-color: hsl(30,20%%, 95%%);\n"
		"}\n"
		"h2 {\n"
		"	font-family: sans-serif;\n"
		"	font-size: 1em;\n"
		"}\n"
		"svg.railroad-diagram path,\n"
		"svg.railroad-diagram rect\n"
		"{\n"
		"	stroke-width: 2px;\n"
		"}\n"
		".railroad-diagram .terminal text {\n"
		"	fill: #44F;\n"
		"}\n"
		"div svg.railroad-diagram {\n"
		"	width: 80%%;  // Scale to the width of the parent\n"
		"	height: 100%%;  // Preserve the ratio\n"
		"}\n"
		"dt {\n"
		"	font-weight: bold;\n"
		"	padding-bottom: 5px;\n"
		"}\n"
		"dd {\n"
		"	padding-bottom: 10px;\n"
		"}\n"
		"</style>\n"
*/
		"<script src='railroad-diagrams.js'></script>\n"
		"</head>\n"
		"\n"
		"<body>\n"
		"<dl>\n"
		"\n",
		parser_name_u			// "Xxx Grammar"
	);

	for (int i = 0; i < rules.length(); i++)
		emit_rule_railroad(rules[i]);
	printf(
		"</dl>\n"
		"</body>\n"
		"</html>\n"
	);
}
