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
 *	.	any character, including a newline
 *	?	Zero or one of the following expression
 *	*	Zero or more of the following expression
 *	+	One or more of the following expression
 *	(expr)	Group subexpressions (does not imply capture)
 *	|A|B...	Either A or B (or ...)
 *	&A	Continue only if A succeeds
 *	!A	Continue only if A fails
 *	anychar	match that non-operator character
 *	\char	match the escaped character (including the operators, 0 b e f n r t, and any other char)
 *	\a	alpha
 *	\d	digit
 *	\h	hexadecimal
 *	\s	whitespace
 *	\w	word (alpha or digit)
 *	\177	match the specified octal character
 *	\xXX	match the specified hexadecimal (0-9a-fA-F)
 *	\x{1-2}	match the specified hexadecimal (0-9a-fA-F)
 *	\u12345	match the specified 1-5 digit Unicode character (only if compiled for Unicode support)
 *	\u{1-5}	match the specified 1-5 digit Unicode character (only if compiled for Unicode support)
 *	[a-z]	Normal character class (a literal hyphen may occur at start)
 *	[^a-z]	Negated character class. Characters may include the \escapes listed above
 *	! ` @ # % _ ; : <	Call the extended_match function
 *	control-character	Call the extended_match function
 * NOT YET IMPLEMENTED:
 *	{n,m}	match from n (default 0) to m (default unlimited) repetitions of the following expression.
 *	Captures.
 *
 * Note: Possessive alternates and possessive repetition will never backtrack.
 * Once an alternate has matched, no subsequent alterative will be tried in that group.
 * Once a repetition has been made, it will never be unwound.
 * It is your responsibility to ensure these possessive operators never match unless it's final.
 * You should use negative assertions to control inappropriate greed.
 *
 * You can use any scalar data type for Char, and a pointer to it for TextPtr (even wrapping a socket).
 * You can subclass Pegexp to override match_extended&skip_extended to handle special command characters.
 * You can replace the default NullCapture with a capture object with features for your extended command chars.
 */
#include	<stdint.h>
#include	<stdlib.h>
#include	<ctype.h>
#include	<refcount.h>

typedef	const char*	PegexpPC;

/*
 * Wrap a const char* to adapt it as a PEG parser input.
 * Every operation here works as for a char*, but it also supports at_eof()/at_bot() to detect the ends.
 * More pointer-ish methods are possible, but this is limited to the methods needed for Pegexp.
 */
class	TextPtrChar
{
private:
	const char*	data;
	const char*	origin;
public:
	TextPtrChar() : data(0), origin(0) {}				// Null constructor
	TextPtrChar(const char* s) : data(s), origin(s) {}			// Normal constructor
	TextPtrChar(TextPtrChar& c) : data(c.data), origin(c.origin) {}		// Copy constructor
	TextPtrChar(const TextPtrChar& c) : data(c.data), origin(c.origin) {}	// Copy constructor
	~TextPtrChar() {};
	TextPtrChar&		operator=(TextPtrChar s)	// Assignment
				{ data = s.data; origin = s.origin; return *this; }
	operator const char*()					// Access the char under the pointer
				{ return static_cast<const char*>(data); }
	char			operator*()			// Dereference to char under the pointer
				{ return *data; }

	bool			at_eof() { return *data == '\0'; }
	bool			at_bot() { return data == origin; }
	TextPtrChar		operator++(int)	{ return data++; }
};

class	NullCapture
: public RefCounted
{
public:
	NullCapture() {}
};

template <typename TextPtr = TextPtrChar, typename Char = char, typename Capture = NullCapture>
class PegState
{
public:
	PegState()
		: pc(0), text(0), succeeded(true), capture(0) {}
	PegState(PegexpPC _pc, TextPtr _text, Ref<Capture> _capture)
		: pc(_pc), text(_text), succeeded(true), capture(_capture) {}
	PegState(const PegState& c)
		: pc(c.pc), text(c.text), succeeded(c.succeeded), capture(c.capture) {}
	PegState&		operator=(const PegState& c)
		{ pc = c.pc; text = c.text; succeeded = c.succeeded; capture = c.capture; return *this; }
	operator bool() { return succeeded; }

	PegexpPC	pc;		// Current location in the pegexp
	TextPtr		text;		// Current text we're looking at
	bool		succeeded;	// Is this a viable path to completion?
	Ref<Capture>   	capture;	// What did we learn while getting here?

	PegState&	fail() { succeeded = false; return *this; }
	PegState&	succeed() { return *this; }
};

template<typename TextPtr = TextPtrChar, typename Char = char, typename Capture = NullCapture, typename S = PegState<TextPtr, Char, Capture>>
class Pegexp
{
public:
	using		State = S;
	PegexpPC	pegexp;

	Pegexp(PegexpPC _pegexp) : pegexp(_pegexp) {}
	PegexpPC	code() const { return pegexp; }

	State		match(TextPtr& text)
	{
		State	state(pegexp, text, 0);
		State	result;
		do {
			// Reset for another trial:
			state.pc = pegexp;
			state.text = text;
			state.succeeded = true;

			result = match_here(state);
			if (result)
			{		// Succeeded
				if (*result.pc != '\0')	// The pegexp has a syntax error, e.g. an extra )
					return result.fail();
				return result;
			}
		} while (!text.at_eof() && text++);
		return result;	// This will show where we last failed
	}

	State		match_here(TextPtr& text)
	{
		State	state(pegexp, text, 0);
		return match_here(state);
	}

	State		match_here(State& state)
	{
		if (*state.pc == '\0' || *state.pc == ')')
			return state.succeed();
		State	r = match_atom(state);
		while (r && *state.pc != '\0' && *state.pc != ')')
			r = match_atom(state);
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
		bool	negated;
		State	start_state(state);
		start_state.pc--;
		if ((negated = (*state.pc == '^')))
			state.pc++;

		bool	in_class = false;
		while (*state.pc != '\0' && *state.pc != ']')
		{
			Char	c1;

			// Handle actual properties, not other escapes
			if (*state.pc == '\\' && isalpha(state.pc[1]))
			{
				state.pc++;
				if (char_property(state.pc, *state.text))
					in_class = true;
				continue;
			}

			c1 = literal_char(state.pc);
			if (*state.pc == '-')
			{
				Char	c2;
				state.pc++;
				c2 = literal_char(state.pc);
				in_class |= (*state.text >= c1 && *state.text <= c2);
			}
			else
				in_class |= (*state.text == c1);
		}
		if (*state.pc == ']')
			state.pc++;
		if (negated)
			in_class = !in_class;
		if (state.text.at_eof())	// End of input never matches
			in_class = false;
		if (in_class)
			state.text++;
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
	virtual State	match_extended(State& state)
	{
		return match_literal(state);
	}

	State		match_literal(State& state)
	{
		if (state.text.at_eof() || *state.pc != *state.text)
			return state.fail();
		state.text++;
		state.pc++;
		return state.succeed();
	}

	/*
	 * The guts of the algorithm. Match one atom against the current text.
	 *
	 * If this returns true, state.pc has been advanced to the next atom,
	 * and state.text has consumed any input that matched.
	 *
	 * If it returns false, state.pc has still been advanced but state.text is unchanged
	 */
	State		match_atom(State& state)
	{
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
			match = state.text.at_bot() ? true : state.text[-1] == '\n';
			break;

		case '$':	// End of line or end of input
			match = state.text.at_eof() || *state.text == '\n';
			break;

		case '.':	// Any character
			if ((match = !state.text.at_eof()))
				state.text++;
			break;

		default:	// Literal character
			if (rc > 0 && rc < ' ')		// Control characters
				goto extended;
			if ((match = (!state.text.at_eof() && rc == *state.text)))
				state.text++;
			break;

		case '\\':	// Escaped literal char
			if ((match = (!state.text.at_eof() && char_property(state.pc, *state.text))))
				state.text++;
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
				if (!match_atom(state))
					goto fail;
				repetitions++;
			}
			while (max == 0 || repetitions < max)
			{
				TextPtr		repetition_start = state.text;
				state.pc = repeat_pc;
				if (!match_atom(state))
				{
					// Ensure that state.pc is now pointing at the following expression
					state.pc = repeat_pc;
					state.text = repetition_start;
					skip_atom(state.pc);
					break;
				}
				if (state.text == repetition_start)
					break;	// We didn't advance, so don't keep trying. Can happen e.g. on *()
				repetitions++;
			}
			match = true;
			break;
		}

		case '(':	// Parenthesised group
			start_state.pc = state.pc;
			if (!match_here(state))
				break;
			if (*state.pc != '\0')		// Don't advance past group if it was not closed
				state.pc++;		// Skip the ')'
			match = true;
			break;

		case '|':	// Alternates
		{
			PegexpPC	next_alternate = state.pc-1;
			while (*next_alternate == '|')	// There's another alternate
			{
				state = start_state;
				state.pc = next_alternate+1;
				// REVISIT: This reports failure of the last alternative. Should it report the first?
				start_state.pc = next_alternate+1;

				// Work through all atoms of the current alternate:
				do {
					if (!match_atom(state))
						break;
				} while (!(match = *state.pc == '\0' || *state.pc == ')' || *state.pc == '|'));

				if (match)
				{		// This alternate matched, skip to the end of the alternates
					while (*state.pc == '|')	// There's another alternate
						skip_atom(state.pc);
					break;
				}
				next_alternate = skip_atom(next_alternate);
			}
			break;
		}

		case '&':	// Positive lookahead assertion
		case '!':	// Negative lookahead assertion
		{
			match = (rc == '!') != (bool)match_atom(state);
			state.pc = skip_atom(skip_from);	// Advance state.pc to after the assertion
			state.text = start_state.text;		// We always continue with the original text
			break;
		}

		// Extended commands:
		case '~':
		case '`':
		case '@':
		case '#':
		case '%':
		case '_':
		case ';':
		case ':':
		case '<':
		extended:
			state.pc--;
			return match_extended(state);
		}
	fail:
		return match ? state.succeed() : start_state.fail();
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
		case ':':
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
		return pc;
	}
};

#endif	// PEGEXP_H
