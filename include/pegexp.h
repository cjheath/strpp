#if !defined(PEGEXP_H)
#define PEGEXP_H
/*
 * Pegular expressions, aka Pegexp, formally "regular PEGs"
 *
 * Possessive regular expressions using prefix operators
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 *
 * Pegexp's use Regex-style operators, but in prefix position:
 *	^	start of the input or start of any line
 *	$	end of the input or the end of any line
 *	.	any character (alternately, byte), including a newline
 *	?	Zero or one of the following expression
 *	*	Zero or more of the following expression
 *	+	One or more of the following expression
 *	(expr)	Group subexpressions (does not imply capture)
 *	|A|B...	Either A or B (or ...)
 *	&A	Continue only if A succeeds
 *	!A	Continue only if A fails
 *	anychar	match that non-operator character
 *	\char	match the escaped character (including the operators, 0 b e f n r t, and any other char)
 *	\a	alpha character (alternately, byte)
 *	\d	digit character (alternately, byte)
 *	\h	hexadecimal
 *	\s	whitespace character (alternately, byte)
 *	\w	word (alpha or digit) character (alternately, byte)
 *	\177	match the specified octal character
 *	\xXX	match the specified hexadecimal (0-9a-fA-F)
 *	\x{1-2}	match the specified hexadecimal (0-9a-fA-F)
 *	\u12345	match the specified 1-5 digit Unicode character (only if compiled for Unicode support)
 *	\u{1-5}	match the specified 1-5 digit Unicode character (only if compiled for Unicode support)
 *	[a-z]	Normal character (alternately, byte) class (a literal hyphen may occur at start)
 *	[^a-z]	Negated character (alternately, byte) class. Characters may include the \escapes listed above
 *	`	Prefix to specify 8-bit byte only, not UTF-8 character (REVISIT: accepted but not implemented)
 *	! @ # % _ ; <		Call the match_extended function
 *	control-character	Call the match_extended function
 * NOT YET IMPLEMENTED:
 *	{n,m}	match from n (default 0) to m (default unlimited) repetitions of the following expression.
 *	Captures.
 *
 * Any atom may be followed by :name (name is a repetition of alphanum or _) optionally terminated by a :
 * This indicates that the contents of the previous atom should be passed to the Capture with that name.
 * The default Capture is null, but you can define a Capture that saves the value matched by that atom.
 *
 * Note: Possessive alternates and possessive repetition will never backtrack.
 * Once an alternate has matched, no subsequent alterative will be tried in that group.
 * Once a repetition has been made, it will never be unwound.
 * It is your responsibility to ensure these possessive operators never match unless it's final.
 * You should use negative assertions to control inappropriate greed.
 *
 * You can use any scalar data type for Char, and any Source providing the required methods (even wrapping a socket).
 * You can subclass Pegexp to override match_extended&skip_extended to handle special command characters.
 * You can replace the default NullCapture with a capture object with features for your extended command chars.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */
#include	<cstdint>
#include	<cstdlib>
#include	<cctype>
#include	<char_encoding.h>

typedef	const char*	PegexpPC;	// The Pegex is always 8-bit, not UTF-8

/*
 * Adapt a pointer-ish thing to be a suitable input for a Pegexp,
 * by adding the methods get_byte(), get_char(), same() and at_eof().
 * same() should return true if the two Inputs are at the same position in the text
 */
template<
	typename DataPtr = const UTF8*,
	typename _Char = UCS4
>
class	PegexpPointerSource
{
public:
	using Char = _Char;
	PegexpPointerSource(const DataPtr cp) : data(cp) {}
	PegexpPointerSource(const PegexpPointerSource& pi) : data(pi.data) {}

	char		get_byte()
			{ return at_eof() ? 0 : (*data++ & 0xFF); }
	Char		get_char()
			{ if (sizeof(Char) == 1) return get_byte();
			  return (Char)(at_eof() ? UCS4_NONE : UTF8Get(data)); }
	bool		at_eof() const
			{ return *data == '\0'; }
	bool		same(PegexpPointerSource& other) const
			{ return data == other.data; }

protected:
	const UTF8*	data;
};

using	PegexpDefaultSource = PegexpPointerSource<>;

/*
 * The default Result (of matching an atom) is just a POD copy of the various pointers.
 * It does no copying or extra computation.
 * (which otherwise might be incurred by computing (from-to) for example).
 */
template <typename _Source = PegexpDefaultSource>
class	PegexpDefaultResult
{
public:
	using Source = _Source;

	// Any subclasses must have this constructor, to capture matched text:
	PegexpDefaultResult(Source _from, Source _to, PegexpPC _name = 0, int _name_len = 0)
	: from(_from)		// pointer to start of the text matched by this atom
	, to(_to)		// pointer to the text following the match
	, name(_name)		// Name of a label, or NULL
	, name_len(_name_len)	// Length of the label, if any
	{ }

	// REVISIT: Consider including "succeeding" here instead of in PegexpState?

	PegexpPC	name;
	int		name_len;
	Source		from;
	Source		to;
};

/*
 * An example of the API that Pegexp requires for Context.
 * Implement your own template class having this signature to capture match fragments,
 * or to pass your own context down to match_extended
 */
template <typename Result = PegexpDefaultResult<>>
class	PegexpNullContext
{
public:
	using	Source = typename Result::Source;
	PegexpNullContext() : capture_disabled(0) {}

	int		capture(Result) { return 0; }
	int		capture_count() const { return 0; }
	void		rollback_capture(int count) {}
	int		capture_disabled;
};

template <typename Source = PegexpDefaultSource>
class PegexpState
{
public:
	using 		Char = typename Source::Char;
	PegexpState()
			: pc(0), text(0)
			, succeeding(true), at_bol(true), binary_code(false) {}
	PegexpState(PegexpPC _pc, Source _text)
			: pc(_pc), text(_text)
			, succeeding(true), at_bol(true), binary_code(false) {}
	PegexpState(const PegexpState& c)
			: pc(c.pc), text(c.text)
			, succeeding(c.succeeding), at_bol(c.at_bol), binary_code(c.binary_code) {}
	PegexpState&		operator=(const PegexpState& c)
			{ pc = c.pc; text = c.text;
			  succeeding = c.succeeding; at_bol = c.at_bol; binary_code = c.binary_code;
			  return *this; }
	bool		ok() const { return succeeding; }
	Char		next()
			{
				Char	ch = binary_code ? text.get_byte() : text.get_char();
				binary_code = false;
				at_bol = ch == '\n';
				return ch;
			}

	PegexpPC	pc;		// Current location in the pegexp
	Source		text;		// Current text we're looking at
	bool		succeeding;	// Is this a viable path to completion?
	bool		at_bol;		// Start of text or line. REVISIT: Should be in Source implementation
	bool		binary_code;	// Expect next character in literal binary coding (not UTF-8, etc)

	// If you want to add debug tracing for example, you can override these in a subclass:
	PegexpState&	fail() { succeeding = false; return *this; }
	PegexpState&	progress() { succeeding = true; return *this; }
};

template<
	typename _Source = PegexpDefaultSource,
	typename _Result = PegexpDefaultResult<_Source>,
	typename _Context = PegexpNullContext<_Result>
>
class Pegexp
{
public:	// Expose our template types for subclasses to use:
	using 		Source = _Source;
	using 		Char = typename Source::Char;
	using 		State = PegexpState<Source>;
	using		Context = _Context;
	using		Result = _Result;

	PegexpPC	pegexp;	// The text of the Peg expression: 8-bit data, not UTF-8

	Pegexp(PegexpPC _pegexp) : pegexp(_pegexp) {}
	PegexpPC	code() const { return pegexp; }

	// Match the Peg expression at or after the start of the text:
	State		match(Source& start, Context* context = 0)
	{
		int	initial_captures = context ? context->capture_count() : 0;
		State	state(pegexp, start);
		State	result;
		while (true)
		{
			// Reset for another trial:
			state.pc = pegexp;
			state.text = start;
			state.succeeding = true;
			// state.at_bol = true;		// Don't reset, since we probably aren't at the start. REVISIT: This should be in the Source
			state.binary_code = false;
			if (context)
				context->rollback_capture(initial_captures);

			result = match_here(state, context);
			if (result.ok())
			{		// Succeeded
				if (*result.pc != '\0')	// The pegexp has a syntax error, e.g. an extra )
					return result.fail();
				return result;
			}
			if (start.at_eof())
				break;

			// Move forward one character
			(void)start.get_char();
		}
		return result;	// This will show where we last failed
	}

	State		match_here(Source& start, Context* context = 0)
	{
		State	state(pegexp, start);
		return match_here(state, context);
	}

	State		match_here(State& state, Context* context = 0)
	{
		if (*state.pc == '\0' || *state.pc == ')')
			return state.progress();
		int		sequence_capture_start = context ? context->capture_count() : 0;

		State	r = match_atom(state, context);
		while (r.ok() && *state.pc != '\0' && *state.pc != ')')
			r = match_atom(state, context);

		// If not all atoms in the sequence match, any captures in the incomplete sequence must be reverted:
		if (!r.ok() && context)
			context->rollback_capture(sequence_capture_start);
		return r;
	}

	static int	unhex(Char c)
	{
		if (c >= '0' && c <= '9')
			return c-'0';
		if (c >= 'A' && c <= 'F')
			return c-'A'+10;
		if (c >= 'a' && c <= 'f')
			return c-'a'+10;
		return -1;
	}

protected:
	Char		literal_char(PegexpPC& pc)
	{
		Char		rc = *pc++;
		bool		braces = false;

		if (rc == '\0')
		{
			pc--;
			return 0;
		}
		if (rc != '\\')
			return rc;
		rc = *pc++;
		if (rc >= '0' && rc <= '7')		// Octal
		{
			rc -= '0';
			if (*pc >= '0' && *pc <= '7')
				rc = (rc<<3) + *pc++-'0';
			if (*pc >= '0' && *pc <= '7')
				rc = (rc<<3) + *pc++-'0';
		}
		else if (rc == 'x')			// Hexadecimal
		{
			if (*pc == '\0')
				return 0;
			if ((braces = (*pc == '{')))
				pc++;
			rc = unhex(*pc++);
			if (rc < 0)
				return 0;		// Error: invalid first hex digit
			int second = unhex(*pc++);
			if (second >= 0)
				rc = (rc << 4) | second;
			else
				pc--;			// Invalid second hex digit
			if (braces && *pc == '}')
				pc++;
		}
		else if (rc == 'u')			// Unicode
		{
			rc = 0;
			if ((braces = (*pc == '{')))
				pc++;
			for (int i = 0; *pc != '\0' && i < 5; i++)
			{
				int	digit = unhex(*pc);
				if (digit < 0)
					break;
				rc = (rc<<4) | digit;
				pc++;
			}
			if (braces && *pc == '}')
				pc++;
		}
		else
		{
			static const char*	special_escapes = "n\nt\tr\rb\be\ef\f";

			for (const char* escape = special_escapes; *escape != '\0'; escape += 2)
				if (rc == *escape)
					return escape[1];
		}
		return rc;
	}

	bool		char_class(State& state)
	{
		if (state.text.at_eof())	// End of input never matches
			return false;

		bool	negated;
		State	start_state(state);
		start_state.pc--;
		if ((negated = (*state.pc == '^')))
			state.pc++;

		bool	in_class = false;
		Char	ch = state.next();
		while (*state.pc != '\0' && *state.pc != ']')
		{
			Char	c1;

			// Handle actual properties, not other escapes
			if (*state.pc == '\\' && isalpha(state.pc[1]))
			{
				state.pc++;
				if (char_property(state.pc, ch))
					in_class = true;
				continue;
			}

			c1 = literal_char(state.pc);
			if (*state.pc == '-')
			{
				Char	c2;
				state.pc++;
				c2 = literal_char(state.pc);
				in_class |= (ch >= c1 && ch <= c2);
			}
			else
				in_class |= (ch == c1);
		}
		if (*state.pc == ']')
			state.pc++;
		if (negated)
			in_class = !in_class;
		return in_class;
	}

	bool		char_property(PegexpPC& pc, Char ch)
	{
		switch (char32_t esc = *pc++)
		{
		case 'a':	return isalpha(ch);			// Alphabetic
		case 'd':	return isdigit(ch);			// Digit
		case 'h':	return isdigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');	// Hexadecimal
		case 's':	return isspace(ch);
		case 'w':	return isalpha(ch) || isdigit(ch);	// Alphabetic or digit
		default:	pc -= 2;	// oops, not a property
				esc = literal_char(pc);
				return esc == ch;			// Other escaped character, exact match
		}
	}

	// Null extension; treat extensions like literal characters
	virtual State	match_extended(State& state, Context* context)
	{
		return match_literal(state);
	}

	State		match_literal(State& state)
	{
		if (state.text.at_eof() || *state.pc != state.next())
			return state.fail();
		state.pc++;
		return state.progress();
	}

	/*
	 * The guts of the algorithm. Match one atom against the current text.
	 *
	 * If this returns true, state.pc has been advanced to the next atom,
	 * and state.text has consumed any input that matched.
	 *
	 * If it returns false, state.pc has still been advanced but state.text is unchanged
	 */
	State		match_atom(State& state, Context* context)
	{
		int		initial_captures = context ? context->capture_count() : 0;
		State		start_state(state);
		PegexpPC	skip_from = state.pc;
		bool		match = false;
		switch (char rc = *state.pc++)
		{
		case '\0':	// End of expression
			state.pc--;
		case ')':	// End of group
			match = true;
			break;

		case '^':	// Start of line
			match = state.at_bol;
			break;

		case '$':	// End of line or end of input
			match = state.text.at_eof() || state.next() == '\n';
			state = start_state;
			state.pc++;
			break;

		case '.':	// Any character
			if ((match = !state.text.at_eof()))
				(void)state.next();
			break;

		default:	// Literal character
			if (rc > 0 && rc < ' ')		// Control characters
				goto extended;
			match = !state.text.at_eof() && rc == state.next();
			break;

		case '\\':	// Escaped literal char
			match = !state.text.at_eof() && char_property(state.pc, state.next());
			break;

		case '[':	// Character class
			match = char_class(state);
			break;

		case '?':	// Zero or one
		case '*':	// Zero or more
		case '+':	// One or more
		// case '{':	// REVISIT: Implement counted repetition
		{
			int		min = rc == '+' ? 1 : 0;
			int		max = rc == '?' ? 1 : 0;
			const PegexpPC	repeat_pc = state.pc;
			int		repetitions = 0;

			while (repetitions < min)
			{
				state.pc = repeat_pc;
				if (!match_atom(state, context).ok())
					goto fail;
				repetitions++;
			}
			while (max == 0 || repetitions < max)
			{
				int		iteration_captures = context ? context->capture_count() : 0;
				Source		repetition_start = state.text;
				state.pc = repeat_pc;
				if (!match_atom(state, context).ok())
				{
					if (context)	// Undo new captures on failure of an unmatched iteration
						context->rollback_capture(iteration_captures);
					// Ensure that state.pc is now pointing at the following expression
					state.pc = repeat_pc;
					state.text = repetition_start;
					skip_atom(state.pc);
					state.progress();
					break;
				}
				if (state.text.same(repetition_start))
					break;	// We didn't advance, so don't keep trying. Can happen e.g. on *()
				repetitions++;
			}
			match = true;
			break;
		}

		case '(':	// Parenthesised group
			start_state.pc = state.pc;
			if (!match_here(state, context).ok())
				break;
			if (*state.pc != '\0')		// Don't advance past group if it was not closed
				state.pc++;		// Skip the ')'
			match = true;
			break;

		case '|':	// Alternates
		{
			PegexpPC	next_alternate = state.pc-1;
			int		alternate_captures = context ? context->capture_count() : 0;
			while (*next_alternate == '|')	// There's another alternate
			{
				state = start_state;
				state.pc = next_alternate+1;
				// REVISIT: This means failure reports the last alternative. Should it report the first?
				start_state.pc = next_alternate+1;

				// Work through all atoms of the current alternate:
				do {
					if (!match_atom(state, context).ok())
						break;
				} while (!(match = *state.pc == '\0' || *state.pc == ')' || *state.pc == '|'));

				if (match)
				{		// This alternate matched, skip to the end of the alternates
					while (*state.pc == '|')	// There's another alternate
						skip_atom(state.pc);
					break;
				}
				next_alternate = skip_atom(next_alternate);
				if (context)	// Undo new captures on failure of an alternate
					context->rollback_capture(initial_captures);
			}
			break;
		}

		case '&':	// Positive lookahead assertion
		case '!':	// Negative lookahead assertion
		{
			if (context)
				context->capture_disabled++;
			match = (rc == '!') != match_atom(state, context).ok();
			if (context)
				context->capture_disabled--;
			state.pc = skip_atom(skip_from);	// Advance state.pc to after the assertion
			state.text = start_state.text;		// We always continue with the original text
			break;
		}

		case '`':
			state.binary_code = true;
			match = true;
			break;

		// Extended commands:
		case '~':
		case '@':
		case '#':
		case '%':
		case '_':
		case ';':
		case '<':
		extended:
			state.pc--;
			state = match_extended(state, context);
			match = state.ok();
			break;
		}
		if (!match)
		{
	fail:		if (context)	// Undo new captures on failure
				context->rollback_capture(initial_captures);

#if 0
			/*
			 * REVISIT: If this is the furthest point reached in the text so far,
			 * record the rule that was last attempted. Multiple rules may be
			 * attempted at this point, we want to know what characters would
			 * have allowed the parse to proceed.
			 */
			if (context		// We need something to report to
			 && state.text >= context->furthest_success
			 && strchr("?*+(|&", rc)== (char*)0)	// Don't report special characters
			{
				// Should this be before the switch, because we hadn't failed then?
				if (start_state.text > context->furthest_success)
					context->wipe_failures();	// We got further this time

				/*
				 * Terminal symbols for which we can report failure:
				 * ^ = beginning of line
				 * $ = end of line
				 * !. = EOF
				 * . = any character
				 * [...] = character class (definition is between start_state.pc and state.pc)
				 * \literal character
				 * Otherwise it's a literal character.
				 * 	(Report the whole sequence? What if several have a common prefix?)
				 */
				context->record_failure(start_state, rc);
			}
#endif

			return start_state.fail();
		}

		// Detect a label and save the matched atom
		PegexpPC	name = 0;
		if (*state.pc == ':')
		{
			name = ++state.pc;
			while (isalpha(*state.pc) || isdigit(*state.pc) || *state.pc == '_')
				state.pc++;
			PegexpPC	name_end = state.pc;
			if (*state.pc == ':')
				state.pc++;
			if (context && context->capture_disabled == 0)
				(void)context->capture(Result(start_state.text, state.text, name, name_end-name));
		}
		return state.progress();
	}

	// Null extension; treat extensions like literal characters
	virtual void	skip_extended(PegexpPC& pc)
	{
		pc++;
	}

	const PegexpPC	skip_atom(PegexpPC& pc)
	{
		switch (char rc = *pc++)
		{
		case '\\':	// Escaped literal char
			pc--;
			(void)literal_char(pc);
			break;

		case '[':	// Skip char class
			if (*pc == '^')
				pc++;
			while (*pc != '\0' && *pc != ']')
			{
				(void)literal_char(pc);
				if (*pc == '-')
				{
					pc++;
					(void)literal_char(pc);
				}
			}
			if (*pc == ']')
				pc++;
			break;

		case '(':	// Group
			while (*pc != '\0' && *pc != ')')
				skip_atom(pc);
			if (*pc != '\0')
				pc++;
			break;

		case '|':	// First or next alternate; skip to next alternate
			while (*pc != '|' && *pc != ')' && *pc != '\0')
				skip_atom(pc);
			break;

		case '&':	// Positive lookahead assertion
		case '!':	// Negative lookahead assertion
			skip_atom(pc);
			break;

		// Extended commands:
		case '~':
		case '`':
		case '@':
		case '#':
		case '%':
		case '_':
		case ';':
		case '<':	// Extension
		extended:
			pc--;
			skip_extended(pc);
			break;

		default:
			if (rc > 0 && rc < ' ')		// Control characters
				goto extended;
			break;

		// As-yet unimplemented features:
		// case '{':	// REVISIT: Implement counted repetition
		}
		if (*pc == ':')
		{		// Skip a label
			pc++;
			while (isalpha(*pc) || isdigit(*pc) || *pc == '_')
				pc++;
			if (*pc == ':')
				pc++;
		}
		return pc;
	}
};

#endif	// PEGEXP_H
