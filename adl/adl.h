#include	<stdio.h>
/*
 * Aspect Definition Language optimised parser
 */
#include	<char_encoding.h>
#include	<error.h>
#include	<fcntl.h>

class	ADLSourcePtr
{
	const UTF8*	data;
	int		peeked_bytes;
	int		_line_number;
	int		_column;

public:
	ADLSourcePtr(const UTF8* _data) : data(_data), peeked_bytes(0), _line_number(1), _column(1) {}
	ADLSourcePtr(const ADLSourcePtr& c) : data(c.data), peeked_bytes(0), _line_number(c._line_number), _column(c._column) {}
	UCS4	peek_char()
		{
			const UTF8*	tp = data;
			UCS4		ch = UTF8Get(tp);
			peeked_bytes = tp-data;
			return ch != 0 ? ch : UCS4_NONE;
		}
	void	advance()
		{
			if (peeked_bytes == 0)
				return;
			if (*data == '\n')
				_column = 1, _line_number++;
			else
				_column++;
			data += peeked_bytes;
			peeked_bytes = 0;
		}
	off_t	operator-(const ADLSourcePtr start) const { return data - start.data; }
	int	line_number() const { return _line_number; }
	int	column() const { return _column; }
	const char*	peek() const { return data; }

	void	print_from(const ADLSourcePtr& start) const {
			printf("%.*s", (int)(*this - start), start.data);
		}
	void	print_ahead() const {
			printf("`%.*s`...\n", 20, data);
		}
};

/*
 * This is an API stub. During parsing, these methods get called.
 * If Syntax lookup is required, you need to save enough data to implement it.
 */
template<typename _Source = ADLSourcePtr>
class ADLSink
{
public:
	using	Source = _Source;

	ADLSink() {}

	void	error(const char* why, const char* what, const Source& where)
		{
			printf("At line %d:%d, %s MISSING %s: ", where.line_number(), where.column(), why, what);
			where.print_ahead();
		}

	void	definition_starts() {}			// A declaration just started
	void	definition_ends() {}			// This declaration just ended
	void	ascend() {}				// Go up one scope level to look for a name
	void	name(Source start, Source end) {}	// A name exists between start and end
	void	descend() {}				// Go down one level from the last name
	void	pathname(bool ok) {}			// The sequence *ascend name *(descend name) is complete
	void	object_name() {}			// The last pathname was for a new object
	void	supertype() {}				// Last pathname was a supertype
	void	reference_type(bool is_multi) {}	// Last pathname was a reference
	void	reference_done(bool ok) {}		// Reference completed
	void	alias() {}				// Last pathname is an alias
	void	block_start() {}			// enter the block given by the pathname and supertype
	void	block_end() {}				// exit the block given by the pathname and supertype
	void	is_array() {}				// This definition is an array
	void	assignment(bool is_final) {}		// The value(s) are assigned to the current definition
	void	string_literal(Source start, Source end) {}	// Contents of a string between start and end
	void	numeric_literal(Source start, Source end) {}	// Contents of a number between start and end
	// void	pegexp_match(Source start, Source end) {}	// Contents of a matched value between start and end
	void	object_literal() {}			// An object_literal (supertype, block, assignment) was pushed
	void	reference_literal() {}			// The last pathname is a value to assign to a reference variable
	void	pegexp_literal(Source start, Source end) {}	// Contents of a pegexp between start and end

	Source	lookup_syntax(Source type)		// Return Source of a Pegexp string to use in matching
		{ return Source(""); }
};

template<
	typename _Sink = ADLSink<>
>
class	ADLParser
{
public:
	using		Sink = _Sink;
	using		Source = typename Sink::Source;
	typedef	Source	Type;

	~ADLParser() {}
	ADLParser() {}

	bool	parse(Source&);			// ?BOM *definition

	void	error(const char* why, const char* what, const Source& where)
		{ sink.error(why, what, where); }

protected:
	bool	definition(Source&);		// &. !'}' ?path_name body ?';'
	bool	path_name(Source&);		// *'.' ?(name *('.' name))
	bool	name(Source&);			// | symbol | integer
	bool	body(Source&);			// | reference | alias_from | ?supertype block |?supertype ?block ?post_body EOB
	bool	EOB(Source&);			// |&';' |&'}' |EOF
	bool	reference(Source&);		// (| '->' | '=>') path_name ?block ?assignment EOB
	bool	alias_from(Source&);		// '!' path_name EOB
	bool	supertype(Source&);		// ':' ?path_name
	bool	block(Source&);			// '{' *definition '}'
	bool	post_body(Source&, Type&);	// | '[]' ?assignment | assignment
	bool	assignment(Source&, Type&);	// | final_assignment | tentative_assignment
	bool	final_assignment(Source&, Type&);	// '=' value
	bool	tentative_assignment(Source&, Type&);	// '~=' value
	bool	value(Source&, Type&);		// | atomic_value | '[' atomic_value *(',' atomic_value) ']'
	bool	array_value(Source& source, Type&); // '[' atomic_value *(',' atomic_value) ']'
	bool	atomic_value(Source&, Type&);	// | '/' pegexp_sequence '/' | path_name | object_literal | pegexp_match
	bool	reference_literal(Source&, Type&);	// pathname
	bool	object_literal(Source&);	// supertype ?block ?assignment
	bool	pegexp_match(Source&, Type&);
	bool	space(Source&);			// Optional white-space
	// White-space is free above here, explicit below
	bool	symbol(Source&);		// [_\a] *[_\w\p{Mn}]
	bool	integer(Source&);		// [1-9] *[0-9]
	bool	pegexp(Source& source);		// '/' pegexp_sequence '/'
	bool	pegexp_sequence(Source&);	// | pegexp_atom | +('|' pegexp_atom)
	bool	pegexp_atom(Source&);		// ?[*+?] (| pegexp_char | pegexp_class | pegexp_group)
	bool	pegexp_group(Source&);		// '(' pegexp_sequence ')'
	bool	pegexp_lookahead(Source&);	// [&!] pegexp_atom
	bool	pegexp_char(Source&);		// | '\\[adhsw]' | '\\' ?[0-3] [0-7] ?[0-7] | '\\x' \h ?\h | '\\u' ?[0-1] \h ?\h ?\h ?\h
						// | '\\' [pP] '{' +[A-Za-z_] '}' | '\\' [0befntr\\*+?()|/\[] | [^*+?()|/\[ ]
	bool	pegexp_class(Source&);		// '[' ?'^' ?'-' +pegexp_class_part ']'
	bool	pegexp_class_part(Source&);	// !']' pegexp_class_char ?('-' !']' pegexp_class_char)
	bool	pegexp_class_char(Source&);	// | !'-' pegexp_char | [*+?()|/]

	// Bootstrap for values:
	bool	string_literal(Source&);
	bool	numeric_literal(Source&);

	Sink	sink;
};

// ?BOM *definition
template<typename Source> bool ADLParser<Source>::parse(Source& source)
{
	Source	probe = source;

	if (probe.peek_char() == 0xFEFF)	// ignore a Byte Order Mark
		probe.advance();
	space(probe);
	while (definition(probe))
		;
	// printf("PARSE ends at '%d': ", probe.peek_char()); probe.print_ahead();
	source = probe;
	return true;
}

// &. !'}' ?path_name body ?';'
template<typename Source> bool ADLParser<Source>::definition(Source& source)
{
	Source	probe = source;

	UCS4	ch = probe.peek_char();
	if (UCS4_NONE == ch || '}' == ch)	// EOF or closing }
		return false;			// I see no definition here

	sink.definition_starts();
	Source	name_start(probe);
	bool	has_path = path_name(probe);	// Accept a path_name
	sink.object_name();

	// printf("DEFINING `"); probe.print_from(name_start); printf("`\n");

	if (!body(probe))
		return false;
	// printf("DEFINITION ends `"); probe.print_from(p); printf("`\n");

	ch = probe.peek_char();
	if (';' == ch)
	{
		probe.advance();
		space(probe);
	}

	sink.definition_ends();

	// There was a body, so advance:
	source = probe;
	return true;
}

// &(|'.' |name) *'.' ?(name *('.' name))
template<typename Source> bool ADLParser<Source>::path_name(Source& source)
{
	Source	probe = source;
	bool	ok = false;

	// Ascend one scope level for each .
	while ('.' == probe.peek_char())
	{
		ok = true;
		probe.advance();
		sink.ascend();
		space(probe);
	}

	if (name(probe))
	{
		ok = true;
		space(probe);
		source = probe;			// Already succeeded, get more if we can

		while ('.' == probe.peek_char()) // See if we can descend
		{
			probe.advance();
			sink.descend();
			space(probe);
			if (!name(probe))
			{
				sink.pathname(true);
				return true;	// Succeed without advancing on previous results
			}
			space(probe);
			source = probe;		// Descent succeeded, try for more
		}
	}
	if (ok)
		source = probe;
	sink.pathname(ok);
	return ok;
}

// | symbol | integer
template<typename Source> bool ADLParser<Source>::name(Source& source)
{
	Source	probe = source;

	bool	ok = false;
	while (true)
	{
		Source	start = probe;
		if (!symbol(probe))
		{
			if (!integer(probe))
				return ok;
		}
		ok = true;
		sink.name(start, probe);
		space(probe);
		source = probe;
	}
}

// | reference | alias_from | ?supertype block |?supertype ?block ?post_body EOB
template<typename Source> bool ADLParser<Source>::body(Source& source)
{
	if (reference(source))
		return true;
	if (alias_from(source))
		return true;

	Source	probe(source);
	Type	type(probe);		// Use Syntax for this type
	bool	has_supertype = supertype(probe);
	bool	has_block = block(probe);
	bool	has_post_body = post_body(probe, type);

	if (!has_block && !EOB(probe))	// If there's no block, the body must be properly terminated
		return false;

	source = probe;
	return true;
}

// |';' |'}' |EOF
template<typename Source> bool ADLParser<Source>::EOB(Source& source)
{
	UCS4	ch = source.peek_char();

	return ';' == ch || '}' == ch || UCS4_NONE == ch;
}

// (| '->' | '=>') path_name ?block ?assignment EOB
template<typename Source> bool ADLParser<Source>::reference(Source& source)
{
	Source	probe(source);
	UCS4	ch;

	// Look for the reference symbol (-> or =>):
	ch = probe.peek_char();
	if ('-' != ch && '=' != ch)
		return false;
	probe.advance();
	if ('>' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	// We must find a path_name:
	Type	type(probe);
	if (!path_name(probe))
	{
		error("reference", "typename", probe);
		return false;
	}
	sink.reference_type(ch == '=');

	bool	has_block = block(probe);
	bool	has_assignment = assignment(probe, type);

	bool	ok = EOB(probe);
	sink.reference_done(ok);
	if (ok)
		source = probe;
	return ok;
}

// '!' path_name EOB
template<typename Source> bool ADLParser<Source>::alias_from(Source& source)
{
	Source	probe(source);

	if ('!' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	if (!path_name(probe))
		return false;

	bool	ok = EOB(probe);
	if (ok)
	{
		sink.alias();
		source = probe;
	}
	return ok;
}

// ':' ?path_name
template<typename Source> bool ADLParser<Source>::supertype(Source& source)
{
	Source	probe(source);

	if (':' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	Source	start(probe);
	bool	has_path_name = path_name(probe);
	sink.supertype();
	// printf("Found supertype path_name `"); probe.print_from(start); printf("`\n");
	space(probe);

	source = probe;
	return true;
}

// '{' *definition '}'
template<typename Source> bool ADLParser<Source>::block(Source& source)
{
	Source	probe(source);

	// Must start with {
	if ('{' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	sink.block_start();

	// Zero or more definitions:
	while (definition(probe))
		;

	// Must end with }
	if ('}' != probe.peek_char())
	{
		error("block", "closing }", probe);
		return false;
	}
	probe.advance();
	sink.block_end();
	space(probe);

	source = probe;
	return true;
}

// | '[]' ?assignment | assignment
template<typename Source> bool ADLParser<Source>::post_body(Source& source, Type& type)
{
	Source	probe(source);

	bool	is_array = false;
	bool	has_assignment;
	if ('[' == probe.peek_char())
	{
		probe.advance();

		if (']' != probe.peek_char())
		{
			error("array_indicator", "closing ]", probe);
			return false;	// '[' with no ']'
		}
		probe.advance();
		sink.is_array();
		space(probe);
		is_array = true;
	}

	has_assignment = assignment(probe, type);
	if (!is_array && !has_assignment)
		return false;

accept:	source = probe;
	return true;
}

// | final_assignment | tentative_assignment
template<typename Source> bool ADLParser<Source>::assignment(Source& source, Type& type)
{
	return final_assignment(source, type)
	    || tentative_assignment(source, type);
}

// '=' value
template<typename Source> bool ADLParser<Source>::final_assignment(Source& source, Type& type)
{
	Source	probe(source);
	if ('=' != probe.peek_char())
		return false;
	probe.advance();
	space(probe);

	bool	has_value = value(probe, type);
	if (!has_value)
	{
		error("final_assignment", "value", probe);
		return false;	// Assignment must have a value
	}

	sink.assignment(true);
	space(probe);
	source = probe; 
	return true;
}

// '~=' value
template<typename Source> bool ADLParser<Source>::tentative_assignment(Source& source, Type& type)
{
	Source	probe(source);
	if ('~' != probe.peek_char())
		return false;
	probe.advance();

	if ('=' != probe.peek_char())
	{
		error("tentative_assignment", "= after ~", probe);
		return false;	// '~' with no '='
	}
	probe.advance();
	space(probe);

	bool	has_value = value(probe, type);
	if (!has_value)
		return false;	// Assignment must have a value

	sink.assignment(false);
	space(probe);
	source = probe; 
	return true;
}

// | array_value | atomic_value
template<typename Source> bool ADLParser<Source>::value(Source& source, Type& type)
{
	return atomic_value(source, type)
	    || array_value(source, type);
}

// '[' atomic_value *(',' atomic_value) ']'
template<typename Source> bool ADLParser<Source>::array_value(Source& source, Type& type)
{
	Source	probe(source);

	if ('[' != probe.peek_char())
		return false;
	probe.advance();

	space(probe);

	UCS4	ch;
	while (true)
	{
		if (!atomic_value(probe, type))
			return false;

		ch = probe.peek_char();		// Save ch to avoid peeking again for ']'
		if (',' != ch)
			break;
		probe.advance();
		space(probe);
	}
	if (']' != ch)
		return false;
	probe.advance();
	space(probe);
	source = probe;
	return true;
}

// | '/' pegexp_sequence '/' | path_name | object_literal | pegexp_match
template<typename Source> bool ADLParser<Source>::atomic_value(Source& source, Type& type)
{
	bool	expecting_syntax = true;		// REVISIT: Set these appropriately depending on syntax
	bool	expecting_reference = true;

	if (expecting_syntax && '/' == source.peek_char())
		return pegexp(source);

	Source	probe(source);

	/* REVISIT: This should follow reference values, but we need Pegexp Syntax for that */
	Source	syntax = sink.lookup_syntax(type);
	if (pegexp_match(probe, syntax))
	{		// A hacked-up literal
		source = probe;
		return true;
	}

	if (expecting_reference)
	{
		if (reference_literal(probe, type)
		 || object_literal(probe))
		{
			source = probe;
			return true;
		}
	}
	return false;

/* REVISIT when pegexp_match is complete
printf("Looking for value matching Type syntax, at: "); source.print_ahead();
	return pegexp_match(source, syntax);
*/
}

template<typename Source> bool ADLParser<Source>::reference_literal(Source& source, Type& type)
{
	bool	ok = path_name(source);
	if (ok)
		sink.reference_literal();
	return ok;
}

// supertype ?block ?assignment
template<typename Source> bool ADLParser<Source>::object_literal(Source& source)
{
	Type	type(source);
	if (!supertype(source))
		return false;

	bool	has_block = block(source);
	bool	has_assignment = assignment(source, type);
	sink.object_literal();
	return true;
}

// Value matches the Type syntax of the variable being assigned
template<typename Source> bool ADLParser<Source>::pegexp_match(Source& source, Type& type)
{
	UCS4	ch = source.peek_char();

	/*
	 * REVISIT: match the assignment type syntax provided.
	 * This hack matches string and numeric values
	 */
	if ('\'' == ch)
		return string_literal(source);
	if ('0' <= ch && ch <= '9' || '-' == ch || '+' == ch)
		return numeric_literal(source);

	// sink.pegexp_match(source, probe);
	return false;
}

// Value matches the Type syntax for a string
template<typename Source> bool ADLParser<Source>::string_literal(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('\'' != ch)
		return false;
	probe.advance();

	Source	start(probe);
	while ((ch = probe.peek_char()) && '\'' != ch)
	{
		probe.advance();
		if (ch == '\\')
			probe.advance(), ch = probe.peek_char();
	}
	sink.string_literal(start, probe);
	probe.advance();
	source = probe;
	return true;
}

// Value matches the Type syntax for a number
template<typename Source> bool ADLParser<Source>::numeric_literal(Source& source)
{
	Source	probe(source);
	UCS4	ch;

	while ((ch = probe.peek_char()) && ('0' <= ch && ch <= '9' || '-' == ch || '+' == ch || ch == '.'))
		probe.advance();
	bool	ok = probe-source > 0;
	if (ok)
		sink.numeric_literal(source, probe);
	source = probe;
	return ok;
}

// Optional white-space: *(| +[ \t\n\r] | '//' *(!'\n' .))
template<typename Source> bool ADLParser<Source>::space(Source& source)
{
	Source	probe(source);
	UCS4	ch;

	while ((ch = probe.peek_char()) != UCS4_NONE)	// EOF
	{
		source = probe;	// Accept the current situation
		if (' ' == ch || '\t' == ch || '\n' == ch || '\r' == ch)
		{
			probe.advance();
			source = probe;
			continue;
		}
		if ('/' == ch)
		{
			probe.advance();
			if ('/' != probe.peek_char())
				break;
			probe.advance();
			while ((ch = probe.peek_char()) != UCS4_NONE)	// EOF
			{
				probe.advance();
				source = probe;
				if ('\n' == ch)
					break;
			}
			continue;
		}
		break;	// Not white-space
	}
	return true;
}

// White-space is free above here (not handled yet)

// [_\a] *[_\w]
template<typename Source> bool ADLParser<Source>::symbol(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('_' != ch && !UCS4IsAlphabetic(ch))
		return false;
	probe.advance();

	while ('_' == (ch = probe.peek_char())
	    || (UCS4IsAlphabetic(ch) || UCS4IsDecimal(ch) /* || UCS4IsNonSpacingMark(ch) */))
	    	probe.advance();
	source = probe;
	return true;
}

// [1-9] *[0-9]
template<typename Source> bool ADLParser<Source>::integer(Source& source)
{
	Source	probe(source);

	if (ASCIIDigit(probe.peek_char()) < 1)
		return false;
	probe.advance();
	while (ASCIIDigit(probe.peek_char()) >= 0)
		probe.advance();
	source = probe;
	return true;
}

// '/' pegexp_sequence '/'
template<typename Source> bool ADLParser<Source>::pegexp(Source& source)
{
	Source	probe(source);

	if ('/' != probe.peek_char())
		return false;
	probe.advance();
	Source	start(probe);

	if (!pegexp_sequence(probe))
		return false;

	if ('/' != probe.peek_char())
	{
		error("Pegexp", "closing /", probe);
		return false;
	}
	sink.pegexp_literal(start, probe);
	probe.advance();

	source = probe;
	return true;
}

// | +('|' +pegexp_atom) | *pegexp_atom
template<typename Source> bool ADLParser<Source>::pegexp_sequence(Source& source)
{
	Source	probe(source);
	if ('|' == probe.peek_char())
	{
		while ('|' == probe.peek_char())
		{
			probe.advance();
			bool	ok = false;

			while (pegexp_atom(probe))
				ok = true;
			if (!ok)
			{
				error("pegexp_sequence", "atom", probe);
				return false;
			}
		}
accept:		source = probe;
		return true;
	}

	while (pegexp_atom(probe))
		;
	goto accept;
}

// ?[*+?] (| pegexp_lookahead | pegexp_char | pegexp_class | pegexp_group)
template<typename Source> bool ADLParser<Source>::pegexp_atom(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	// Check for repetition operator
	if ('*' == ch || '+' == ch || '?' == ch)
		probe.advance();

	if (pegexp_lookahead(probe))
	{
accept:
		source = probe;
		return true;
	}

	if (pegexp_char(probe))
		goto accept;
	if (pegexp_class(probe))
		goto accept;
	if (pegexp_group(probe))
		goto accept;

	return false;
}

// '(' pegexp_sequence ')'
template<typename Source> bool ADLParser<Source>::pegexp_group(Source& source)
{
	Source	probe(source);

	if ('(' != probe.peek_char())
		return false;
	probe.advance();

	if (!pegexp_sequence(probe))
	{
		error("pegexp_group", "sequence", probe);
		return false;
	}
	
	if (')' != probe.peek_char())
	{
		error("pegexp_group", "closing )", probe);
		return false;
	}
	probe.advance();
	source = probe;
	return true;
}

// [&!] pegexp_atom
template<typename Source> bool ADLParser<Source>::pegexp_lookahead(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('&' != ch && '!' != ch)
		return false;
	probe.advance();
	return pegexp_atom(probe);
}

// | '\\[adhswLU]'			// alpha, digit, hexadecimal, whitespace, word (alpha or digit), Lowercase, Uppercase
// | '\\' ?[0-3] [0-7] ?[0-7]		// Octal character
// | '\\x' \h ?\h			// hex character, 1 or 2 digits
// | '\\x{' +\h '}'			// hex character \x{...}, arbitrary precision
// | '\\u' \h ?\h ?\h ?\h		// Unicode character 1..4 digits
// | '\\u{' +\h '}'			// Unicode character \u{...}, arbitrary precision
// | '\\' [pP] '{' +[A-Za-z_] '}'	// Unicode named property
// | '\\' [.0befntr\\*+?()|/\[]		// Special escapes
// | [^*+?()|/\[\0- ]	  		// Other non-ctl chars except pegexp operator initiators (but allow . operator)
template<typename Source> bool ADLParser<Source>::pegexp_char(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('\\' == ch)
	{
		probe.advance();
		ch = probe.peek_char();
		if (UCS4IsASCII(ch))
		{
			if (0 != strchr("adhswLU", (char)ch))
			{			// Character property is ok
				probe.advance();
		accept:		source = probe;
				return true;
			}
			if (ch >= '0' && ch <= '7')
			{			// Start of octal char
				bool	zero_to_three = ch <= '3';
				probe.advance();
				ch = probe.peek_char();
				if (ch >= '0' && ch <= '7')
				{
					probe.advance();
					if (!zero_to_three)
						goto accept;
					// 3rd digit is accepted only if the first was 0..3
					ch = probe.peek_char();
					if (ch >= '0' && ch <= '7')
						probe.advance();
				}
				goto accept;
			}
			if ('x' == ch || 'u' == ch)
			{
				bool	is_hex = 'x' == ch;
				probe.advance();
				ch = probe.peek_char();
				bool	has_curly = '{' == ch;
				if (has_curly)
					probe.advance(), ch = probe.peek_char();
				if (!UCS4HexDigit(ch))
					return false;
				const	int	max = is_hex ? (has_curly ? 8 : 2) : (has_curly ? 8 : 4);
				for (int i = 1; i < max; i++)
				{
					probe.advance(), ch = probe.peek_char();
					if (!UCS4HexDigit(ch))
						break;
				}
				if (has_curly && '}' != ch)
					return false;	// Missing closing curly
				goto accept;
			}
			if ('p' == ch || 'P' == ch)
			{			// char property name '\\' [pP] '{' +[A-Za-z_] '}'
				probe.advance();
				if ('{' != probe.peek_char())
					return false;
				probe.advance();
				ch = probe.peek_char();
				bool	got_one = false;
				while (ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' || '_' == ch)
				{
					got_one = true;
					probe.advance();
					ch = probe.peek_char();
				}
				if (!got_one || '}' != ch)
					return false;
				probe.advance();
				goto accept;
			}
			if (0 != strchr(".0befntr\\*+?()|/[", (char)ch))
			{			// Special escape
				probe.advance();
				goto accept;
			}
		}
		// Unrecognised escape after backslash
		return false;
	}

	// No control characters, or whitespace, or other unescaped special characters:
	if (ch <= ' ' || (UCS4IsASCII(ch) && 0 != strchr("*+?()|/\\[", ch)))
		return false;	// These chars are not allowed unescaped
	probe.advance();
	source = probe;
	return true;
}

// '[' ?'^' ?'-' +pegexp_class_part ']'
template<typename Source> bool ADLParser<Source>::pegexp_class(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if ('[' != ch)
		return false;
	probe.advance(), ch = probe.peek_char();

	if ('^' == ch)
		probe.advance(), ch = probe.peek_char();
	if ('-' == ch)	// a hyphen must be first
		probe.advance(), ch = probe.peek_char();
	if (!pegexp_class_part(probe))
	{
		error("pegexp_class", "valid class", probe);
		return false;
	}
	while (pegexp_class_part(probe))
		;
	if (']' != probe.peek_char())
	{
		error("pegexp_class", "]", probe);
		return false;
	}
	probe.advance();
	source = probe;
	return true;
}

// !']' pegexp_class_char ?('-' !']' pegexp_class_char)
template<typename Source> bool ADLParser<Source>::pegexp_class_part(Source& source)
{
	Source	probe(source);
	UCS4	ch = probe.peek_char();

	if (']' == ch)
		return false;
	if (!pegexp_class_char(probe))
	{
		error("pegexp_class_part", "valid class character", probe);
		return false;
	}
	ch = probe.peek_char();
	if ('-' == ch)
	{		// Character range
		probe.advance(), ch = probe.peek_char();
		if (']' == ch)
			return false;
		if (!pegexp_class_char(probe))
			return false;
	}
	source = probe;
	return true;
}

// | !'-' pegexp_char | [*+?()|/]
template<typename Source> bool ADLParser<Source>::pegexp_class_char(Source& source)
{
	Source	probe(source);

	UCS4	ch = probe.peek_char();

	if ('-' != ch && pegexp_char(probe))
	{
		source = probe;
		return true;
	}
	if (!UCS4IsASCII(ch) || 0 == strchr("*+?()|/", (char)ch))	// These chars are allowed in classes but not pegexps
		return false;
	probe.advance();
	source = probe;
	return true;
}
