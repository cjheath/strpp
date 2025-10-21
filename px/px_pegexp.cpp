/*
 * Px PEG parser generator. Functions to make Px to Pegexp
 *
 * Copyright 2025 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<strval.h>
#include	<variant.h>
#include	<char_encoding.h>

#include	<px_parser.h>
#include	<px_pegexp.h>

/*
 * This function converts Px literal text into what Pegexp requires as input (at runtime).
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

// a repetition operator applies to this atom. Ensure it is wrapped in () if necessary
bool is_single_atom(Variant atom)
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
		{		// Each repetition has {optional repeat_count, atom, optional label}
			VariantArray	repetitions = element.as_variant_array();

			StrVal	ret;
			for (int i = 0; i < repetitions.length(); i++)
			{
				StrVariantMap	repetition = repetitions[i].as_variant_map();
				Variant		atom = repetition["atom"];
				Variant		repeat_count = repetition["repeat_count"]; // Maybe None
				bool		repeating = repeat_count.type() != Variant::None;
				if (repeating)
					ret += repeat_count.as_variant_map()["limit"].as_strval();
				bool sa = is_single_atom(atom);
				if (!sa && repeating)
					ret += "(";
				ret += generate_pegexp(atom);
				Variant		label = repetition["label"];	// Maybe None
				if (label.type() != Variant::None)
					ret += StrVal(":")+label.as_variant_map()["name"].as_strval()+":";
				if (!sa && repeating)
					ret += ")";
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
			return StrVal("(") + generate_pegexp(alternates[0]) + ")";
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

StrVal generate_parameter(Variant parameter_map)
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

StrVal generate_parameters(Variant parameters)
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
