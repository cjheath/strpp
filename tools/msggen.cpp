/*
 * msggen: reads a Message Catalog Source (an MCS file, against
 * ../../adl/cpp's mcs.adl schema) and writes <name>_err.h/<name>_msg.h -
 * <name> being the catalog's own name (its file's last top-level
 * statement), downcased - as a faithful transcription of what the MCS
 * source declares. Not hard-wired to any one library: the catalog file
 * and its own name decide everything about the output - guard macros,
 * file names, comments - so the same binary generates strpp's own
 * strpp_err.h/strpp_msg.h from messages/strpp.mcs today and any other
 * library's headers from its own catalog tomorrow, unchanged.
 *
 * Not part of strpp's normal build, and not automated in the Makefile: run
 * by hand whenever a catalog's .mcs file changes, and the two generated
 * headers are committed - the reverse of the usual generated-file
 * convention, forced by a real cyclic dependency in this repo: adl/cpp
 * cannot build without strpp's own library, so strpp's build cannot
 * depend on adl/cpp's being built first. This program itself is the one
 * place in strpp/tools that is allowed to depend on ADL - never strpp's
 * own library or tests.
 *
 * Only the base definitions are read - the ones a plain, unfiltered
 * children() walk would still need `aspect().is_null()` to skip: any
 * Contextual Extension (e.g. strpp.es.mcs's Spanish text) is deliberately
 * invisible here, exactly as it should be for a generator that only ever
 * needs the base language (adl/HANDOFF.md, "Next planned work: the
 * MCS/message-catalog generator").
 *
 * Parameter types are not declared in the schema (a standing decision -
 * adl/HANDOFF.md §10.2/§10.4 as inherited from strpp/HANDOFF.md) and
 * nothing here can recover the informal parameter names in each message's
 * own preceding comment either - comments aren't part of the parsed tree.
 * So every generated function takes plain Variant parameters, named only
 * by position (p1, p2, ...) - a real, visible difference from strpp's own
 * hand-written str_err.h/str_msg.h, which use specific types (StrVal, int,
 * ...) and meaningful names. Variant's own non-explicit constructors from
 * StrVal/int/long/long long/const char* make this a drop-in for every
 * existing call site regardless.
 *
 * Below the line marked GENERATOR LOGIC, nothing touches ADL::MemStore (or
 * any other concrete Store) directly, and nothing names any one catalog or
 * library - only Handle/Value, kept as close as this codebase currently
 * allows to "the store API" the human asked this to depend on rather than
 * a specific backend, in case that API is formalised later. All of main()'s
 * own setup, which unavoidably is concrete, stays above that line.
 */
#include	<cstdio>
#include	<cstdlib>
#include	<cstring>
#include	<sys/stat.h>
#include	<unistd.h>

#include	<array.h>
#include	<strval.h>
#include	<adlparser.h>
#include	<adlstore.h>
#include	<adlmem.h>
#include	<adlstrval.h>
#include	<adl_display.h>

typedef	ADLStrValSink<ADL::MemStore>	ADLMemStoreSink;
typedef	ADLSourceStrVal			ADLMemSource;

static char* slurp_file(const char* filename, off_t* size_p)
{
	int		fd;
	struct	stat	stat;
	char*		text;
	if ((fd = open(filename, O_RDONLY)) < 0
	 || fstat(fd, &stat) < 0
	 || (stat.st_mode&S_IFMT) != S_IFREG
	 || (text = new char[stat.st_size+1]) == 0
	 || read(fd, text, stat.st_size) < stat.st_size)
	{
		perror(filename);
		exit(1);
	}
	if (size_p)
		*size_p = stat.st_size;
	text[stat.st_size] = '\0';
	return text;
}

static bool load_file(ADLMemStoreSink& sink, const char* filename)
{
	off_t	file_size;
	char*	raw = slurp_file(filename, &file_size);
	StrVal	text(raw, (StrValIndex)file_size, 0, ArrayTakeOver);
	ADLMemSource			source(text);
	ADLParser<ADLMemStoreSink>	adl(sink);
	bool	ok = adl.parse(source);
	unsigned	errors = adl_display_errors(filename);
	return ok && errors == 0;
}

/* ------------------------- GENERATOR LOGIC ------------------------- */
/* Nothing below here names ADL::MemStore, ADLStrValSink, ADLParser or any
 * other concrete Store/Sink/Parser type - only ADL::Handle and ADL::Value,
 * both already store-agnostic in shape even though no separate interface
 * for them has been pulled out yet. */

using	ADL::Handle;
using	ADL::Value;

/*
 * A String value is stored as raw source text, quotes and all (strpp's own
 * HANDOFF.md §10.3) - strip the surrounding quotes and decode the escapes
 * this catalog actually uses. Does not yet handle the full escape grammar
 * (octal, \x, \u) - none of strpp's own messages need them yet, and this
 * is flagged rather than silently wrong if that changes.
 */
static StrVal decode_string_literal(StrVal raw)
{
	StrVal	body = raw.substr(1).shorter(1);	// drop the leading/trailing '
	StrVal	out;
	int	i = 0, n = (int)body.length();
	while (i < n)
	{
		UCS4	c = body[i];
		if (c == '\\' && i+1 < n)
		{
			UCS4	e = body[i+1];
			switch (e)
			{
			case '\'':	out += "'"; break;
			case '\\':	out += "\\\\"; break;	// re-escaped for the C++ literal
			case 'n':	out += "\\n"; break;
			case 't':	out += "\\t"; break;
			case 'r':	out += "\\r"; break;
			case 'b':	out += "\\b"; break;
			case 'f':	out += "\\f"; break;
			default:
				fprintf(stderr, "msggen: unhandled escape '\\%c' in %s - passing through unescaped\n",
					(char)e, raw.asUTF8());
				out += StrVal(e);
			}
			i += 2;
		}
		else
		{
			if (c == '"')
				out += "\\\"";	// this literal is about to sit inside a C++ "..."
			else
				out += StrVal(c);
			i++;
		}
	}
	return out;
}

// The highest {N} placeholder used in a message's default text - the
// parameter count, since parameters are not declared anywhere else.
static int placeholder_count(StrVal text)
{
	int	highest = 0;
	int	i = 0, n = (int)text.length();
	while (i < n)
	{
		if (text[i] == '{')
		{
			int	value = 0;
			int	j = i+1;
			while (j < n && text[j] >= '0' && text[j] <= '9')
			{
				value = value*10 + (text[j] - '0');
				j++;
			}
			if (j < n && text[j] == '}' && j > i+1)
				highest = value > highest ? value : highest;
		}
		i++;
	}
	return highest;
}

// ADL_NAME_LIKE_THIS -> PascalCase, for the function name half of the pair
// (ErrorSTR_Assert, ErrorSTR_NumberOverflow, ...). The macro half needs no
// transform at all - the ADL name is already the right shape.
static StrVal pascal_case(StrVal adl_name)
{
	StrVal	out;
	bool	start_of_word = true;
	int	i = 0, n = (int)adl_name.length();
	while (i < n)
	{
		UCS4	c = adl_name[i];
		if (c == '_')
			start_of_word = true;
		else
		{
			out += StrVal((UCS4)(start_of_word ? toupper((int)c) : tolower((int)c)));
			start_of_word = false;
		}
		i++;
	}
	return out;
}

// name, all-lowercase - for file names derived from the catalog's own name.
static StrVal downcase(StrVal name)
{
	StrVal	out;
	int	i = 0, n = (int)name.length();
	while (i < n)
	{
		out += StrVal((UCS4)tolower((int)name[i]));
		i++;
	}
	return out;
}

// name, all-uppercase - for #include guards derived from the catalog's own
// name.
static StrVal upcase(StrVal name)
{
	StrVal	out;
	int	i = 0, n = (int)name.length();
	while (i < n)
	{
		out += StrVal((UCS4)toupper((int)name[i]));
		i++;
	}
	return out;
}

// Handle::lookup() only searches one level, *locally* - it can never find
// a field that's purely inherited and has no local child of its own,
// which is exactly the shape of every "Prefix ~= 'STR';"-style assignment
// here (Set/Message declare the field; each instance only ever assigns a
// value to it, never re-declares it). The assigned value itself lives on
// a separate, anonymous Assignment object, a sibling within the same
// container, found by matching its own variable() by name. (Found by
// crashing on a null Handle first - Object::lookup() has no supertype
// fallback at all, so it simply doesn't find these.)
static Value assigned_value(Handle container, StrVal field_name)
{
	Array<Handle>&	kids = container.children();
	for (int i = 0; i < kids.length(); i++)
	{
		Handle	k = kids[i];
		if (k.is_assignment() && k.variable().name() == field_name)
			return k.value();
	}
	return Value();
}

// The same shape, for the one case where the field itself has no name to
// match by - the anonymous Text object-literal (README "Anonymity": not
// nameable or reopenable). `field` here is the exact Text handle already
// found by children(); its own assigned value is a sibling Assignment
// within the same container, matched by variable() identity instead of by
// name.
static Value assigned_value(Handle container, Handle field)
{
	Array<Handle>&	kids = container.children();
	for (int i = 0; i < kids.length(); i++)
	{
		Handle	k = kids[i];
		if (k.is_assignment() && k.variable() == field)
			return k.value();
	}
	return Value();
}

// The one real Text child of a Message - not found by name (it's
// anonymous, per README "Anonymity" - not nameable or reopenable), and
// not just the first child of the right supertype either: skip anything
// with a non-null aspect, since a Contextual Extension (a translation) is
// exactly what this generator must never see.
static Handle base_text_of(Handle message)
{
	Array<Handle>&	kids = message.children();
	for (int i = 0; i < kids.length(); i++)
	{
		Handle	k = kids[i];
		if (!k.aspect().is_null())
			continue;
		Handle	s = k.super();
		if (!s.is_null() && s.name() == "Text")
			return k;
	}
	return Handle();
}

struct GeneratedMessage
{
	StrVal	set_prefix;	// 'STR', 'VAR', ... (already decoded, no quotes)
	StrVal	name;		// ASSERT, NUMBER_OVERFLOW, ...
	int	number;		// Message.Number
	StrVal	text;		// decoded default text, C++-literal-ready
	int	params;		// highest {N} used
};

// Every Set's own Messages, in declaration order, skipping anything
// Contextual Extension attached (aspect().is_null()) and anything that
// isn't a Message at all (Prefix/Number/Language are Set's own other
// children, found the same children() way).
static void collect_set(Handle set, Array<GeneratedMessage>& out)
{
	StrVal	prefix = decode_string_literal(assigned_value(set, StrVal("Prefix")).string);
	Array<Handle>&	kids = set.children();
	for (int i = 0; i < kids.length(); i++)
	{
		Handle	m = kids[i];
		if (!m.aspect().is_null())
			continue;
		Handle	s = m.super();
		if (s.is_null() || s.name() != "Message")
			continue;	// Prefix, Number, Language - not a Message

		Handle	text_h = base_text_of(m);
		if (text_h.is_null())
		{
			fprintf(stderr, "msggen: %s has no base Text - skipped\n", m.name().asUTF8());
			continue;
		}
		StrVal	text = decode_string_literal(assigned_value(m, text_h).string);

		GeneratedMessage	gm;
		gm.set_prefix = prefix;
		gm.name = m.name();
		gm.number = atoi(assigned_value(m, StrVal("Number")).string.asUTF8());
		gm.text = text;
		gm.params = placeholder_count(text);
		out.push(gm);
	}
}

// The catalog's own Sets, in declaration order - every child with a null
// aspect (skipping any Contextual Extension of the catalog itself, though
// none exists in strpp.mcs today) and a real Set as its supertype.
static void collect_catalog(Handle catalog, Array<GeneratedMessage>& out)
{
	Array<Handle>&	kids = catalog.children();
	for (int i = 0; i < kids.length(); i++)
	{
		Handle	set = kids[i];
		if (!set.aspect().is_null())
			continue;
		Handle	s = set.super();
		if (s.is_null() || s.name() != "Set")
			continue;
		collect_set(set, out);
	}
}

static void write_err_h(FILE* f, StrVal catalog_name, Array<GeneratedMessage>& messages)
{
	StrVal	lower = downcase(catalog_name);
	StrVal	guard = upcase(catalog_name) + StrVal("_ERR_H");
	fprintf(f,
		"#if\t!defined(%s)\n"
		"#define\t%s\n"
		"/*\n"
		" * GENERATED by msggen.cpp from %s's own MCS source - do not\n"
		" * hand-edit. Regenerate and re-commit after changing it; see\n"
		" * msggen.cpp's own header comment for why the generated file is\n"
		" * committed rather than built on demand.\n"
		" *\n"
		" * %s's error numbers: one per message in the set, with the message's\n"
		" * default text in a comment. %s_msg.h holds the function that reports\n"
		" * each of these.\n"
		" */\n"
		"#include\t<error.h>\n\n",
		guard.asUTF8(), guard.asUTF8(), catalog_name.asUTF8(),
		catalog_name.asUTF8(), lower.asUTF8());

	// Set-number #defines, once per distinct prefix, in first-seen order.
	Array<StrVal>	seen;
	for (int i = 0; i < messages.length(); i++)
	{
		StrVal	p = messages[i].set_prefix;
		bool	already = false;
		for (int j = 0; j < seen.length(); j++)
			if (seen[j] == p)
				already = true;
		if (!already)
		{
			seen.push(p);
			fprintf(f, "#define\t%sERR_SET\t\t\t%d\t// The message set allocated to the %s\n",
				p.asUTF8(), (int)seen.length(), p.asUTF8());
		}
	}
	fprintf(f, "\n");

	StrVal	last_prefix;
	for (int i = 0; i < messages.length(); i++)
	{
		GeneratedMessage	m = messages[i];
		if (m.set_prefix != last_prefix)
		{
			fprintf(f, "// %s:\n\n", m.set_prefix.asUTF8());
			last_prefix = m.set_prefix;
		}
		fprintf(f, "#define\t%sERR_%s\t\tErrNum(%sERR_SET, %d)\t// %s\n",
			m.set_prefix.asUTF8(), m.name.asUTF8(),
			m.set_prefix.asUTF8(), m.number, m.text.asUTF8());
	}

	fprintf(f, "\n#endif\t// %s\n", guard.asUTF8());
}

static void write_msg_h(FILE* f, StrVal catalog_name, Array<GeneratedMessage>& messages)
{
	StrVal	lower = downcase(catalog_name);
	StrVal	guard = upcase(catalog_name) + StrVal("_MSG_H");
	fprintf(f,
		"#if\t!defined(%s)\n"
		"#define\t%s\n"
		"/*\n"
		" * GENERATED by msggen.cpp from %s's own MCS source - do not\n"
		" * hand-edit. Regenerate and re-commit after changing it.\n"
		" *\n"
		" * %s's error reporting functions: one per message, gathering the\n"
		" * parameters its default text calls for and handing them to the error\n"
		" * buffer's Error(). %s_err.h holds the numbers.\n"
		" *\n"
		" * Parameters are not typed or named in the MCS schema (a standing\n"
		" * decision, and comments aren't part of the parsed tree either), so\n"
		" * every parameter here is a plain Variant, named only by position -\n"
		" * unlike strpp's own hand-written str_msg.h, which uses specific\n"
		" * types and meaningful names. Variant's own non-explicit constructors\n"
		" * from StrVal/int/long/long long/const char* make this a drop-in for\n"
		" * every existing call site regardless.\n"
		" */\n"
		"#include\t<%s_err.h>\n"
		"#include\t<errbuf.h>\n"
		"#include\t<strval.h>\n"
		"#include\t<variant.h>\n\n",
		guard.asUTF8(), guard.asUTF8(), catalog_name.asUTF8(),
		catalog_name.asUTF8(), lower.asUTF8(), lower.asUTF8());

	StrVal	last_prefix;
	for (int i = 0; i < messages.length(); i++)
	{
		GeneratedMessage	m = messages[i];
		if (m.set_prefix != last_prefix)
		{
			fprintf(f, "// %s:\n\n", m.set_prefix.asUTF8());
			last_prefix = m.set_prefix;
		}

		fprintf(f, "inline ErrNum\nError%s_%s(", m.set_prefix.asUTF8(), pascal_case(m.name).asUTF8());
		for (int p = 1; p <= m.params; p++)
			fprintf(f, "%sVariant p%d", p > 1 ? ", " : "", p);
		fprintf(f, ")\n{\n\treturn Error(%sERR_%s,\n\t\t\"%s\",\n\t\tVariantArray()",
			m.set_prefix.asUTF8(), m.name.asUTF8(), m.text.asUTF8());
		for (int p = 1; p <= m.params; p++)
			fprintf(f, " << p%d", p);
		fprintf(f, ");\n}\n\n");
	}

	fprintf(f, "#endif\t// %s\n", guard.asUTF8());
}

/* --------------------------------------------------------------------- */

int main(int argc, char** argv)
{
	if (argc != 5)
	{
		fprintf(stderr, "usage: %s <adl.adl> <mcs.adl> <catalog.mcs> <output-dir>\n", argv[0]);
		return 1;
	}
	const char*	adl_adl = argv[1];
	const char*	mcs_adl = argv[2];
	const char*	catalog_mcs = argv[3];
	const char*	out_dir = argv[4];

	ADL::MemStore	store;
	ADLMemStoreSink	sink(store);

	sink.root_object = sink.last_object();		// nothing loaded yet - TOP
	if (!load_file(sink, adl_adl))
	{
		fprintf(stderr, "msggen: %s did not parse cleanly\n", adl_adl);
		return 1;
	}
	sink.root_object = sink.last_object();
	if (!load_file(sink, mcs_adl))
	{
		fprintf(stderr, "msggen: %s did not parse cleanly\n", mcs_adl);
		return 1;
	}
	sink.root_object = sink.last_object();
	if (!load_file(sink, catalog_mcs))
	{
		fprintf(stderr, "msggen: %s did not parse cleanly\n", catalog_mcs);
		return 1;
	}

	Handle	catalog = sink.last_object();	// the catalog file's own last statement
	if (catalog.is_null())
	{
		fprintf(stderr, "msggen: %s has no top-level statement to generate from\n", catalog_mcs);
		return 1;
	}
	StrVal	catalog_name = catalog.name();
	StrVal	lower = downcase(catalog_name);

	Array<GeneratedMessage>	messages;
	collect_catalog(catalog, messages);
	if (messages.length() == 0)
	{
		fprintf(stderr, "msggen: no messages found under %s - nothing to generate\n",
			catalog_name.asUTF8());
		return 1;
	}

	StrVal	err_path = StrVal(out_dir) + "/" + lower + "_err.h";
	StrVal	msg_path = StrVal(out_dir) + "/" + lower + "_msg.h";
	FILE*	err_f = fopen(err_path.asUTF8(), "w");
	FILE*	msg_f = fopen(msg_path.asUTF8(), "w");
	if (!err_f || !msg_f)
	{
		perror(out_dir);
		return 1;
	}
	write_err_h(err_f, catalog_name, messages);
	write_msg_h(msg_f, catalog_name, messages);
	fclose(err_f);
	fclose(msg_f);

	fprintf(stderr, "msggen: wrote %d messages to %s and %s\n",
		(int)messages.length(), err_path.asUTF8(), msg_path.asUTF8());
	return 0;
}
