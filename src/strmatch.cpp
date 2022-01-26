/*
 * Unicode String regular expressions - the Matcher
 */
#include	<strregex.h>
#include	<string.h>
#include	<utility>

RxMatcher::RxMatcher(const char* _nfa, bool take_ownership)
: nfa_owned(take_ownership)
, nfa(_nfa)
{
	RxOp		op;
	const char*	nfa_p = nfa;

	op = (RxOp)*nfa_p++;
	assert(op == RxOp::RxoStart);
	search_station = (nfa_p-nfa)+UTF8Get(nfa_p);
	start_station = (nfa_p-nfa)+UTF8Get(nfa_p);
	max_station = UTF8Get(nfa_p);
	max_counter = *nfa_p++;
	max_capture = (*nfa_p++ & 0xFF) - 1;
	for (int i = 0; i < max_capture; i++)
	{
		CharBytes	byte_count = UTF8Get(nfa_p);
		names.push_back(StrVal(nfa_p, byte_count));
		nfa_p += byte_count;
	}
	assert(start_station == nfa_p-nfa);
}

RxMatcher::~RxMatcher()
{
	if (nfa && nfa_owned) delete(nfa);
}

RxMatch
RxMatcher::match_after(StrVal target, CharNum offset)
{
	RxMatch		match(*this, target);
	(void)match.match_at(searchStation(), offset);
	return match;
}

RxMatch
RxMatcher::match_at(StrVal target, CharNum offset)
{
	RxMatch		match(*this, target);
	(void)match.match_at(startStation(), offset);
	return match;
}

void
RxMatcher::decode(RxStationID station, RxDecoded& instr)
{
	const char*	nfa_p = nfa+station;
	switch (instr.op = (RxOp)*nfa_p++)
	{
	case RxOp::RxoStart:			// We decoded this in the constructor
		instr.next = start_station;
		instr.alternate = search_station;
		return;

	case RxOp::RxoChar:
		instr.character = UTF8Get(nfa_p);
		break;

	case RxOp::RxoCharProperty:		// A character with a named Unicode property
	case RxOp::RxoCharClass:		// Character class
	case RxOp::RxoNegCharClass:		// Negated Character class
		instr.text.bytes = UTF8Get(nfa_p);
		instr.text.utf8 = nfa_p;
		nfa_p += instr.text.bytes;
		break;

	// REVISIT: Add other decode cases here:
	default:
		assert(0 == "Decode is not fully implemented");
		return;
	}
	instr.next = (RxStationID)(nfa_p-nfa);
}

RxMatch::RxMatch(RxMatcher& _matcher, StrVal _target)
: matcher(_matcher)
, target(_target)
, succeeded(false)
{
	int	i;
	current_stations = new RxStationID[i = matcher.maxStation()];
	current_count = 0;
	next_stations = new RxStationID[i];
	next_count = 0;

	current_counter = counters = new short[i = matcher.maxCounter()];
	memset(counters, 0, sizeof(short)*i);

	captures = new CharNum[i = 2 * matcher.maxCapture()];
	memset(captures, 0, sizeof(CharNum)*i);
}

RxMatch::~RxMatch()
{
	delete[] current_stations;
	delete[] next_stations;
	delete[] counters;
	delete[] captures;
}

#if 0
void
RxMatch::take(RxMatch& alt)	// Take contents from alt, to avoid copying the vectors
{
	target = alt.target;
	succeeded = alt.succeeded;
	// REVISIT: mumble, mumble
}
#endif

void
RxMatch::addthread(RxStationID thread, CharNum offset)
{
	for (int i = 0; i < next_count; i++)
		if (next_stations[i] == thread)
			return;		// We already have this thread
	// If the opcode at the new thread is Jump or Split, add the target(s). If it's Save, do that and skip over it
	RxDecoded	instr;
	matcher.decode(thread, instr);
	switch (instr.op)
	{
	case RxOp::RxoJump:
		addthread(instr.next, offset);
		break;

	case RxOp::RxoSplit:
		addthread(instr.next, offset);
		addthread(instr.alternate, offset);
		break;

	case RxOp::RxoCaptureStart:
		// REVISIT: every thread needs its own set of capture locations
		captures[instr.capture_number*2] = offset;
		addthread(instr.next, offset);
		break;

	case RxOp::RxoCaptureEnd:
		captures[instr.capture_number*2+1] = offset;
		addthread(instr.next, offset);
		break;

	default:
		next_stations[next_count++] = thread;
	}
};

/*
 * Continue matching the target string from offset, from the NFA instruction at RxStationID.
 * Populate the RxMatch where necessary.
 */
bool
RxMatch::match_at(RxStationID start, CharNum& offset)
{
	RxDecoded	instr;

	current_count = 0;
	current_stations[current_count++]  = start;
	// Step forward one character at a time, following all threads
	for (; current_count > 0 && offset < target.length(); offset++)
	{
		for (int thread = 0; thread < current_count; thread++)
		{
			RxStationID	pc = current_stations[thread];
			matcher.decode(pc, instr);

			// break from here to accept instr.next, continue to ignore it.
			switch (instr.op)
			{
			case RxOp::RxoAccept:			// Termination condition
				succeeded = true;
				captures[1] = offset;
				return true;

			case RxOp::RxoBOL:			// Beginning of Line
				if (offset == 0 || target[offset-1] == '\n')
					break;
				continue;

			case RxOp::RxoEOL:			// End of Line
				if (offset == target.length() || target[offset] == '\n')
					break;
				continue;

			case RxOp::RxoAny:			// Any single char
				// As long as we aren't at the end, all is good
				// REVISIT: Add except-newline mode behaviour
				if (offset >= target.length())
					break;
				continue;

			case RxOp::RxoChar:			// A specific UCS4 character
				// REVISIT: Add case-insensitive mode behaviour
				if (target[offset] == instr.character)
					break;
				continue;			// Dead-end this thread

			case RxOp::RxoCharClass:		// Character class
			case RxOp::RxoNegCharClass:		// Negated Character class
				// REVISIT: Re-implement these by unpacking the characters one at a time
			case RxOp::RxoCharProperty:		// A character with a named Unicode property
			{
				// Check that the next characters match these ones
				StrBody		body(instr.text.utf8, false, instr.text.bytes);
				StrVal		expected(&body);
				CharNum		length = expected.length();	// Count the chars in the literal
				assert(length > 0);

				// REVISIT: Implement these operations

				break;
			}

#if 0
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

			case RxOp::RxoSubroutineCall:		// Subroutine call to a named group
			{
				// Match the subroutine group and push_back subroutineMatches
				RxMatch		m;
				// REVISIT: Implement RxoSubroutineCall
				// match_at(...
				return false;
			}
#endif

			// Compiler opcodes that aren't part of the VM:
			case RxOp::RxoNull:
			case RxOp::RxoStart:
			case RxOp::RxoLiteral:
			case RxOp::RxoRepetition:
			case RxOp::RxoNonCapturingGroup:
			case RxOp::RxoNamedCapture:
			case RxOp::RxoAlternate:
			case RxOp::RxoEndGroup:
			case RxOp::RxoFirstAlternate:

			// Shunts (handled in addthread)
			case RxOp::RxoJump:
			case RxOp::RxoSplit:
			case RxOp::RxoCaptureStart:
			case RxOp::RxoCaptureEnd:
			// Not yet implemented:
			case RxOp::RxoNegLookahead:
			case RxOp::RxoSubroutineCall:
				assert(0 == "Should not happen");
				return 0;
			}
			addthread(instr.next, offset+1);
		}
		std::swap(current_stations, next_stations);
		current_count = next_count;
		next_count = 0;
	}

	// We ran out of threads that could move forward, or ran out of input to move forward on
	return false;
}

#if 0
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

	case RxOp::RxoAccept:			// Termination condition
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

	case RxOp::RxoSubroutineCall:		// Subroutine call to a named group
		return nfa_p+1;

	case RxOp::RxoRepetition:		// {n, m}
		return continue_nfa(nfa_p+2)+1;
	}
}
#endif
