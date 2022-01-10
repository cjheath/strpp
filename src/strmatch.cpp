/*
 * Unicode String regular expressions - the Matcher
 */
#include	<strregex.h>
#include	<string.h>

class	RxBacktracks
: public std::vector<CharBytes>
{
};

void
RxMatch::take(RxMatch& alt)	// Take contents from alt, to avoid copying the vectors
{
	target = alt.target;
	succeeded = alt.succeeded;
	offset = alt.offset;
	length = alt.length;
	clear();
	captures.swap(alt.captures);
	captureVecs.swap(alt.captureVecs);
	subroutineMatches.swap(alt.subroutineMatches);
}

RxMatcher::RxMatcher(const char* _nfa, bool take_ownership)
: nfa_owned(take_ownership)
, nfa(_nfa)
, depth(0)
{
}

RxMatch
RxMatcher::match_after(StrVal target, CharNum offset)
{
	RxMatch		match(target);
	RxBacktracks	bt;
	for (CharNum o = offset; o < target.length(); o++)
	{
		const UTF8*	nfa_p = nfa;
		CharNum		off = o;
		// nfa_p and off get modified, so reset them each time
		if (match_at(match, bt, nfa_p, off))
			break;
	}
	return match;
}

RxMatch
RxMatcher::match_at(StrVal target, CharNum offset)
{
	RxMatch		match(target);
	RxBacktracks	bt;
	const UTF8*	nfa_p = nfa;
	match_at(match, bt, nfa_p, offset);
	return match;
}

/*
 * Continue matching the target string from offset, advancing both nfa_p and offset.
 * Populate the RxMatch where necessary.
 */
bool
RxMatcher::match_at(RxMatch& match, RxBacktracks& bt, const char*& nfa_p, CharNum& offset)
{
	for (;;)
	{
#if	TRACE_MATCH
		const char*	np = nfa_p;
		printf("(at %d): NFA ", offset);
		RxCompiler::instr_dump(nfa, np, depth);
#endif

		switch ((RxOp)*nfa_p++)
		{
		case RxOp::RxoNull:
			return false;

		case RxOp::RxoStart:			// Place to start the DFA
		{
			// CharBytes	offset_next;	// NFA offset to the next RxOp
			/* offset_next = */(void)UTF8Get(nfa_p);	// Offset to the RxoEnd
			int num_names = (*nfa_p++ & 0xFF) - 1;
			for (int i = 0; i < num_names; i++)
			{
				CharBytes	byte_count = UTF8Get(nfa_p);
				names.push_back(StrVal(nfa_p, byte_count));
				nfa_p += byte_count;
			}

			match.offset = offset;
			continue;
		}

		case RxOp::RxoEnd:			// Termination condition
			match.succeeded = true;
			match.length = offset-match.offset;
			nfa_p--;
			return true;

		case RxOp::RxoBOL:			// Beginning of Line
			if (offset == 0 || match.target[offset-1] == '\n')
				continue;
			return false;			// Not beginning of line

		case RxOp::RxoEOL:			// End of Line
			if (offset == match.target.length() || match.target[offset] == '\n')
				continue;
			return false;			// Not end of line

		case RxOp::RxoAny:			// Any single char
			// As long as we aren't at the end, move forward one character and continue.
			if (offset >= match.target.length())
				return false;
			offset++;
			continue;

		case RxOp::RxoLiteral:		// A string of specific characters
		{
			// Check that the next characters match these ones
			CharBytes	byte_count = UTF8Get(nfa_p);
			StrBody		body(nfa_p, false, byte_count);	// Static body, contents won't be copied or deleted
			StrVal		expected(&body);
			CharNum		length = expected.length();	// Count the chars in the literal
			assert(length > 0);

			if (match.target.substr(offset, length) != expected)
				return false;
			offset += length;	// Advance
			nfa_p += byte_count;
			continue;		// with the next expression in this sequence
		}

		case RxOp::RxoCharProperty:		// A character with a named Unicode property
		case RxOp::RxoCharClass:		// Character class
		case RxOp::RxoNegCharClass:		// Negated Character class
		{
			// Check that the next characters match these ones
			CharBytes	byte_count = UTF8Get(nfa_p);
			StrBody		body(nfa_p, false, byte_count);	// Static body, contents won't be copied or deleted
			StrVal		expected(&body);
			CharNum		length = expected.length();	// Count the chars in the literal
			assert(length > 0);

			// REVISIT: Implement these operations

			offset += length;	// Advance
			nfa_p += byte_count;
			continue;
		}

		case RxOp::RxoRepetition:		// {n..m} Match n repetitions then up to (m-n) more
		{
			int		min, max;
			CharNum		repetition_start = offset;
			int		backtrack_start = bt.size();
			int		i;
			const char*	continue_p = continue_nfa(nfa_p-1); // Set continue_p to point to the NFA after the repeated item

			min = (*nfa_p++ & 0xFF) - 1;
			max = (*nfa_p++ & 0xFF) - 1;

			// Repeat the minimum number of times, or fail
			for (i = 0; i < min; i++)
			{
				const char*	np = nfa_p;	// Reset to the repeated pattern after each advance
				if (!match_at(match, bt, np, offset))
				{
					// REVISIT: How to undo any captures made during a failing sequence?
					offset = repetition_start;
					return false;
				}
			}

			/*
			 * Continue repeating up to the maximum number of times.
			 *
			 * REVISIT: I believe we can limit the size of the backtrack stack. It means
			 * the NFA might not find a solution that's available to it, but repetition of
			 * a nested group probably *should* have some limit like this.
			 * Repetition of a single char, any, or char-class/property won't need the stack.
			 */
			for (i = min; (max == 0 || i < max) && offset < match.target.length(); i++)
			{
				CharNum		iteration_start = offset;

				const char*	np = nfa_p;	// Reset to the repeated pattern after each advance
				if (!match_at(match, bt, np, offset))
				{
					offset = iteration_start;
					break;
				}
				if (bt.size() > backtrack_start || offset-iteration_start > 1)	// start pushing backtrack stack if we step further than one 
					bt.push_back(iteration_start);
			}

			// Now, continue with the NFA, backtracking if needed
			do {
				CharNum current_offset = offset;

				// Recurse trying to succeed in matching what follows this repetition
				const char*	np = continue_p;	// Start from the following pattern for each backtrack
				if (match_at(match, bt, np, offset))
				{
					while (bt.size() > backtrack_start)
						bt.pop_back();	// Discard the rest of the backtrack positions
					return true;
				}

				if (bt.size() > backtrack_start)
				{		// Use the previous saved backtracking position
					offset = bt.back();
					bt.pop_back();
				}
				else if (offset > 0)		// or the previous character if none saved
					offset = current_offset-1;
				else
					break;
			} while (bt.size() > 0 || offset > repetition_start);
			return false;
		}

		case RxOp::RxoFirstAlternate:
		{
			CharNum 	start_offset = offset;	// Remember the target offset and reset it for each alternate
			const char*	next_p;
			for (;;) {
				next_p = nfa_p-1 + UTF8Get(nfa_p);	// Next alternate or end
				offset = start_offset;
				if (match_at(match, bt, nfa_p, offset))
				{		// Succeeding means we hit RxoEnd/RxoEndGroup, or RxoAlternate which advanced to End
					nfa_p--;
					break;
				}
				if (*next_p == (char)RxOp::RxoEndGroup || *next_p == (char)RxOp::RxoEnd)
				{
					nfa_p = next_p;
					return false;	// No alternate succeeded
				}
				assert(*next_p == (char)RxOp::RxoAlternate);
				nfa_p = next_p+1;
#if	TRACE_MATCH
				printf("(at %d): NFA ", offset);
				RxCompiler::instr_dump(nfa, next_p, depth);
#endif
			}
			continue;	// An alternate succeeded, we're now looking at the next thing
		}

		case RxOp::RxoAlternate:		// Match to the EndGroup or the next alternate
			// This alternate succeeded. Skip to the instruction after the last alternate
			// REVISIT: If the rest of the RE fails, backtrack to here and try this alternative
			do {
				const char*	np = nfa_p-1;
				nfa_p = np + UTF8Get(nfa_p);
			} while (*nfa_p++ == (char)RxOp::RxoAlternate);
			nfa_p--;
			return true;

		case RxOp::RxoEndGroup:		// End of a group
			return true;

		case RxOp::RxoNonCapturingGroup:	// (...); match until EndGroup
			/* offset_next = */ (void)UTF8Get(nfa_p);	// Pointer to what follows the group
			if (!match_at(match, bt, nfa_p, offset))
				return false;
			continue;

		case RxOp::RxoNamedCapture:		// (?<name>...)	match until EndGroup and save as a capture
		{
			/* offset_next = */ (void)UTF8Get(nfa_p);
			int	group_num = (*nfa_p++ & 0xFF) - 1;
			if (!match_at(match, bt, nfa_p, offset))
				return false;
			// REVISIT: Record the capture
			continue;
		}

		case RxOp::RxoNegLookahead:		// (?!...) match until EndGroup and return the opposite
		{
			CharNum 	start_offset = offset;	// Remember the target offset and reset it for each alternate
			const char*	next_p = nfa_p-1 + UTF8Get(nfa_p) + 1;	// Start of this instr, plus size, plus the EndGroup
			if (match_at(match, bt, nfa_p, offset))
				return false;
			// Not matched, as requested, so continue with the next instruction at the current offset
			offset = start_offset;
			nfa_p = next_p;
			continue;
		}

		case RxOp::RxoSubroutine:		// Subroutine call to a named group
		{
			int group_num = (*nfa_p++ & 0xFF) - 1;
#if 0
			// Match the subroutine group and push_back subroutineMatches
			RxMatch		m;
			// REVISIT: Implement RxoSubroutine
			// match_at(...
#endif
			return false;
		}
		}
	}
	return false;
}

const char*
RxMatcher::continue_nfa(const char* nfa_p) const
{
	int		offset_next;
	const char*	start_p = nfa_p;

	switch ((RxOp)*nfa_p++)
	{
	case RxOp::RxoNull:
	case RxOp::RxoStart:			// Place to start the DFA
		// There is no need to ever do this
		assert("This shouldn't happen" && false);

	case RxOp::RxoEnd:			// Termination condition
	case RxOp::RxoEndGroup:			// End of a group
	case RxOp::RxoBOL:			// Beginning of Line
	case RxOp::RxoEOL:			// End of Line
	case RxOp::RxoAny:			// Any single char
		return nfa_p;

	case RxOp::RxoFirstAlternate:		// |
	case RxOp::RxoAlternate:		// |
		offset_next = UTF8Get(nfa_p);	// REVISIT: Are these ever needed?
		return start_p+offset_next;

	case RxOp::RxoNonCapturingGroup:	// (...)
	case RxOp::RxoNegLookahead:		// (?!...)
	case RxOp::RxoNamedCapture:		// (?<name>...)
		offset_next = UTF8Get(nfa_p);
		return start_p+offset_next+1;

	case RxOp::RxoLiteral:			// A specific string
	case RxOp::RxoCharProperty:		// A char with a named Unicode property
	case RxOp::RxoCharClass:		// Character class; instr contains a string of character pairs
	case RxOp::RxoNegCharClass:		// Negated Character class
		offset_next = UTF8Get(nfa_p);	// actually byte_count
		return nfa_p+offset_next;

	case RxOp::RxoSubroutine:		// Subroutine call to a named group
		return nfa_p+1;

	case RxOp::RxoRepetition:		// {n, m}
		return continue_nfa(nfa_p+2)+1;
	}
}
