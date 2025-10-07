#include	<stdio.h>

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
 *	\u1234	match the specified 1-4 digit Unicode character (only if compiled for Unicode support)
 *	\u{1-8}	match the specified 1-8 digit Unicode character (only if compiled for Unicode support)
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

typedef	const char*	PatternP;	// The Pegexp is always 8-bit, not UTF-8

/*
 * Pegexp and Peg require a Source, which represents a location in a byte stream, and only moves forwards.
 * A Source may be copied. Each copy will re-read the same data as it moves forward. This is how Pegexp backtracks.
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
	bool		same(const PegexpPointerSource& other) const
			{ return data == other.data; }
	size_t		bytes_from(PegexpPointerSource origin)
			{ return byte_count - origin.byte_count; }
	bool		operator<(PegexpPointerSource& other)
			{ assert(data && other.data || (!data && !other.data)); return data < other.data; }
	off_t		operator-(PegexpPointerSource& other)
			{ assert(data && other.data || (!data && !other.data)); return data - other.data; }

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

// State is used to manage matching progress and backtracking locations.
template<
	typename _Source = PegexpDefaultSource
>
class PegexpState
{
public:
	using Source = _Source;

	// Constructor, copy, and assignment:
	PegexpState() : pattern(0), source() {}
	PegexpState(PatternP _pattern, Source _source) : pattern(_pattern), source(_source) {}
	PegexpState(const PegexpState& c) : pattern(c.pattern), source(c.source) {}
	PegexpState&	operator=(const PegexpState& c)
			{ pattern = c.pattern; source = c.source; return *this; }

	bool		at_expr_end()
			{ return *pattern == '\0' || *pattern == ')'; }

	PatternP	pattern;	// Current location in the Pegexp
	Source		source;		// Current source we're looking at

//	void		display() const;
};

/*
 * The default Match is just a copy of a start (from) and end (to) State.
 *
 * Success is where either:
 * - The "from" pattern is empty (we can always match nothing), or
 * - The pattern has advanced (from > to), or
 * - The source has advanced (from > to)
 *
 * Note that the pattern may advance without source advancing, when zero repetitions matched.
 * If the source advances but more repetitions are allowed, the pattern may remain unchanged, but
 * this isn't used in practise, because we don't declare a Match until all repetitions are attempted.
 */
template <typename _State = PegexpState<>>
class	PegexpDefaultMatch
{
public:
	typedef	_State	State;

	PegexpDefaultMatch() {}	// Default constructor

	// Any subclasses must have this constructor, to capture text matches:
	PegexpDefaultMatch(State _from, State _to)
	: from(_from)		// State (expression and source) before attempting matching
	, to(_to)		// State after attempting matching
	{ }

	// Additional data and constructors may be present in implementations, depending on the Context.

	bool		is_failure() const
			{
				if (*from.pattern == '\0')
					return false;	// We can always match the empty pattern
				// We failed if the pattern didn't advance and nothing was matched
				return from.pattern == to.pattern && from.source.same(to.source);
			}

	State		from;
	State		to;
};

/*
 * An example and default for the API that Pegexp requires for Context.
 * Implement your own template class having this signature to capture matches,
 * or to pass your own context down to a match_extended (as in Peg).
 */
template <typename _Match = PegexpDefaultMatch<PegexpState<PegexpDefaultSource>>>
class	PegexpDefaultContext
{
public:
	using	Match = _Match;
	using	State = typename Match::State;
	using	Source = typename Match::Source;
	PegexpDefaultContext()
	: capture_disabled(0)
	, repetition_nesting(0)
	{}

	// Captures are disabled inside a look-ahead (which can be nested). This holds the nesting count:
	int		capture_disabled;

	// Calls to capture() inside a repetition happen with in_repetition set. This holds the nesting.
	int		repetition_nesting;	// A counter of repetition nesting

	// Every capture gets a consecutive capture number, so we can roll back recent captures if needed:
	int		capture_count() const { return 0; }

	// A capture is a named Match. capture() should return the capture_count afterwards.
	int		capture(PatternP name, int name_len, Match, bool in_repetition) { return 0; }

	// In some grammars, capture can occur within a failing path, so we can roll back to a count:
	void		rollback_capture(int count) {}

	// When an atom of a Pegexp fails, the atom (pointer to start and end) and Source location are passed here.
	// Recording this can be useful to see why a Pegexp didn't proceed further, for example.
	void		record_failure(PatternP op, PatternP op_end, Source location) {}

	// Declare a match failure
	Match		match_failure(State at)
			{ return Match(at, at); }

	// Success is indicated by a "from" and "to" Source location:
	Match		match_result(State from, State to)
			{ return Match(from, to); }
};

template<
	typename _Context = PegexpDefaultContext<PegexpDefaultMatch<PegexpState<PegexpDefaultSource>>>
>
class Pegexp
{
public:	// Expose our template types for subclasses to use:
	using		Context = _Context;
	using		Match = typename Context::Match;
	using 		State = typename Match::State;
	using 		Source = typename State::Source;
	using 		Char = typename Source::Char;

	// Special characters that should be backslash escaped to use a random string as a literal:
	static	const char*	special;

	PatternP	pattern;	// The text of the Peg expression: 8-bit data, not UTF-8

	Pegexp(PatternP _pattern) : pattern(_pattern) {}

	// Match the Peg expression at or after the start of the source,
	// Advance to the end of the matched text and return the starting offset, or -1 on failure
	Match		match(Source& source, Context* context)
	{
		int	initial_captures = context->capture_count();
		off_t	offset = 0;

		while (true)
		{
			Source	attempt = source;
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
		return context->match_failure(State(pattern, source));	// Always at_eof()
	}

	Match		match_here(Source& source, Context* context)
	{
		State	state(pattern, source);

		bool ok = match_sequence(state, context);
		if (ok && *state.pattern == '\0')	// An extra ')' can cause match_sequence to succeed incorrectly
		{
			Match	match = context->match_result(State(pattern, source), state);

			// point source at the text following the successful attempt:
			source = state.source;
			return match;
		}
		else
		{
			// state.pattern points to the start of the atom of the pattern which failed,
			// state.source to the next unmatched input
			return context->match_failure(state);
		}
	}

protected:
	bool		match_sequence(State& state, Context* context)
	{
		if (state.at_expr_end())
			return true;	// We matched nothing, successfully

		int		sequence_capture_start = context->capture_count();

		bool	ok = match_atom(state, context);
		while (ok && !state.at_expr_end())
			ok = match_atom(state, context);

		// If we didn't get to the end of the sequence, rollback any captures in the incomplete sequence:
		if (!ok)
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
	Char		literal_char(PatternP& pattern)
	{
		Char		rc = *pattern++;
		bool		braces = false;

		if (rc == '\0')
		{
			pattern--;
			return 0;
		}
		if (rc != '\\')
			return rc;
		rc = *pattern++;
		if (rc >= '0' && rc <= '7')		// Octal
		{
			rc -= '0';
			if (*pattern >= '0' && *pattern <= '7')
				rc = (rc<<3) + *pattern++-'0';
			if (*pattern >= '0' && *pattern <= '7')
				rc = (rc<<3) + *pattern++-'0';
		}
		else if (rc == 'x')			// Hexadecimal
		{
			if (*pattern == '\0')
				return 0;
			if ((braces = (*pattern == '{')))
				pattern++;
			rc = unhex(*pattern++);
			if (rc < 0)
				return 0;		// Error: invalid first hex digit
			int second = unhex(*pattern++);
			if (second >= 0)
				rc = (rc << 4) | second;
			else
				pattern--;		// Invalid second hex digit
			if (braces && *pattern == '}')
				pattern++;
		}
		else if (rc == 'u')			// Unicode
		{
			rc = 0;
			if ((braces = (*pattern == '{')))
				pattern++;
			for (int i = 0; *pattern != '\0' && i < (braces ? 8 : 4); i++)
			{
				int	digit = unhex(*pattern);
				if (digit < 0)
					break;
				rc = (rc<<4) | digit;
				pattern++;
			}
			if (braces && *pattern == '}')
				pattern++;
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

	bool		char_property(PatternP& pattern, Char ch)
	{
		switch (char32_t esc = *pattern++)
		{
		case 'a':	return isalpha(ch);	// Alphabetic
		case 'd':	return isdigit(ch);	// Digit
		case 'h':	return isdigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');	// Hexadecimal
		case 'L':	return islower(ch);
		case 'U':	return isupper(ch);
		case 's':	return isspace(ch);
		case 'w':	return isalnum(ch);	// Alphabetic or digit
		default:	pattern -= 2;		// oops, not a property
				esc = literal_char(pattern);
				return esc == ch;	// Other escaped character, exact match
		}
	}

	bool		char_class(State& state)
	{
		if (state.source.at_eof())		// End of input never matches a char_class
			return false;

		State	start_state(state);
		start_state.pattern--;

		bool	negated;
		if ((negated = (*state.pattern == '^')))
			state.pattern++;

		bool	in_class = false;
		Char	ch = state.source.get_char();
		while (*state.pattern != '\0' && *state.pattern != ']')
		{
			Char	c1;

			// Handle actual properties, not other escapes
			if (*state.pattern == '\\' && isalpha(state.pattern[1]))
			{
				state.pattern++;
				if (char_property(state.pattern, ch))
					in_class = true;
				continue;
			}

			c1 = literal_char(state.pattern);
			if (*state.pattern == '-')
			{
				Char	c2;
				state.pattern++;
				c2 = literal_char(state.pattern);
				in_class |= (ch >= c1 && ch <= c2);
			}
			else
				in_class |= (ch == c1);
		}
		if (*state.pattern == ']')
			state.pattern++;
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
		if (state.source.at_eof() || *state.pattern != state.source.get_char())
			return false;
		state.pattern++;
		return true;
	}

	/*
	 * The guts of the algorithm. Match one atom against the current text.
	 *
	 * If this returns true, state.pattern has been advanced to the next atom,
	 * and state.source has consumed any input that matched.
	 */
	bool		match_atom(State& state, Context* context)
	{
		int		initial_captures = context->capture_count();
		State		start_state(state);

		bool		matched = false;
		char		rc;
		switch (rc = *state.pattern++)
		{
		case '\0':	// End of expression
			state.pattern--;
		case ')':	// End of group
			matched = true;
			break;

		case '^':	// Start of line
			matched = state.source.at_bol();
			break;

		case '$':	// End of line or end of input
			matched = state.source.at_eof() || state.source.get_char() == '\n';
			state.source = start_state.source;
			break;

		case '.':	// Any character
			if ((matched = !state.source.at_eof()))
				(void)state.source.get_char();
			break;

		default:	// Literal character
			if (rc > 0 && rc < ' ')		// Control characters
				goto extended;
			matched = !state.source.at_eof() && rc == state.source.get_char();
			break;

		case '\\':	// Escaped literal char
			matched = !state.source.at_eof() && char_property(state.pattern, state.source.get_char());
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
			const PatternP	repeat_pattern = state.pattern;		// This is the code we'll repeat
			State		iteration_start = state;	// revert here on iteration failure
			int		repetitions = 0;

			if (max != 1)
				context->repetition_nesting++;

			// Attempt the minimum iterations:
			while (repetitions < min)
			{
				state.pattern = repeat_pattern;	// We run the same atom more than once
				if (!match_atom(state, context))
					goto repeat_fail;	// We didn't meet the minimum repetitions
				repetitions++;
			}

			// Continue up to max iterations:
			while (max == 0 || repetitions < max)
			{
				int		iteration_captures = context->capture_count();
				iteration_start = state;
				state.pattern = repeat_pattern;
				if (!match_atom(state, context))
				{
					// Undo new captures on failure of an unmatched iteration
					context->rollback_capture(iteration_captures);

					// printf("iterated %d (>=min %d <=max %d)\n", repetitions, min, max);
					skip_atom(state.pattern);	// We're done with these repetitions, move on
					break;
				}
				if (state.source.same(iteration_start.source))
					break;	// We didn't advance, so don't keep trying. Can happen e.g. on *()
				repetitions++;
			}
			matched = true;
	repeat_fail:	if (max != 1)
				context->repetition_nesting--;
			break;
		}

		case '(':	// Parenthesised group
			start_state.pattern = state.pattern-1;	// Fail back to the start of the group
			if (!match_sequence(state, context))
				break;
			if (*state.pattern != '\0')		// Don't advance past group if it was not closed
				state.pattern++;		// Skip the ')'
			matched = true;
			break;

		case '|':	// Alternates
		{
			PatternP	next_alternate = state.pattern-1;
			int		alternate_captures = context->capture_count();
			while (*next_alternate == '|')	// There's another alternate
			{
				state = start_state;
				state.pattern = next_alternate+1;
				start_state.pattern = next_alternate+1;

				// Work through all atoms of the current alternate:
				do {
					if (!match_atom(state, context))
						break;
				} while (!(matched = state.at_expr_end() || *state.pattern == '|'));

				if (matched)
				{		// This alternate matched, skip to the end of the alternates
					while (*state.pattern == '|')	// There's another alternate
						skip_atom(state.pattern);
					break;
				}
				next_alternate = skip_atom(next_alternate);
				// Undo new captures on failure of an alternate
				context->rollback_capture(initial_captures);
			}
			break;
		}

		case '&':	// Positive lookahead assertion
		case '!':	// Negative lookahead assertion
		{
			context->capture_disabled++;
			matched = match_atom(state, context);
			if (rc == '!')
				matched = !matched;
			context->capture_disabled--;

			state = start_state;	// Continue with the same source
			if (matched)	// Assertion succeeded, skip it
				state.pattern = skip_atom(state.pattern);
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
			state.pattern--;
			matched = match_extended(state, context);
			break;
		}
		if (!matched)
		{
			// Undo new captures on failure
			context->rollback_capture(initial_captures);

			/*
			 * If this is the furthest point reached in the source so far,
			 * record the rule that was last attempted. Multiple rules may be
			 * attempted at this point, we want to know what characters would
			 * have allowed the parse to proceed.
			 */
			if ((char*)0 == strchr("?*+(|&!", rc))		// Don't report composite operators
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
					start_state.pattern,	// The PegexPC of the operator
					state.pattern,		// The PegexPC of the end of the operator
					start_state.source	// The Source location
				); // ... record only if state.source >= context->furthest_success
			}

			state = start_state;
			return false;
		}

		// Detect a label and save the matched atom
		PatternP	name = 0;
		if (*state.pattern == ':')
		{
			name = ++state.pattern;
			while (isalnum(*state.pattern) || *state.pattern == '_')
				state.pattern++;
			PatternP	name_end = state.pattern;
			if (*state.pattern == ':')
				state.pattern++;
			if (context->capture_disabled == 0)
				(void)context->capture(
					name, name_end-name,
					context->match_result(start_state, state),
					context->repetition_nesting > 0
				);
		}
		return true;
	}

	// Null extension; treat extensions like literal characters
	virtual void	skip_extended(PatternP& pattern)
	{
		pattern++;
	}

	const PatternP	skip_atom(PatternP& pattern)
	{
		switch (char rc = *pattern++)
		{
		case '\\':	// Escaped literal char
			pattern--;
			(void)literal_char(pattern);
			break;

		case '[':	// Skip char class
			if (*pattern == '^')
				pattern++;
			while (*pattern != '\0' && *pattern != ']')
			{
				(void)literal_char(pattern);
				if (*pattern == '-')
				{
					pattern++;
					(void)literal_char(pattern);
				}
			}
			if (*pattern == ']')
				pattern++;
			break;

		case '(':	// Group
			while (*pattern != '\0' && *pattern != ')')
				skip_atom(pattern);
			if (*pattern != '\0')
				pattern++;
			break;

		case '|':	// First or next alternate; skip to next alternate
			while (*pattern != '|' && *pattern != ')' && *pattern != '\0')
				skip_atom(pattern);
			break;

		case '&':	// Positive lookahead assertion
		case '!':	// Negative lookahead assertion
			skip_atom(pattern);
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
			pattern--;
			skip_extended(pattern);
			break;

		default:
			if (rc > 0 && rc < ' ')		// Control characters
				goto extended;
			break;

		// As-yet unimplemented features:
		// case '{':	// REVISIT: Implement counted repetition
		}
		if (*pattern == ':')
		{		// Skip a label
			pattern++;
			while (isalnum(*pattern) || *pattern == '_')
				pattern++;
			if (*pattern == ':')
				pattern++;
		}
		return pattern;
	}
};

template<typename Context> const char* Pegexp<Context>::special = "^$.\\[]?*+{()}|&!~@#%_;<`:";

#endif	// PEGEXP_H
