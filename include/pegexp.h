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
 * NOT YET IMPLEMENTED:
 *	{n,m}	match from n (default 0) to m (default unlimited) repetitions of the following expression.
 *	<name>	Call the callout function passing the specified name.
 *	Captures.
 *
 * Note: Possessive alternates and possessive repetition will never backtrack.
 * Once an alternate has matched, no subsequent alterative will be tried in that group.
 * Once a repetition has been made, it will never be unwound.
 * It is your responsibility to ensure these possessive operators never match unless it's final.
 * You should use negative assertions to control inappropriate greed.
 */
#include	<stdint.h>
#include	<stdlib.h>
#include	<ctype.h>

typedef	const char*	PegexpPC;

/*
 * Wrap a const char* to adapt it as a PEG parser input.
 * Every operation here works as for a char*, but it also supports at_eof() to detect the terminator.
 */
class	TextPtrChar
{
private:
	const char*	data;
public:
	TextPtrChar() : data(0) {}				// Null constructor
	TextPtrChar(const char* s) : data(s) {}			// Normal constructor
	TextPtrChar(TextPtrChar& c) : data(c.data) {}		// Copy constructor
	TextPtrChar(const TextPtrChar& c) : data(c.data) {}	// Copy constructor
	~TextPtrChar() {};
	TextPtrChar&		operator=(const char* s)	// Assignment
				{ data = s; return *this; }
	operator const char*()					// Access the char under the pointer
				{ return static_cast<const char*>(data); }
	char			operator*()			// Dereference to char under the pointer
				{ return *data; }
	bool			at_eof() { return **this == '\0'; }
	TextPtrChar		operator++(int)	{ return data++; }
};

template<typename TextPtr = TextPtrChar, typename Char = char>
class Pegexp
{
public:
	PegexpPC		pegexp;

	Pegexp(PegexpPC _pegexp) : pegexp(_pegexp) {}
	PegexpPC		code() const { return pegexp; }

	struct State;
	typedef	bool 		(*CalloutFn)(State& state);
	struct State {
		PegexpPC	pc;		// Current location in the pegexp
		TextPtr		text;		// Current text we're looking at
		TextPtr		target;		// Original start of the string
		CalloutFn	callout;
		void*		closure;	// User context for the callout
	};

	int		match(TextPtr& text, CalloutFn callout = 0, void* closure = 0)
	{
		TextPtr	trial = text;		// The first search starts here
		State	state = {
				.pc = pegexp,		// The expression to match
				.text = text,		// Start point in text
				.target = text,		// Needed to stop ^ referencing text[-1]
				.callout = callout,	// Where to send callouts, when implemented
				.closure = closure
			};
		do {
			state.pc = pegexp;
			state.text = trial;

			if (match_here(state))
			{		// The matching section is between trial and state.text
				if (*state.pc != '\0')	// The pegexp has a syntax error, e.g. an extra )
					break;

				int	length = state.text - trial;
				text = state.text-length;
				return length;
			}
		} while (!trial.at_eof() && trial++);
		// pegexp points to the part of the expression that failed. Is this useful?
		text = 0;
		return -1;
	}

	int		match_here(TextPtr& text, CalloutFn callout = 0, void* closure = 0)
	{
		State	state = {
				.pc = pegexp,		// The expression to match
				.text = text,		// Start point in text
				.target = text,		// Needed to stop ^ referencing text[-1]
				.callout = callout,	// Where to send callouts, when implemented
				.closure = closure
			};
		bool success = match_here(state);
		if (!success)
			return -1;
		int	length = state.text - state.target;
		state.text = text;
		return length;
	}

	bool		match_here(State& state)
	{
		if (*state.pc == '\0' || *state.pc == ')')
			return true;
		do {
			if (!match_atom(state))
				return false;
		} while (*state.pc != '\0' && *state.pc != ')');
		return true;
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

	/*
	 * The guts of the algorithm. Match one atom against the current text.
	 *
	 * If this returns true, state.pc has been advanced to the next atom,
	 * and state.text has consumed any input that matched.
	 *
	 * If it returns false, state.pc has still been advanced but state.text is unchanged
	 */
	bool		match_atom(State& state)
	{
		PegexpPC		skip_from = state.pc;
		switch (char rc = *state.pc++)
		{
		case '\0':	// End of expression
			state.pc--;
		case ')':	// End of group
			return true;

		case '^':	// Start of line
			return state.text == state.target || state.text[-1] == '\n';

		case '$':	// End of line or end of input
			return state.text.at_eof() || *state.text == '\n';

		case '.':	// Any character
			if (state.text.at_eof())
				return false;		// No more data available
			state.text++;
			return true;

		default:	// Literal character
			if (state.text.at_eof() || rc != *state.text)
				return false;
			state.text++;
			return true;

		case '\\':	// Escaped literal char
			if (state.text.at_eof() || !char_property(state.pc, *state.text))
				return false;
			state.text++;
			return true;

		case '[':	// Character class
			return char_class(state);

		case '?':	// Zero or one
		case '*':	// Zero or more
		case '+':	// One or more
			return match_repeat(state, rc == '+' ? 1 : 0, rc == '?' ? 1 : 0);
		// case '{':	// REVISIT: Implement counted repetition

		case '(':	// Parenthesised group
			if (!match_here(state))
			{
				state.pc = skip_atom(skip_from);	// Advance state.pc to after the ')' so repetition works
				return false;
			}
			if (*state.pc != '\0')		// Don't advance past group if it was not closed
				state.pc++;		// Skip the ')'
			return true;

		case '|':	// Alternates
		{
			state.pc--;
			const TextPtr	revert = state.text;	// We always return to the current text
			while (*state.pc == '|')	// There's another alternate
			{
				state.pc++;
				state.text = revert;
				if (match_alternate(state))
				{		// This alternate matched, skip to the end of these alternates
					while (*state.pc == '|')	// We reached the next alternate
						skip_atom(state.pc);
					return true;
				}
				// If we want to know the furthest text looked at, we should save that from state.text here.
				state.pc = skip_atom(skip_from);
			}
			return false;
		}

		case '&':	// Positive lookahead assertion
		case '!':	// Negative lookahead assertion
		{
			const TextPtr	revert = state.text;	// We always return to the current text
			bool		succeed = (rc == '!') != match_atom(state);
			state.pc = skip_atom(skip_from);	// Advance state.pc to after the assertion
			state.text = revert;
			return succeed;
		}

		case '<':	// Implement callouts
			return state.callout(state);
		}
	}

	bool		match_alternate(State& state)
	{
		do {
			if (!match_atom(state))
				return false;
		} while (*state.pc != '\0' && *state.pc != ')' && *state.pc != '|');
		return true;
	}

	bool		match_repeat(State& state, int min, int max)
	{
		TextPtr		iter_start;
		const PegexpPC	repeat_pc = state.pc;
		int		repetitions = 0;

		while (repetitions < min)
		{
			state.pc = repeat_pc;
			iter_start = state.text;
			if (!match_atom(state))
			{
				state.text = iter_start;
				return false;
			}
			repetitions++;
		}
		while (max == 0 || repetitions < max)
		{
			iter_start = state.text;
			state.pc = repeat_pc;
			if (!match_atom(state))
			{
				// Ensure that state.pc is now pointing at the following expression
				state.pc = repeat_pc;
				skip_atom(state.pc);
				state.text = iter_start;
				return true;
			}
			if (state.text == iter_start)
				break;	// We didn't advance, so don't keep trying. Can happen e.g. on *()
			repetitions++;
		}
		return true;
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

		case '<':	// Callout
			while (*pc != '\0' && *pc++ != '>')
				;
			break;

		default:
			break;

		// As-yet unimplemented features:
		// case '{':	// REVISIT: Implement counted repetition
		// Other unused special characters: ~`@#%_;:
		}
		return pc;
	}
};

#endif	// PEGEXP_H
