/*
 * C++ code generator for a parser
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
#include	<px_cpp.h>

/*
 * Take a literal string containing the Pegexp text of a rule, and turn it into a C++ literal
 */
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

void emit_cpp(const char* base_name, VariantArray rules)
{
	StrVal	capture_arrays;
	StrVal	rules_text;
	StrVal	parser_name = StrVal((UCS4)base_name[0]).asUpper()+(base_name+1);
	StrVal	file_base_name = StrVal(base_name)+"_parser";

	for (int i = 0; i < rules.length(); i++)
	{
		if (i)
			rules_text += ",";
		rules_text += "\n";
		emit_rule_cpp(rules[i], capture_arrays, rules_text);
	}

	const UTF8*	parser_name_u = parser_name.asUTF8();
	const UTF8*	file_base_name_u = file_base_name.asUTF8();

	printf(
		"/*\n"
		" * Rules for a %sParser\n"
		" *\n"
		" * You must declare this type in %s.h by expanding the Peg<> template\n"
		" */\n"
		"#include\t<%s.h>\n"
		"\n"
		"%s\n"				// capture_arrays
		"template<>%sParser::Rule\t%sParser::rules[] =\n{"
		"%s\n"				// rules_text
		"};\n"
		"\n"
		"template<>int\t%sParser::num_rule = sizeof(%sParser::rules)/sizeof(%sParser::rules[0]);\n",

		parser_name_u,			// Rules for a XXX
		file_base_name_u,		// You must declare...
		file_base_name_u,		// #include...
		capture_arrays.asUTF8(),
		parser_name_u,			// XxParser::Rule XxParser::rules[] = {
		parser_name_u,
		rules_text.asUTF8(),
		parser_name_u,			// XxParser::num_rule
		parser_name_u,			// XxParser::rules
		parser_name_u			// XxParser::rules[0]
	);
}

