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
 * The first transform produces the expected display. The second ensures that passes the Javascript compiler.
 */
StrVal generate_railroad_literal(StrVal literal, bool as_char_class = false)
{
	static	auto	c_escaped = "\n\t\r\b\f";	// C also defines \a, \v but they're not well-known
	static	auto	c_esc_chars = "ntrbf";		// Match the character positions
	static	auto	hex = "0123456789ABCDEF";

	// fprintf(stderr, "generate_railroad_literal (as_char_class %s) from `%s`", as_char_class ? "true" : "false", literal.asUTF8());

	// Unescape special characters to what we need to match:
	literal.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			ErrNum		e;
			unsigned int	i;
			UCS4    	ch = UTF8Get(cp);       // Get UCS4 character

			if (ch != '\\')
				return ch;

			// Check for c-style escape:
			const char*	sp;
			ch = UTF8Get(cp);
			if (ch < 0x80 && (sp = strchr(c_esc_chars, (char)ch)))
				return (UCS4)c_escaped[sp-c_esc_chars];

			// Check for character property: adhswLU
			if (ch < 0x80 && 0 != strchr("adhswLU", (char)ch))
				return StrVal("\\")+ch;

			// Handle numeric escapes:
			StrVal	s;
			if (ch == '0')		// Octal
			{
				StrBody	temp_body(cp, false, 3);		// no-copy string body
				ch = StrVal(&temp_body).asInt32(&e, 8, &i);
				if (e == 0 || e == STRERR_TRAIL_TEXT)
					cp += i;
			}
			if (ch == 'x')		// Hex, two formats
			{
				if (*cp != '{')
				{
					StrBody	temp_body(cp, false, 2);	// no-copy string body
					ch = StrVal(&temp_body).asInt32(&e, 16, &i);
				}
				else
				{
					StrBody	temp_body(cp+1, false, 8);	// no-copy string body
					ch = StrVal(&temp_body).asInt32(&e, 16, &i);
				}
				if (e == 0 || e == STRERR_TRAIL_TEXT)
					cp += i + (*cp == '{' ? 2 : 0);
			}
			else if (ch == 'u')	// Unicode, two formats
			{
				if (*cp == '{')
				{
					StrBody	temp_body(cp+1, false, 8);	// no-copy string body
					ch = StrVal(&temp_body).asInt32(&e, 16, &i);
				}
				else
				{
					StrBody	temp_body(cp, false, 4);	// no-copy string body
					ch = StrVal(&temp_body).asInt32(&e, 16, &i);
				}
				if (e == 0 || e == STRERR_TRAIL_TEXT)
					cp += i + (*cp == '{' ? 2 : 0);
				else
					fprintf(stderr, "error 0x%X\n", (int32_t)e);
			}
			return ch;
		}
	);

	// Now escape the character so it's valid for Javascript:
	literal.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4    ch = UTF8Get(cp);       // Get UCS4 character

			switch (ch)
			{
			case '\\':			// backslash escape is mandatory
			case '\'':			// We only use ' so that's mandatory
				break;
			case '\n': ch = 'n'; break;	// Newline escape is mandatory
			case '\r': ch = 'r'; break;	// Carriage return escape is mandatory
			// These replacements aren't mandatory, but nice:
			case '\0': ch = '0'; break;
			case '"':  ch = '"'; break;
			case '\v': ch = 'v'; break;
			case '\t': ch = 't'; break;
			case '\b': ch = 'b'; break;
			case '\f': ch = 'f'; break;
			default:
				// Not required to replace control&Unicode characters by \u or \u{} escapes
				// if (ch < ' ') { }	// REVISIT: Make other control characters display
				return ch;
			}

			return StrVal("\\\\")+ch;
		}
	);

	if (as_char_class)
	{
		if (literal[0] == '^')		// Handle negated classes
			literal = StrVal("![")+literal.substr(1)+"]";
		else
			literal = StrVal("[")+literal+"]";
	}

	// fprintf(stderr, " to `%s`\n", literal.asUTF8());
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

StrVal	generate_alternates(StrVariantMap alternates);

StrVal	generated_repeated(Variant repeat_count, StrVal atom_str)
{
	if (repeat_count.type() == Variant::None)
		return atom_str;

	UCS4 repeat_op = repeat_count.as_variant_map()["limit"].as_strval()[0];
	switch (repeat_op)
	{
	default:
	case '&': case '!':
		return "";
	case '?':
		return StrVal("Optional(")+atom_str+")";
	case '*':
		return StrVal("ZeroOrMore(")+atom_str+")";
	case '+':
		return StrVal("OneOrMore(")+atom_str+")";
	}
}

StrVal	generate_atom(StrVariantMap atom)
{
	auto		entry = atom.begin();
	StrVal		node_type = entry->first;
	Variant		element = entry->second;

	if (node_type == "any")
		return "Terminal('any char')";
	else if (node_type == "call")
	{
		StrVal	e = element.as_strval();
		return StrVal("NonTerminal('") + e + "', {href: '#" + e + "'})";
	}
	else if (node_type == "property")
		return StrVal("Terminal('") + element.as_strval() + "')";
	else if (node_type == "literal")
		return StrVal("Terminal('")
			+ generate_railroad_literal(element.as_strval())
			+ "')";
	else if (node_type == "class")
		return StrVal("Terminal('")
			+ generate_railroad_literal(element.as_strval(), true)
			+ "')";
	else if (node_type == "group")
	{
		VariantArray	alternates = element.as_variant_map()["alternates"].as_variant_array();
		return generate_alternates(alternates[0].as_variant_map());
	}
	else
		assert(!"Surprising atom");
	return "";
}

StrVal	generate_repetition(StrVariantMap repetition)
{
	Variant		repeat_count = repetition["repeat_count"];
	StrVariantMap	atom = repetition["atom"].as_variant_map();	//
			// -> any, call, property, literal, class, group

	return generated_repeated(repeat_count, generate_atom(atom));
}

StrVal	generate_sequence(VariantArray repetitions)
{
	StrVal	ret("Sequence(");
	for (int i = 0; i < repetitions.length(); i++)
	{
		if (i > 0)
			ret += ", ";
		ret += generate_repetition(repetitions[i].as_variant_map());
	}
	return ret+")";
}

// rule -> alternates -> sequence -> repetition -> repeat_count & atom -> { any, call, property, literal, class, group }

StrVal generate_alternates(StrVariantMap alternates)
{
	Variant	sequence_list = alternates["sequence"];	// Can be VariantArray or StrVariantMap
	// printf("sequences: %s\n", sequence_list.as_json().asUTF8());

	if (sequence_list.type() == Variant::VarArray)
	{
		VariantArray	sequences = sequence_list.as_variant_array();
		if (sequences.length() == 1)	// Only one alternative
			return generate_sequence(sequences[0].as_variant_map()["repetition"].as_variant_array());

		StrVal	ret("Choice(0, ");
		for (int i = 0; i < sequences.length(); i++)
		{
			if (i > 0)
				ret += ", ";
			ret += generate_sequence(sequences[i].as_variant_map()["repetition"].as_variant_array());
		}
		return ret + ")";
	}
	else
	{
		return generate_sequence(sequence_list.as_variant_map()["repetition"].as_variant_array());
	}
}

/*
 * For the Variant node passed, generate the railroad.js expression which corresponds
 */
StrVal generate_railroad(Variant re)
{
	return generate_alternates(re.as_variant_map());
}

void emit_rule_railroad(
	Variant		_rule
)
{
	StrVariantMap	rule = _rule.as_variant_map()["rule"].as_variant_map();
	Variant		rulename_v = rule["name"];
	Variant		alternates_vmap = rule["alternates"]; // Variant(StrVariantMap)

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
