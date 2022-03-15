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
#include	<ctype.h>

typedef	const char*	PegexpPC;

template<typename TextPtr = const char*, typename Char = char>
class Pegexp {
	PegexpPC		pegexp;
	bool			IsAlpha(char c) { return isalpha(c); }
	bool			IsDigit(char c) { return isdigit(c); }

public:
	Pegexp(PegexpPC _pegexp) : pegexp(_pegexp) {}

	struct State {
		PegexpPC	pc;		// Current location in the pegexp
		TextPtr		text;		// Current text we're looking at
		TextPtr		target;		// Original start of the string
		bool		(*callout)(PegexpPC pegexp, TextPtr& text);
	};
	typedef	bool 	(*callout_fn)(PegexpPC pegexp, TextPtr& text);

	int		match(TextPtr& text, callout_fn callout = 0)
	{
		TextPtr	trial = text;		// The first search starts here
		State	state = {
				.pc = pegexp,		// The expression to match
				.text = text,		// Start point in text
				.target = text,		// Needed to stop ^ referencing text[-1]
				.callout = callout	// Where to send callouts, when implemented
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
		} while (*trial++ != '\0');
		// pegexp points to the part of the expression that failed. Is this useful?
		text = 0;
		return -1;
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
	Char		escaped_char(PegexpPC& pc)
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
			c1 = escaped_char(state.pc);
			if (*state.pc == '-')
			{
				Char	c2;
				state.pc++;
				c2 = escaped_char(state.pc);
				in_class |= (*state.text >= c1 && *state.text <= c2);
			}
			else
				in_class |= (*state.text == c1);
		}
		if (*state.pc == ']')
			state.pc++;
		if (negated)
			in_class = !in_class;
		if (*state.text == '\0')	// End of string never matches
			in_class = false;
		if (in_class)
			state.text++;
		return in_class;
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
		switch (uint32_t rc = *state.pc++)
		{
		case '\0':	// End of expression
			state.pc--;
		case ')':	// End of group
			return true;

		case '^':	// Start of line
			return state.text == state.target || state.text[-1] == '\0';

		case '$':	// End of line or end of state.text
			return *state.text == '\0' || *state.text == '\n';

		case '.':	// Any character
			if ('\0' == *state.text)
				return false;		// No more chars are available
			state.text++;
			return true;

		default:	// Literal character
			if (rc != *state.text)
				return false;
			state.text++;			// Cannot be the terminating NUL because rc != '\0'
			return true;

		case '\\':	// Escaped literal char
			state.pc--;
			rc = escaped_char(state.pc);
			switch (rc)
			{
			case 'a':			// Alphabetic
				if (!IsAlpha(*state.text))
					return false;
				break;
			case 'd':			// Digit
				if (!IsDigit(*state.text))
					return false;
				break;
			case 'h':			// Hexadecimal
				if (!IsDigit(*state.text) && !(*state.text >= 'a' && *state.text <= 'f') && !(*state.text >= 'A' && *state.text <= 'F'))
					return false;
				break;
			case 'w':			// Alphabetic or digit
				if (!IsAlpha(*state.text) && !IsDigit(*state.text))
					return false;
				break;
			default:			// Other escaped character, exact match
				if (rc != *state.text)
					return false;
			}

			if (*state.text != '\0')	// Don't run past the terminating NUL, even if it matches
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
			do {
				if (!match_alternate(state))
				{		// This alternate failed, find the next
					state.pc = skip_atom(skip_from);
					continue;
				}
				// This alternate matched, skip to the end of these alternates
				while (*state.pc == '|')
					skip_atom(state.pc);
				return true;
			} while (*state.pc == '|');	// There's another alternate
			return false;

		case '&':	// Positive lookahead assertion
		case '!':	// Negative lookahead assertion
		{
			const TextPtr	revert = state.text;	// We always return to the current text
			bool		succeed = (rc == '!') != match_atom(state);
			state.pc = skip_atom(skip_from);	// Advance state.pc to after the assertion
			state.text = revert;
			return succeed;
		}

		// case '<':	// REVISIT: Implement callouts
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
		switch (*pc++)
		{
		case '\\':	// Escaped literal char
			(void)escaped_char(pc);
			break;

		case '[':	// Skip char class
			if (*pc == '^')
				pc++;
			while (*pc != '\0' && *pc != ']')
			{
				(void)escaped_char(pc);
				if (*pc == '-')
				{
					pc++;
					(void)escaped_char(pc);
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

		default:
			break;

		// As-yet unimplemented features:
		// case '{':	// REVISIT: Implement counted repetition
		}
		return pc;
	}
};
#endif	// PEGEXP_H
