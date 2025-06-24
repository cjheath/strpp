#if !defined(PEGEXP_H)
#define PEGEXP_H
/*
 * Pegular expressions, aka Pegexp, formally "regular PEGs".
 *
 * Possessive regular expressions using prefix operators
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
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
 *	! @ # % _ ; < `		Call the match_extended function
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
 */
#include	<unistd.h>

#include	<cstdint>
#include	<cstdlib>
#include	<cstring>
#include	<cctype>
#include	<char_encoding.h>

typedef	const char*	PegexpPC;	// The Pegexp is always 8-bit, not UTF-8

/*
 * Adapt a pointer-ish thing to be a suitable input for a Pegexp,
 * by adding the methods get_byte(), get_char(), same() and at_eof().
 * same() should return true if the two Inputs are at the same position in the text.
 * at_bol() is used for the ^ predicate. It's assumed that the start of the string is bol.
 */
template<
	typename DataPtr = const UTF8*,
	typename _Char = UCS4
>
class	PegexpPointerSource
{
public:
	using Char = _Char;

	PegexpPointerSource()
	: data(0)
	, byte_count(0)
	, line_count(1)
	, column_char(1)
	{ }

	PegexpPointerSource(const DataPtr cp)
	: data(cp)
	, byte_count(0)
	, line_count(1)
	, column_char(1)
	{ }

	PegexpPointerSource(const PegexpPointerSource& pi)
	: data(pi.data)
	, byte_count(pi.byte_count)
	, line_count(pi.line_count)
	, column_char(pi.column_char)
	{ }

	char		get_byte()
			{
				if (at_eof()) return 0;
				char c = (*data++ & 0xFF);
				byte_count++;
				bump_counts(c);
				return c;
			}
	Char		get_char()
			{
				if (sizeof(Char) == 1) return get_byte();
				if (at_eof()) return UCS4_NONE;
				const UTF8*	start = data;
				Char	c = UTF8Get(data);
				byte_count += data-start;
				bump_counts(c);
				return c;
			}
	bool		at_eof() const
			{ return *data == '\0'; }
	bool		at_bol() const
			{ return column_char == 1; }
	bool		same(PegexpPointerSource& other) const
			{ return data == other.data; }
	size_t		bytes_from(PegexpPointerSource origin)
			{ return byte_count - origin.byte_count; }

	// These may be used for error reporting:
	off_t		current_byte() const { return byte_count; }
	off_t		current_line() const { return line_count; }
	off_t		current_column() const { return column_char; }	// In Chars

protected:
	const UTF8*	data;		// Pointer to the next byte of data. \0 indicates EOF
	off_t		byte_count;	// Total bytes traversed
	off_t		line_count;	// Incremented from 1 after each \n
	off_t		column_char;	// Reset to one after \n, incremented on get_byte or get_char

	void		bump_counts(Char c)
			{
				if (c == '\n')
				{
					line_count++;
					column_char = 1;
				}
				else
					column_char++;
			}
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
	PegexpDefaultResult(Source _from, Source _to)
	: from(_from)		// pointer to start of the text matched by this atom
	, to(_to)		// pointer to the text following the match
	{ }
	PegexpDefaultResult() {}

	Source		from;
	Source		to;
};

/*
 * An example of the API that Pegexp requires for Context.
 * Implement your own template class having this signature to capture match fragments,
 * or to pass your own context down to match_extended
 */
template <typename _Result = PegexpDefaultResult<>>
class	PegexpNullContext
{
public:
	using	Result = _Result;
	using	Source = typename Result::Source;
	PegexpNullContext() : capture_disabled(0), repetition_nesting(0) {}

	int		capture(PegexpPC name, int name_len, Result, bool in_repetition) { return 0; }
	int		capture_count() const { return 0; }
	void		rollback_capture(int count) {}
	int		capture_disabled;	// A counter of nested disables

	void		record_failure(PegexpPC op, PegexpPC op_end, Source location) {}

	int		repetition_nesting;	// A counter of repetition nesting
	Result		result() const { return Result(); }
};

template <typename Source = PegexpDefaultSource>
class PegexpState
{
public:
	using 		Char = typename Source::Char;
	PegexpState()	: pc(0), text(0) {}
	PegexpState(PegexpPC _pc, Source _text) : pc(_pc), text(_text) {}
	PegexpState(const PegexpState& c) : pc(c.pc), text(c.text) {}
	PegexpState&	operator=(const PegexpState& c)
			{ pc = c.pc; text = c.text; return *this; }
	char		next_byte()	// May be used by an extension
			{ return text.get_byte(); }
	Char		next_char()
			{ return text.get_char(); }
	bool		at_bol()
			{ return text.at_bol(); }
	bool		at_expr_end()
			{ return *pc == '\0' || *pc == ')'; }

	PegexpPC	pc;		// Current location in the pegexp
	Source		text;		// Current text we're looking at
};

template<
	typename _Context = PegexpNullContext<PegexpDefaultResult<PegexpDefaultSource>>
>
class Pegexp
{
public:	// Expose our template types for subclasses to use:
	using		Context = _Context;
	using		Result = typename Context::Result;
	using 		Source = typename Result::Source;
	using 		Char = typename Source::Char;
	using 		State = PegexpState<Source>;

	PegexpPC	pegexp;	// The text of the Peg expression: 8-bit data, not UTF-8

	Pegexp(PegexpPC _pegexp) : pegexp(_pegexp) {}

	// Match the Peg expression at or after the start of the source,
	// Advance to the end of the matched text and return the starting offset, or -1 on failure
	off_t		match(Source& source, Context* context = 0)
	{
		int	initial_captures = context ? context->capture_count() : 0;
		off_t	offset = 0;
		State	attempt(pegexp, source);

		while (true)
		{
			attempt = State(pegexp, source);	// Reset for another trial
			if (context)
				context->rollback_capture(initial_captures);

			bool result = match_sequence(attempt, context);
			if (result)
			{
				// Set the Source to the text following the successful attempt
				source = attempt.text;

				// If the pegexp has an extra ), fail
				if (*attempt.pc != '\0')
					return -1;
				return offset;
			}

			// Move forward one character, or terminate if none available:
			if (source.at_eof())
				break;
			(void)source.get_char();
			offset++;
		}
		source = attempt.text;	// Set the Source to where we last failed
		return -1;
	}

	bool		match_sequence(Source& source, Context* context = 0)
	{
		State	state(pegexp, source);
		return match_sequence(state, context);
	}

	bool		match_sequence(State& state, Context* context = 0)
	{
		if (state.at_expr_end())
			return true;	// We matched nothing, successfully

		int		sequence_capture_start = context ? context->capture_count() : 0;

		bool	ok = match_atom(state, context);
		while (ok && !state.at_expr_end())
			ok = match_atom(state, context);

		// If we didn't get to the end of the sequence, rollback any captures in the incomplete sequence:
		if (context && !ok)
			context->rollback_capture(sequence_capture_start);
		return ok;
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
	// The Pegexp wants to match a single Char, so extract it from various representations:
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

	bool		char_property(PegexpPC& pc, Char ch)
	{
		switch (char32_t esc = *pc++)
		{
		case 'a':	return isalpha(ch);	// Alphabetic
		case 'd':	return isdigit(ch);	// Digit
		case 'h':	return isdigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');	// Hexadecimal
		case 's':	return isspace(ch);
		case 'w':	return isalnum(ch);	// Alphabetic or digit
		default:	pc -= 2;		// oops, not a property
				esc = literal_char(pc);
				return esc == ch;	// Other escaped character, exact match
		}
	}

	bool		char_class(State& state)
	{
		if (state.text.at_eof())		// End of input never matches a char_class
			return false;

		State	start_state(state);
		start_state.pc--;

		bool	negated;
		if ((negated = (*state.pc == '^')))
			state.pc++;

		bool	in_class = false;
		Char	ch = state.next_char();
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
		if (!in_class)
			state = start_state;
		return in_class;
	}

	// Null extension; treat extensions like literal characters
	virtual bool	match_extended(State& state, Context* context)
	{
		return match_literal(state, context);
	}

	bool		match_literal(State& state, Context* context)
	{
		if (state.text.at_eof() || *state.pc != state.next_char())
			return false;
		state.pc++;
		return true;
	}

	/*
	 * The guts of the algorithm. Match one atom against the current text.
	 *
	 * If this returns true, state.pc has been advanced to the next atom,
	 * and state.text has consumed any input that matched.
	 */
	bool		match_atom(State& state, Context* context)
	{
		int		initial_captures = context ? context->capture_count() : 0;
		State		start_state(state);

		bool		match = false;
		char		rc;
		switch (rc = *state.pc++)
		{
		case '\0':	// End of expression
			state.pc--;
		case ')':	// End of group
			match = true;
			break;

		case '^':	// Start of line
			match = state.at_bol();
			break;

		case '$':	// End of line or end of input
			match = state.text.at_eof() || state.next_char() == '\n';
			state.text = start_state.text;
			break;

		case '.':	// Any character
			if ((match = !state.text.at_eof()))
				(void)state.next_char();
			break;

		default:	// Literal character
			if (rc > 0 && rc < ' ')		// Control characters
				goto extended;
			match = !state.text.at_eof() && rc == state.next_char();
			break;

		case '\\':	// Escaped literal char
			match = !state.text.at_eof() && char_property(state.pc, state.next_char());
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
			const PegexpPC	repeat_pc = state.pc;		// This is the code we'll repeat
			State		iteration_start = state;	// revert here on iteration failure
			int		repetitions = 0;

			if (context && max != 1)
				context->repetition_nesting++;

			// Attempt the minimum iterations:
			while (repetitions < min)
			{
				state.pc = repeat_pc;	// We run the same atom more than once
				if (!match_atom(state, context))
					goto repeat_fail;	// We didn't meet the minimum repetitions
				repetitions++;
			}

			// Continue up to max iterations:
			while (max == 0 || repetitions < max)
			{
				int		iteration_captures = context ? context->capture_count() : 0;
				iteration_start = state;
				state.pc = repeat_pc;
				if (!match_atom(state, context))
				{
					if (context)	// Undo new captures on failure of an unmatched iteration
						context->rollback_capture(iteration_captures);
					// printf("iterated %d (>=min %d <=max %d)\n", repetitions, min, max);
					skip_atom(state.pc);	// We're done with these repetitions, move on
					break;
				}
				if (state.text.same(iteration_start.text))
					break;	// We didn't advance, so don't keep trying. Can happen e.g. on *()
				repetitions++;
			}
			match = true;
	repeat_fail:	if (context && max != 1)
				context->repetition_nesting--;
			break;
		}

		case '(':	// Parenthesised group
			start_state.pc = state.pc-1;	// Fail back to the start of the group
			if (!match_sequence(state, context))
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
					if (!match_atom(state, context))
						break;
				} while (!(match = state.at_expr_end() || *state.pc == '|'));

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
			match = match_atom(state, context);
			if (rc == '!')
				match = !match;
			if (context)
				context->capture_disabled--;

			state = start_state;	// Continue with the same text
			if (match)	// Assertion succeeded, skip it
				state.pc = skip_atom(state.pc);
			break;
		}

		// Extended commands:
		case '~':
		case '@':
		case '#':
		case '%':
		case '_':
		case ';':
		case '<':
		case '`':
		extended:
			state.pc--;
			match = match_extended(state, context);
			break;
		}
		if (!match)
		{
			if (context)	// Undo new captures on failure
				context->rollback_capture(initial_captures);

			/*
			 * If this is the furthest point reached in the text so far,
			 * record the rule that was last attempted. Multiple rules may be
			 * attempted at this point, we want to know what characters would
			 * have allowed the parse to proceed.
			 */
			if (context		// We need something to report to
			 && (char*)0 == strchr("?*+(|&!", rc))		// Don't report composite operators
			{
				/*
				 * Terminal symbols for which we can report failure:
				 * ^ = beginning of line
				 * $ = end of line
				 * !. = EOF
				 * . = any character
				 * [...] = character class
				 * \char property
				 * \literal character
				 * Otherwise it's a literal character
				 * (reporting might also show subsequent literals in a string)
				 */
				context->record_failure(
					start_state.pc,		// The PegexPC of the operator
					state.pc,		// The PegexPC of the end of the operator
					start_state.text	// The Source location
				); // ... record only if state.text >= context->furthest_success
			}

			state = start_state;
			return false;
		}

		// Detect a label and save the matched atom
		PegexpPC	name = 0;
		if (*state.pc == ':')
		{
			name = ++state.pc;
			while (isalnum(*state.pc) || *state.pc == '_')
				state.pc++;
			PegexpPC	name_end = state.pc;
			if (*state.pc == ':')
				state.pc++;
			if (context && context->capture_disabled == 0)
			{
				Result r(start_state.text, state.text);

				(void)context->capture(
					name, name_end-name,
					r, context->repetition_nesting > 0
				);
			}
		}
		return true;
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
		case '`':	// Perhaps used for binary codes?
		case '@':
		case '#':
		case '%':
		case '_':
		case ';':
		case '<':	// often used for recursive rule calls
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
			while (isalnum(*pc) || *pc == '_')
				pc++;
			if (*pc == ':')
				pc++;
		}
		return pc;
	}
};

#endif	// PEGEXP_H
