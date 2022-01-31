/*
 * Unicode String regular expressions - the Matcher
 */
#include	<strregex.h>
#include	<string.h>
#include	<utility>

class RxMatch
{
	struct Thread {
		RxStationID	station;
		RxResult	result;
	};

public:
	RxMatch(const RxProgram& _program, StrVal _target);
	~RxMatch();

	const RxResult	match_at(RxStationID start, CharNum& offset);

//private:
	const RxProgram&  program;		// The NFA to execute
	StrVal		target;			// The string we are searching
	bool		succeeded;		// Are we done yet?
	RxResult	result;			// result from the successful thread

	// Concurrent threads of execution:
	Thread*		current_stations;
	RxStationID	current_count;		// How many of the above are populated
	Thread*		next_stations;
	RxStationID	next_count;		// How many of the above are populated
	void		addthread(Thread thread, CharNum offset);
};

RxProgram::RxProgram(const char* _nfa, bool take_ownership)
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

RxProgram::~RxProgram()
{
	if (nfa && nfa_owned) delete(nfa);
}

const RxResult
RxProgram::match_after(StrVal target, CharNum offset) const
{
	RxMatch		match(*this, target);
	return match.match_at(searchStation(), offset);
}

const RxResult
RxProgram::match_at(StrVal target, CharNum offset) const
{
	RxMatch		match(*this, target);
	return match.match_at(startStation(), offset);
}

static int zagzig(int i)
{
	return (i>>1) * ((i&01) ? -1 : 1);
}

/*
 * A decoded NFA instruction
 */
struct RxDecoded
{
	RxOp		op;
	RxStationID	next;			// Every opcode except RxoAccept
	union {
		UCS4		character;	// RxoChar
		struct {
			CharBytes	bytes;
			const UTF8*	utf8;
		} text;				// RxoCharClass, RxoNegCharClass, RxoCharProperty
		RxStationID	alternate;	// RxoSplit, RxoCount
		short		capture_number;	// RxoCaptureStart, RxoCaptureEnd (0..maxCapture)
		short		group_number;	// RxoSubroutineCall
		RxRepetitionRange repetition;
	};
};

void
RxProgram::decode(RxStationID station, RxDecoded& instr) const
{
	const char*	nfa_p = nfa+station;
	switch (instr.op = (RxOp)*nfa_p++)
	{
	case RxOp::RxoStart:
		// We already decoded this in the constructor, don't do it again
		instr.next = start_station;
		instr.alternate = search_station;
		return;

	case RxOp::RxoChar:
		instr.character = UTF8Get(nfa_p);
		break;

	case RxOp::RxoCharProperty:		// A character with a named Unicode property
	case RxOp::RxoCharClass:		// Character class
	case RxOp::RxoNegCharClass:		// Negated Character class
		instr.text.bytes = UTF8Get(nfa_p);		// Get the byte count
		instr.text.utf8 = nfa_p;	// And the pointer
		nfa_p += instr.text.bytes;
		break;

	case RxOp::RxoBOL:
	case RxOp::RxoEOL:
	case RxOp::RxoAny:
	case RxOp::RxoAccept:
	case RxOp::RxoZero:
		break;				// No operands

	case RxOp::RxoNegLookahead:
		instr.alternate = (nfa_p-nfa);	// Where to continue if the lookahead allows it
		instr.alternate += zagzig(UTF8Get(nfa_p));
		break;

	case RxOp::RxoSubroutineCall:
		instr.group_number = *(unsigned char*)nfa_p++;		// group number 1 is the first name
		break;

	case RxOp::RxoJump:
		instr.next = (nfa_p-nfa);
		instr.next += zagzig(UTF8Get(nfa_p));
		return;

	case RxOp::RxoSplit:
		instr.alternate = (nfa_p-nfa);
		instr.alternate += zagzig(UTF8Get(nfa_p));
		break;

	case RxOp::RxoCount:
		instr.repetition.min = *(unsigned char*)nfa_p++ - 1;
		instr.repetition.max = *(unsigned char*)nfa_p++ - 1;
		instr.alternate = (nfa_p-nfa);
		instr.alternate += zagzig(UTF8Get(nfa_p));
		break;

	case RxOp::RxoCaptureStart:
	case RxOp::RxoCaptureEnd:
		instr.capture_number = *(unsigned char*)nfa_p++;
		break;

	case RxOp::RxoNull:			// These opcodes are never in the NFA
	case RxOp::RxoLiteral:
	case RxOp::RxoNonCapturingGroup:
	case RxOp::RxoNamedCapture:
	case RxOp::RxoAlternate:
	case RxOp::RxoEndGroup:
	case RxOp::RxoRepetition:
		return;

	default:
		assert(0 == "Decode is not fully implemented");
		return;
	}
	instr.next = (RxStationID)(nfa_p-nfa);
}

RxMatch::RxMatch(const RxProgram& _program, StrVal _target)
: program(_program)
, target(_target)
, succeeded(false)
{
	int	i;
	current_stations = new Thread[i = program.maxStation()];
	current_count = 0;
	next_stations = new Thread[i];
	next_count = 0;
}

RxMatch::~RxMatch()
{
	delete[] current_stations;
	delete[] next_stations;
}

void
RxMatch::addthread(Thread thread, CharNum offset)
{
	for (int i = 0; i < next_count; i++)
		if (next_stations[i].station == thread.station)
			return;		// We already have this thread

	// If the opcode at the new thread is Jump or Split, add the target(s). If it's Save, do that and skip over it
	RxDecoded	instr;
	program.decode(thread.station, instr);
	switch (instr.op)
	{
	case RxOp::RxoJump:
		addthread({instr.next, thread.result}, offset);
		break;

	case RxOp::RxoSplit:
		addthread({instr.next, thread.result}, offset);
		addthread({instr.alternate, thread.result}, offset);
		break;

	case RxOp::RxoCaptureStart:
		addthread({instr.next, thread.result.capture_set(instr.capture_number*2, offset)}, offset);
		break;

	case RxOp::RxoCaptureEnd:
		addthread({instr.next, thread.result.capture_set(instr.capture_number*2+1, offset)}, offset);
		break;

	default:
		next_stations[next_count++] = thread;
	}
};

/*
 * Continue matching the target string from offset, from the NFA instruction at RxStationID.
 * Populate the RxMatch where necessary.
 */
const RxResult
RxMatch::match_at(RxStationID start, CharNum& offset)
{
	RxDecoded	instr;

	current_count = 0;
	current_stations[current_count++] = {start, RxResult(program)};
	// Step forward one character at a time, following all threads
	for (; current_count > 0 && offset < target.length(); offset++)
	{
		for (int thread_num = 0; thread_num < current_count; thread_num++)
		{
			Thread&		thread = current_stations[thread_num];
			RxStationID	pc = thread.station;
			program.decode(pc, instr);

			// break from here to accept instr.next, continue to ignore it.
			switch (instr.op)
			{
			case RxOp::RxoAccept:			// Termination condition
				succeeded = true;
				return thread.result;

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
					return RxResult();
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
				return RxResult();
			}
#endif
			case RxOp::RxoCount:
				// REVISIT: Implement counted repetition
				break;

			case RxOp::RxoZero:
				// REVISIT: push a zero this thread's counter stack
				break;

			// Compiler opcodes that aren't part of the VM:
			case RxOp::RxoNull:
			case RxOp::RxoStart:
			case RxOp::RxoLiteral:
			case RxOp::RxoRepetition:
			case RxOp::RxoNonCapturingGroup:
			case RxOp::RxoNamedCapture:
			case RxOp::RxoAlternate:
			case RxOp::RxoEndGroup:

			// Shunts (handled in addthread)
			case RxOp::RxoJump:
			case RxOp::RxoSplit:
			case RxOp::RxoCaptureStart:
			case RxOp::RxoCaptureEnd:
			// Not yet implemented:
			case RxOp::RxoNegLookahead:
			case RxOp::RxoSubroutineCall:
				assert(0 == "Should not happen");
				return RxResult();
			}
			addthread({instr.next, thread.result}, offset+1);
		}
		std::swap(current_stations, next_stations);
		current_count = next_count;
		next_count = 0;
	}

	// We ran out of threads that could move forward, or ran out of input to move forward on
	return RxResult();
}
