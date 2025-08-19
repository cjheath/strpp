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
 *	\L	lowercase character
 *	\U	uppercase character
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
 * Pegexp and Peg require a Source, which represents a location in a byte stream, and only moves forwards.
 * This is a default example of the API a Source must implement. You can derive from this, or make your own.
 *
 * Adapt DataPtr (a pointer-ish thing) to be a suitable input for a Pegexp,
 * by adding the required methods:
 * - is_null()		Returns false if there is no DataPtr (not just no more data)
 * - get_byte()		Returns the next byte and moves the location forwards
 * - get_char()		Returns the next UCS4 char and moves the location forwards
 * - at_eof()		Returns true of there is no more data available
 * - at_bol()		Returns true at beginning of line, used for the ^ predicate
 * - same()		Returns true if the other Source is at the same location
 *
 * In addition to these requirements, this Source keeps extra data that a Context might want:
 * - byte_count		Number of bytes since the original start of the stream
 * - line_count		Line number of the current location (starts from 1, increases each \n)
 * - column_char	Character number since the start of the current line (or byte, if get_byte was used)
 */
template<
	typename DataPtr = const UTF8*,
	typename _Char = UCS4
>
class	PegexpPointerSource
{
public:
	using Char = _Char;

	// A null Source, used during initialisation or to communicate match failure:
	PegexpPointerSource()
	: data(0)
	, byte_count(0)
	, line_count(1)
	, column_char(1)
	{ }
	bool		is_null() const { return data == 0; }

	// A valid Source, starting at the beginning of the stream
	PegexpPointerSource(const DataPtr cp)
	: data(cp)
	, byte_count(0)
	, line_count(1)
	, column_char(1)
	{ }

	// Copy constructor. A copy does not advance when the origin advances
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
			{ return data == 0 || *data == '\0'; }
	bool		at_bol() const
			{ return column_char == 1; }

	// REVISIT: I'm not sure these definitions are the best way to manage their requirements:
	bool		same(PegexpPointerSource& other) const
			{ return data == other.data; }
	size_t		bytes_from(PegexpPointerSource origin)
			{ return byte_count - origin.byte_count; }
	bool		operator<(PegexpPointerSource& other) { assert(data && other.data || (!data && !other.data)); return data < other.data; }
	off_t		operator-(PegexpPointerSource& other) { assert(data && other.data || (!data && !other.data)); return data - other.data; }

	// These may be used for error reporting. They're not required by Pegexp
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
 * The default Match is just a POD copy of the start and end Source locations.
 * It does not otherwise save the Match or allocate memory.
 */
template <typename _Source = PegexpDefaultSource>
class	PegexpDefaultMatch
{
public:
	using Source = _Source;

	PegexpDefaultMatch() {}	// Default constructor

	// Any subclasses must have this constructor, to capture text matches:
	PegexpDefaultMatch(Source _from, Source _to)
	: from(_from)		// Source of the start of the matched text
	, to(_to)		// Source of the text following the match
	{ }

	// Additional data and constructors may be present in implementations, depending on the Context.

	// A match failure is indicated by a null "to" Source:
	bool		is_failure() const { return to.is_null(); }

	Source		from;
	Source		to;
};

/*
 * An example and default for the API that Pegexp requires for Context.
 * Implement your own template class having this signature to capture matches,
 * or to pass your own context down to a match_extended (as in Peg).
 */
template <typename _Match = PegexpDefaultMatch<>>
class	PegexpDefaultContext
{
public:
	using	Match = _Match;
	using	Source = typename Match::Source;
	PegexpDefaultContext()
	: capture_disabled(0)
	, repetition_nesting(0)
	{}

	// Captures are disabled inside a look-ahead (which can be nested). This holds the nesting count:
	int		capture_disabled;

	// Calls to capture() inside a repetition happen with in_repetition set. This holds the nesting.
	int		repetition_nesting;	// A counter of repetition nesting

	// Every capture gets a capture number, so we can roll it back if needed:
	int		capture_count() const { return 0; }

	// A capture is a named Match. capture() should return the capture_count afterwards.
	int		capture(PegexpPC name, int name_len, Match, bool in_repetition) { return 0; }

	// In some grammars, capture can occur within a failing expression, so we can roll back to a count:
	void		rollback_capture(int count) {}

	// When an atom of a Pegexp fails, the atom (pointer to start and end) and Source location are passed here.
	// Recording this can be useful to see why a Pegexp didn't proceed further, for example.
	void		record_failure(PegexpPC op, PegexpPC op_end, Source location) {}

	// Declare match failure starting at "from". Failure is indicated here by a null "to" Source.
	Match		match_failure(Source at)
			{ return Match(at, Source()); }

	// Success is indicated by a "from" and "to" Source location:
	Match		match_result(Source from, Source to)
			{ return Match(from, to); }
};

template<
	typename _Context = PegexpDefaultContext<PegexpDefaultMatch<PegexpDefaultSource>>
>
class Pegexp
{
public:	// Expose our template types for subclasses to use:
	using		Context = _Context;
	using		Match = typename Context::Match;
	using 		Source = typename Match::Source;
	using 		Char = typename Source::Char;

	// Special characters that should be backslash escaped to use a random string as a literal:
	static	const char*	special;

	// State is used to manage matching progress and backtracking locations.
	class State
	{
	public:
		// Constructor, copy, and assignment:
		State(PegexpPC _pc, Source _text) : pc(_pc), text(_text) {}
		State(const State& c) : pc(c.pc), text(c.text) {}
		State&		operator=(const State& c)
				{ pc = c.pc; text = c.text; return *this; }

		bool		at_expr_end()
				{ return *pc == '\0' || *pc == ')'; }

		PegexpPC	pc;		// Current location in the pegexp
		Source		text;		// Current text we're looking at
	};

	PegexpPC	pegexp;	// The text of the Peg expression: 8-bit data, not UTF-8

	Pegexp(PegexpPC _pegexp) : pegexp(_pegexp) {}

	// Match the Peg expression at or after the start of the source,
	// Advance to the end of the matched text and return the starting offset, or -1 on failure
	Match		match(Source& source, Context* context = 0)
	{
		int	initial_captures = context ? context->capture_count() : 0;
		off_t	offset = 0;

		while (true)
		{
			Source	attempt = source;
			if (context)
				context->rollback_capture(initial_captures);

			Match	match = match_here(attempt, context);
			if (!match.is_failure())
				return match;

			// Move forward one character, or terminate if none available:
			if (source.at_eof())
				break;
			(void)source.get_char();
			offset++;
		}
		return context->match_failure(source);
	}

	Match		match_here(Source& source, Context* context = 0)
	{
		State	state(pegexp, source);

		bool ok = match_sequence(state, context);
		if (ok && *state.pc == '\0')	// An extra ')' can cause match_sequence to succeed incorrectly
		{
			Match	match = context
				? context->match_result(source, state.text)
				: Match(source, state.text);

			// point source at the text following the successful attempt:
			source = state.text;
			return match;
		}
		else
			return context->match_failure(source);
	}

protected:
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
		case 'L':	return islower(ch);
		case 'U':	return isupper(ch);
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
		Char	ch = state.text.get_char();
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

	// Null extension; treat extensions like literal characters
	virtual bool	match_extended(State& state, Context* context)
	{
		return match_literal(state, context);
	}

	bool		match_literal(State& state, Context* context)
	{
		if (state.text.at_eof() || *state.pc != state.text.get_char())
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

		bool		matched = false;
		char		rc;
		switch (rc = *state.pc++)
		{
		case '\0':	// End of expression
			state.pc--;
		case ')':	// End of group
			matched = true;
			break;

		case '^':	// Start of line
			matched = state.text.at_bol();
			break;

		case '$':	// End of line or end of input
			matched = state.text.at_eof() || state.text.get_char() == '\n';
			state.text = start_state.text;
			break;

		case '.':	// Any character
			if ((matched = !state.text.at_eof()))
				(void)state.text.get_char();
			break;

		default:	// Literal character
			if (rc > 0 && rc < ' ')		// Control characters
				goto extended;
			matched = !state.text.at_eof() && rc == state.text.get_char();
			break;

		case '\\':	// Escaped literal char
			matched = !state.text.at_eof() && char_property(state.pc, state.text.get_char());
			break;

		case '[':	// Character class
			matched = char_class(state);
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
			matched = true;
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
			matched = true;
			break;

		case '|':	// Alternates
		{
			PegexpPC	next_alternate = state.pc-1;
			int		alternate_captures = context ? context->capture_count() : 0;
			while (*next_alternate == '|')	// There's another alternate
			{
				state = start_state;
				state.pc = next_alternate+1;
				start_state.pc = next_alternate+1;

				// Work through all atoms of the current alternate:
				do {
					if (!match_atom(state, context))
						break;
				} while (!(matched = state.at_expr_end() || *state.pc == '|'));

				if (matched)
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
			matched = match_atom(state, context);
			if (rc == '!')
				matched = !matched;
			if (context)
				context->capture_disabled--;

			state = start_state;	// Continue with the same text
			if (matched)	// Assertion succeeded, skip it
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
			matched = match_extended(state, context);
			break;
		}
		if (!matched)
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
				Match	r(start_state.text, state.text);

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

template<typename Context> const char* Pegexp<Context>::special = "^$.\\[]?*+{()}|&!~@#%_;<`:";

#endif	// PEGEXP_H
