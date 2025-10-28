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

StrArray	omitted_rules;

bool is_omitted(StrVal o)
{
	for (int i = 0; i < omitted_rules.length(); i++)
		if (o == omitted_rules[i])
			return true;
	return false;
}

/*
 * This function converts Px literal text into a string to display in a Railroad Terminal node.
 * The first transform produces the expected display. The second ensures it passes the Javascript compiler.
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
			const char*	sp;

			if (ch != '\\')
				return ch;

			// Check for c-style escape, leaving these escaped
			ch = UTF8Get(cp);
			if (ch < 0x80 && (sp = strchr(c_esc_chars, (char)ch)))
				return StrVal("\\")+ch;

			// Check for character property: adhswLU, leaving these escaped
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

			// Check for c-style escape:
			if (ch < 0x80 && (sp = strchr(c_escaped, (char)ch)))
				return StrVal("\\")+(UCS4)c_esc_chars[sp-c_escaped];

#if 1
			// Anything that's now not ASCII will hopefully display correctly in Javascript,
			// but we should expose the BOM at least.
			// If we had Unicode tables we could do this for all non-printing characters.
			if (ch == 0xFEFF)
#else
			// Turn anything that's now not ASCII into a Unicode escape:
			if (ch >= 0x80)
#endif
			{
				static	char	buf[16];

				snprintf(buf, sizeof(buf), "\\u{%X}", ch);
				static StrBody	temp_body(buf, false, strlen(buf));	// zero-touch string body
				return StrVal(&temp_body);
			}

			return ch;
		}
	);

	// fprintf(stderr, " via `%s`", literal.asUTF8());

	// Now escape the character so it's valid for Javascript:
	literal.transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4    ch = UTF8Get(cp);       // Get UCS4 character

			switch (ch)
			{
			case '\\':			// backslash escape is mandatory
			case '\'':			// We only use ' so that's mandatory
				return StrVal("\\")+ch;
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
				if (ch < ' ')		// Make other control characters display
					return StrVal("^")+(UCS4)(ch + '@');

				// Not required to replace Unicode characters by \u or \u{} escapes
				return ch;
			}

			return StrVal("\\")+ch;
		}
	);

	if (as_char_class)
	{
		if (literal[0] == '^')		// Handle negated classes
			literal = StrVal("![")+literal.substr(1)+"]";
		else
			literal = StrVal("[")+literal+"]";
	}

//	fprintf(stderr, " to `%s`\n", literal.asUTF8());
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
	case '&':
	case '!':
	{
		int	o = atom_str.find('\'');
		if (o >= 0) // Insert the predicate symbol at the start of the first string in the atom
			atom_str.insert(o+1, repeat_op);
		return atom_str;
	}
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
		if (is_omitted(e))
			return "";
		return StrVal("NonTerminal('") + e + "', {href: '#" + e + "'})";
	}
	else if (node_type == "property")
		return StrVal("Terminal('\\\\") + element.as_strval() + "')";
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
	assert(!"Surprising atom");
	return "'Unexpected'";
}

StrVal	generate_repetition(StrVariantMap repetition)
{
	Variant		repeat_count = repetition["repeat_count"];
	StrVariantMap	atom = repetition["atom"].as_variant_map();	//
			// -> any, call, property, literal, class, group

	StrVal	atom_str = generate_atom(atom);
	if (atom_str.length() == 0)
		return "";
	return generated_repeated(repeat_count, atom_str);
}

StrVal	generate_sequence(VariantArray repetitions)
{
	StrArray	items;
	for (int i = 0; i < repetitions.length(); i++)
	{
		StrVal	item = generate_repetition(repetitions[i].as_variant_map());
		if (item.length() == 0)
			continue;
		items.append(item);
	}

	if (items.length() == 0)
		return "";		// None left
	if (items.length() == 1)
		return items[0];	// Only one item in sequence -> no Sequence

	bool		started = false;
	StrVal	ret("Sequence(");
	for (int i = 0; i < items.length(); i++)
	{
		if (started)
			ret += ", ";
		started = true;
		ret += items[i];
	}
	return ret+")";
}

StrVal generate_alternates(StrVariantMap alternates)
{
	Variant	sequence_list = alternates["sequence"];	// Can be VariantArray or StrVariantMap

	if (sequence_list.type() == Variant::VarArray)
	{
		StrArray	choices;
		VariantArray	sequences = sequence_list.as_variant_array();
		for (int i = 0; i < sequences.length(); i++)
		{
			VariantArray	seq_items = sequences[i].as_variant_map()["repetition"].as_variant_array();
			StrVal		rep_str = generate_sequence(seq_items);

			if (rep_str.length() == 0)
				continue;
			choices.append(rep_str);
		}

		if (choices.length() == 0)
			return "";		// None left
		if (choices.length() == 1)	// Only one alternative
			return choices[0];

		bool	started = false;
		StrVal	ret("Choice(0, ");
		for (int i = 0; i < choices.length(); i++)
		{
			if (started)
				ret += ", ";
			started = true;
			ret += choices[i];
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
	StrVal		parser_name,
	StrVariantMap	rule,
	StrVal&		railroad_script,
	StrVal&		railroad_calls
)
{
	StrVal		rulename = rule["name"].as_strval();
	Variant		alternates_vmap = rule["alternates"]; // Variant(StrVariantMap)

	railroad_script +=
		StrVal("  ")+rulename+":\n"+
		"    "+"ComplexDiagram("+generate_railroad(alternates_vmap)+")";

	railroad_calls +=
		StrVal("<dt id='")+rulename+"'>"+rulename+"</dt>\n"+
		"  <dd><script>"+parser_name+"Railroads."+rulename+".addTo();</script></dd>\n";
}

void emit_railroad(const char* base_name, VariantArray rules)
{
	StrVal		parser_name = StrVal((UCS4)base_name[0]).asUpper()+(base_name+1);
	const UTF8*	parser_name_u = parser_name.asUTF8();

	StrVal		railroad_script = StrVal("var ")+parser_name+"Railroads = {";
	StrVal		railroad_calls = "<dl>\n";
	for (int i = 0; i < rules.length(); i++)
	{
		StrVariantMap	rule = rules[i].as_variant_map()["rule"].as_variant_map();

		if (is_omitted(rule["name"].as_strval()))
			continue;
		if (i)
			railroad_script += ",";
		railroad_script += "\n";
		emit_rule_railroad(parser_name, rule, railroad_script, railroad_calls);
	}
	railroad_script += "\n};\n";
	railroad_calls += "</dl>\n";

	printf(
		"<html xmlns='http://www.w3.org/1999/xhtml'>\n"
		"<head>\n"
		"<meta charset='UTF-8'>\n"
		"<title>%s Grammar</title>\n"
		"<link rel='stylesheet' href='railroad-diagrams.css'>\n"
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

		"<script src='railroad-diagrams.js'></script>\n"
		"<script>\n"
		"%s"
		"</script>\n"
		"</head>\n"
		"\n"
		"<body>\n"
		"%s"
		"\n"
		"</body>\n"
		"</html>\n",
		parser_name_u,			// "Xxx Grammar"
		railroad_script.asUTF8(),
		railroad_calls.asUTF8()
	);
}
