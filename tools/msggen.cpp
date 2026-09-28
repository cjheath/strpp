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
 * Run as: msggen [-d <output-dir>] <adl.adl> [precursor.adl ...] <catalog.mcs>.
 * With no -d the two generated headers go to stdout, error numbers first;
 * Every input but the last is a precursor, loaded in the order given and
 * each from the context the one before it left behind; the last input is
 * the catalog. The order is the caller's to state, not this program's to
 * assume, because a file loaded in the wrong order does not parse: adl.adl
 * gives TOP and the built-ins, adl/cpp/ietf_languages.adl gives the
 * enumeration mcs.adl refers to but does not declare, and mcs.adl on its
 * own now fails. tools/Makefile's `regenerate` is the one caller.
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
 * A Message's default text is assigned to the Message itself, as a
 * sibling assignment within its Set: the schema gives Message
 * `Syntax = String`, which copies String's Syntax, so a Message is a
 * variable and takes a value directly. Its language is the Set's own
 * Language - not a property of the Message, which is why the default text
 * needs no Text object of its own.
 *
 * A variable and its value are two objects, not one: the Assignment that
 * holds a value is a separate child of the same parent, naming its
 * variable in its own `variable`. That is why the default text is found
 * beside the Message rather than inside it, and why nothing here treats an
 * object as if it carried its own value.
 *
 * Only a Message's own default text is read, and only from the catalog
 * itself. A Text object inside a Message carries a wording of that message
 * for another language, and names it in its own Language reference; those
 * are deliberately **not** read, so that what is generated refers to the
 * base message sets and to nothing else. A language file (strpp.es.mcs) is
 * therefore not an argument here at all - it is still checked by
 * `make -C tools check`, which parses it after the catalog it extends.
 *
 * Anything still carrying an aspect() is skipped, as before: a Contextual
 * Extension is a view from one context and is exactly what a generator
 * must not mistake for a real child (adl/HANDOFF.md, "Next planned work:
 * the MCS/message-catalog generator").
 *
 * Parameter types are not declared in the schema (a standing decision -
 * adl/HANDOFF.md §10.2/§10.4 as inherited from strpp/HANDOFF.md) and
 * nothing here can recover the informal parameter names in each message's
 * own preceding comment either - comments aren't part of the parsed tree.
 * So every generated function takes plain Variant parameters, named only
 * by position (p1, p2, ...) - a real, visible difference from the
 * hand-written pair it replaced (str_err.h/str_msg.h, removed 2026-09-28),
 * which had specific types (StrVal, int, ...) and meaningful names.
 * Variant's own non-explicit constructors from StrVal/int/long/long
 * long/const char* make this a drop-in for every existing call site
 * regardless.
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

/*
 * A message's name is words separated by spaces or underscores - a catalog
 * may write "Number overflow" or NUMBER_OVERFLOW - so both separate here, and
 * the two halves of each generated pair are built from the same words.
 */
static void words_of(StrVal name, Array<StrVal>& out)
{
	StrVal	word;
	int	i = 0, n = (int)name.length();
	for (i = 0; i <= n; i++)
	{
		UCS4	c = i < n ? name[i] : (UCS4)' ';
		if (c == ' ' || c == '_')
		{
			if (!word.isEmpty())
				out.push(word);
			word = StrVal();
		}
		else
			word += StrVal(c);
	}
}

// The macro half of the pair: every word upper-cased and joined with
// underscores ("Number overflow" -> NUMBER_OVERFLOW), naming the ErrNum value
// and the #define that holds it.
static StrVal macro_name(StrVal adl_name)
{
	Array<StrVal>	words;
	words_of(adl_name, words);
	StrVal	out;
	for (int w = 0; w < words.length(); w++)
	{
		if (w)
			out += "_";
		for (int i = 0; i < words[w].length(); i++)
			out += StrVal((UCS4)toupper((int)words[w][i]));
	}
	return out;
}

/*
 * The function half: PascalCase, one capital per word - except a word the
 * catalog already gave in capitals, which is left alone. An acronym is the
 * author's spelling of it and is not to be lower-cased: "Invalid YMDHMS"
 * gives ErrorTIM_InvalidYMDHMS, not ErrorTIM_InvalidYmdhms.
 */
static StrVal pascal_case(StrVal adl_name)
{
	Array<StrVal>	words;
	words_of(adl_name, words);
	StrVal	out;
	for (int w = 0; w < words.length(); w++)
	{
		StrVal	word = words[w];
		bool	all_caps = true;
		for (int i = 0; i < word.length(); i++)
			if (word[i] >= 'a' && word[i] <= 'z')
				all_caps = false;
		for (int i = 0; i < word.length(); i++)
		{
			UCS4	c = word[i];
			if (all_caps)
				out += StrVal(c);
			else
				out += StrVal((UCS4)(i == 0 ? toupper((int)c) : tolower((int)c)));
		}
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
// children, found the same children() way - and so is every Message's own
// text, since that is assigned to the Message and so sits here beside it,
// not inside it).
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
			continue;	// Prefix, Number, Language, an assignment - not a Message

		// The Message's own value: `ASSERT: Message { Number = 1; } ~= '...'`
		// leaves this assignment here, in the Set, as a sibling of ASSERT -
		// so Handle::assigned() (the store's own search, by variable identity
		// rather than by name), not a child of the Message.
		Handle	own = set.assigned(m);
		if (own.is_null() || own.value().string.isEmpty())
		{
			fprintf(stderr, "msggen: %s has no default text - skipped\n", m.name().asUTF8());
			continue;
		}
		StrVal	text = decode_string_literal(own.value().string);

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
			m.set_prefix.asUTF8(), macro_name(m.name).asUTF8(),
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
		" * every parameter here is a plain Variant, named only by position.\n"
		" * The hand-written pair this replaced (str_msg.h, until 2026-09-28)\n"
		" * used specific types and meaningful names instead. Variant's own\n"
		" * non-explicit constructors from StrVal/int/long/long long/const\n"
		" * char* make this a drop-in for every call site regardless.\n"
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
			m.set_prefix.asUTF8(), macro_name(m.name).asUTF8(), m.text.asUTF8());
		for (int p = 1; p <= m.params; p++)
			fprintf(f, " << p%d", p);
		fprintf(f, ");\n}\n\n");
	}

	fprintf(f, "#endif\t// %s\n", guard.asUTF8());
}

/* --------------------------------------------------------------------- */

int main(int argc, char** argv)
{
	const char*	out_dir = 0;			// none given: write to stdout
	int		first_input = 1;

	if (argc > 2 && argv[1][0] == '-' && argv[1][1] == 'd' && argv[1][2] == '\0')
	{
		out_dir = argv[2];
		first_input = 3;
	}
	if (argc - first_input < 2)
	{
		fprintf(stderr, "usage: %s [-d <output-dir>] <adl.adl> [precursor.adl ...] <catalog.mcs>\n",
			argv[0]);
		fprintf(stderr, "\tWith no -d, both generated headers are written to stdout:\n"
				"\tthe error numbers first, then the reporting functions.\n");
		return 1;
	}
	const char*	catalog_mcs = argv[argc-1];	// the last input is the catalog

	ADL::MemStore	store;
	ADLMemStoreSink	sink(store);

	/*
	 * Every input but the last is a precursor, loaded in the order given and
	 * each from the context the one before it left behind - which is why the
	 * order is the caller's to state and not this program's to assume. adl.adl
	 * comes first (TOP and the built-ins); a schema referring to an
	 * enumeration it does not declare comes before that schema; and the
	 * catalog comes last, since it is what the last statement of is read.
	 */
	sink.root_object = sink.last_object();		// nothing loaded yet - TOP
	for (int i = first_input; i < argc; i++)	// the catalog is the last of them
	{
		if (!load_file(sink, argv[i]))
		{
			fprintf(stderr, "msggen: %s did not parse cleanly\n", argv[i]);
			return 1;
		}
		sink.root_object = sink.last_object();
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

	FILE*	err_f = stdout;
	FILE*	msg_f = stdout;
	StrVal	err_path = "<stdout>", msg_path = "<stdout>";

	if (out_dir)
	{
		err_path = StrVal(out_dir) + "/" + lower + "_err.h";
		msg_path = StrVal(out_dir) + "/" + lower + "_msg.h";
		if ((err_f = fopen(err_path.asUTF8(), "w")) == 0
		 || (msg_f = fopen(msg_path.asUTF8(), "w")) == 0)
		{
			perror(out_dir);
			return 1;
		}
	}
	write_err_h(err_f, catalog_name, messages);
	write_msg_h(msg_f, catalog_name, messages);

	if (out_dir)
	{
		fclose(err_f);
		fclose(msg_f);
	}
	fprintf(stderr, "msggen: wrote %d messages to %s and %s\n",
		(int)messages.length(), err_path.asUTF8(), msg_path.asUTF8());
	return 0;
}
