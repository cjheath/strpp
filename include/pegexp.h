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
 * You can use any scalar data type for PChar, and a pointer to it for TextPtr (even wrapping a socket).
 * You can subclass Pegexp to override match_extended&skip_extended to handle special command characters.
 * You can replace the default NullCapture with a capture object with features for your extended command chars.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */
#include	<cstdint>
#include	<cstdlib>
#include	<ctype.h>
#include	<char_encoding.h>

typedef	const char*	PegexpPC;

/*
 * Adapt a pointer-ish thing to be a suitable input for a Pegexp,
 * by adding the methods get_byte(), get_char(), same() and at_eof().
 * same() should return true if the two Inputs are at the same position in the text
 */
template<typename DataPtr = const UTF8*, typename PIChar = UCS4>
class	PegexpPointerInput
{
public:
	using Char = PIChar;
	PegexpPointerInput(const DataPtr cp) : data(cp) {}
	PegexpPointerInput(const PegexpPointerInput& pi) : data(pi.data) {}

	char		get_byte()
			{ return at_eof() ? 0 : (*data++ & 0xFF); }
	PIChar		get_char()
			{ if (sizeof(PIChar) == 1) return get_byte();
			  return (PIChar)(at_eof() ? UCS4_NONE : UTF8Get(data)); }
	bool		at_eof() const
			{ return *data == '\0'; }
	bool		same(PegexpPointerInput& other) const
			{ return data == other.data; }

	size_t		bytes_from(DataPtr origin)
			{ return data - origin; }
	size_t		bytes_from(PegexpPointerInput origin)
			{ return data - origin.data; }
	bool		operator==(const PegexpPointerInput& o)
			{ return o.data == data; }
	const UTF8*	peek() const { return data; }
protected:
	const UTF8*	data;
};

using	PegexpDefaultInput = PegexpPointerInput<>;

template <typename TextPtr = PegexpDefaultInput>
class	NullCapture
{
	NullCapture(const NullCapture&) {}	// May not copy nor assign
	NullCapture&	operator=(const NullCapture& c) { return *this; }
public:
	NullCapture() {}
	int		save(PegexpPC name, TextPtr from, TextPtr to) { return 0; }
	int		count() const { return 0; }
	void		delete_after(int count) {}
};

template <typename TextPtr = PegexpDefaultInput>
class PegState
{
public:
	using 		PChar = typename TextPtr::Char;
	PegState()
			: pc(0), text(0)
			, succeeding(true), at_bol(true), binary_code(false) {}
	PegState(PegexpPC _pc, TextPtr _text)
			: pc(_pc), text(_text)
			, succeeding(true), at_bol(true), binary_code(false) {}
	PegState(const PegState& c)
			: pc(c.pc), text(c.text)
			, succeeding(c.succeeding), at_bol(c.at_bol), binary_code(c.binary_code) {}
	PegState&		operator=(const PegState& c)
			{ pc = c.pc; text = c.text;
			  succeeding = c.succeeding; at_bol = c.at_bol; binary_code = c.binary_code;
			  return *this; }
	operator bool() { return succeeding; }
	PChar		next()
			{
				PChar	ch = binary_code ? text.get_byte() : text.get_char();
				binary_code = false;
				at_bol = ch == '\n';
				return ch;
			}

	PegexpPC	pc;		// Current location in the pegexp
	TextPtr		text;		// Current text we're looking at
	bool		succeeding;	// Is this a viable path to completion?
	bool		at_bol;		// Start of text or line. REVISIT: Should be in TextPtr implementation
	bool		binary_code;	// Expect next character in literal binary coding (not UTF-8, etc)

	// If you want to add debug tracing for example, you can override these in a subclass:
	PegState&	fail() { succeeding = false; return *this; }
	PegState&	progress() { succeeding = true; return *this; }
};

template<typename TextPtrT = PegexpDefaultInput, typename CaptureT = NullCapture<TextPtrT>>
class Pegexp
{
public:
	using 		TextPtr = TextPtrT;
	using		Capture = CaptureT;
	using 		PChar = typename TextPtr::Char;
	using 		State = PegState<TextPtr>;

	PegexpPC	pegexp;

	Pegexp(PegexpPC _pegexp) : pegexp(_pegexp) {}
	PegexpPC	code() const { return pegexp; }

	State		match(TextPtr& start, Capture* capture = 0)
	{
		int	initial_captures = capture ? capture->count() : 0;
		State	state(pegexp, start);
		State	result;
		while (true)
		{
			// Reset for another trial:
			state.pc = pegexp;
			state.text = start;
			state.succeeding = true;
			if (capture)
				capture->delete_after(initial_captures);

			result = match_here(state, capture);
			if (result)
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

	State		match_here(TextPtr& start, Capture* capture = 0)
	{
		State	state(pegexp, start);
		return match_here(state, capture);
	}

	State		match_here(State& state, Capture* capture = 0)
	{
		if (*state.pc == '\0' || *state.pc == ')')
			return state.progress();
		State	r = match_atom(state, capture);
		while (r && *state.pc != '\0' && *state.pc != ')')
			r = match_atom(state, capture);
		return r;
	}

	static int	unhex(PChar c)
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
	PChar		literal_char(PegexpPC& pc)
	{
		PChar		rc = *pc++;
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
		PChar	ch = state.next();
		while (*state.pc != '\0' && *state.pc != ']')
		{
			PChar	c1;

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
				PChar	c2;
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

	bool		char_property(PegexpPC& pc, PChar ch)
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
	virtual State	match_extended(State& state, Capture* capture)
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
	State		match_atom(State& state, Capture* capture)
	{
		int		initial_captures = capture ? capture->count() : 0;
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
				if (!match_atom(state, capture))
					goto fail;
				repetitions++;
			}
			while (max == 0 || repetitions < max)
			{
				int		iteration_captures = capture ? capture->count() : 0;
				TextPtr		repetition_start = state.text;
				state.pc = repeat_pc;
				if (!match_atom(state, capture))
				{
					if (capture)	// Undo new captures on failure of an unmatched iteration
						capture->delete_after(iteration_captures);
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
			if (!match_here(state, capture))
				break;
			if (*state.pc != '\0')		// Don't advance past group if it was not closed
				state.pc++;		// Skip the ')'
			match = true;
			break;

		case '|':	// Alternates
		{
			PegexpPC	next_alternate = state.pc-1;
			int		alternate_captures = capture ? capture->count() : 0;
			while (*next_alternate == '|')	// There's another alternate
			{
				state = start_state;
				state.pc = next_alternate+1;
				// REVISIT: This means failure reports the last alternative. Should it report the first?
				start_state.pc = next_alternate+1;

				// Work through all atoms of the current alternate:
				do {
					if (!match_atom(state, capture))
						break;
				} while (!(match = *state.pc == '\0' || *state.pc == ')' || *state.pc == '|'));

				if (match)
				{		// This alternate matched, skip to the end of the alternates
					while (*state.pc == '|')	// There's another alternate
						skip_atom(state.pc);
					break;
				}
				next_alternate = skip_atom(next_alternate);
				if (capture)	// Undo new captures on failure of an alternate
					capture->delete_after(initial_captures);
			}
			break;
		}

		case '&':	// Positive lookahead assertion
		case '!':	// Negative lookahead assertion
		{
			match = (rc == '!') != (bool)match_atom(state, (Capture*)0);
			state.pc = skip_atom(skip_from);	// Advance state.pc to after the assertion
			state.text = start_state.text;		// We always continue with the original text
			break;
		}

		case '`':
			state.binary_code = true;
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
			match = (state = match_extended(state, capture));
			break;
		}
		if (!match)
		{
	fail:		if (capture)	// Undo new captures on failure
				capture->delete_after(initial_captures);
			return start_state.fail();
		}

		// Detect a label and save the matched atom
		PegexpPC	name = 0;
		if (*state.pc == ':')
		{
			name = ++state.pc;
			while (isalpha(*state.pc) || isdigit(*state.pc) || *state.pc == '_')
				state.pc++;
			if (*state.pc == ':')
				state.pc++;
			if (capture)
				(void)capture->save(name, start_state.text, state.text);
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
