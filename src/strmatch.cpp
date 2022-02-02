/*
 * Unicode String regular expressions - the Matcher
 */
#include	<strregex.h>
#include	<string.h>
#include	<utility>

/*
 * To make it easy to pass around and modify result sets, we use a reference counted body.
 * These never get changed except when the reference count is one (1), and get deleted
 * when the ref_count decrements to zero.
 *
 * These objects are not publicly visible. The end-user API is in RxResult.
 */
class RxResultBody
: public RefCounted
{
public:
	RxResultBody(short c_max, short p_max);		// Needed when we want to truncate the captures
	RxResultBody(const RxProgram& _program)	// program is needed to initialise counter_max&capture_max
			: RxResultBody(_program.maxCounter(), _program.maxCapture()) {}
	RxResultBody(const RxResultBody& _to_copy);	// Used by RxResult::Unshare

	void		counter_push_zero();
	CharNum		counter_incr();
	void		counter_pop();
	const CharNum*	counter_top();

	CharNum		capture(int index);
	CharNum		capture_set(int index, CharNum val);

	// REVISIT: Add function call results

private:
	short		counter_max;
	short		capture_max;	// Each capture has a start and an end, so we allocate double
	short		counters_used;
	CharNum*	counters;	// captures and counters stored in the same array
};

class RxMatch
{
	// It is possible there can be as many threads as Stations in the program.
	// Each cycle traverses an array of current threads, building an array for the next cycle.
	struct Thread {
		RxStationID	station;
		RxResult	result;
	};

public:
	RxMatch(const RxProgram& _program, StrVal _target);
	~RxMatch();

	const RxResult	match_at(RxStationID start, CharNum& offset);

private:
	const RxProgram&  program;		// The NFA to execute
	StrVal		target;			// The string we are searching
	RxResult	result;			// Result from the successful thread

	// Concurrent threads of execution:
	Thread*		current_stations;
	RxStationID	current_count;		// How many of the above are populated
	Thread*		next_stations;
	RxStationID	next_count;		// How many of the above are populated
	void		addthread(Thread thread, CharNum offset, CharNum duplicates_allowed = 0);
};

static int zagzig(int i)
{
	return (i>>1) * ((i&01) ? -1 : 1);
}

RxProgram::RxProgram(const char* _nfa, bool take_ownership)
: nfa_owned(take_ownership)
, nfa(_nfa)
{
	RxOp		op;
	const char*	nfa_p = nfa;

	op = (RxOp)*nfa_p++;
	assert(op == RxOp::RxoStart);
	search_station = nfa_p-nfa;
	search_station += zagzig(UTF8Get(nfa_p));
	start_station = (nfa_p-nfa);
	start_station += zagzig(UTF8Get(nfa_p));
	max_station = UTF8Get(nfa_p);
	max_counter = *nfa_p++;
	max_capture = (*nfa_p++ & 0xFF);

	int	num_names = (*nfa_p++ & 0xFF)-1;
	for (int i = 0; i < num_names; i++)
	{
		CharBytes	byte_count = UTF8Get(nfa_p);
		names.push_back(StrVal(nfa_p, byte_count));
		nfa_p += byte_count;
	}
#ifdef TRACK_RESULTS
	printf("search_station %d start_station %d max_station %d max_counter %d max_capture %d, num_names %d, next %d\n",
		search_station, start_station, max_station, max_counter, max_capture, (int)names.size(), (int)(nfa_p-nfa));
#endif
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
	RxResult	result = match.match_at(searchStation(), offset);
	return result;
}

const RxResult
RxProgram::match_at(StrVal target, CharNum offset) const
{
	RxMatch		match(*this, target);
	RxResult	result = match.match_at(startStation(), offset);
	return result;
}

/*
 * A decoded NFA instruction
 */
struct RxDecoded
{
	RxOp		op;			// The opcode
	RxStationID	next;			// Every opcode except RxoAccept
	union {
		UCS4		character;	// RxoChar
		struct {			// RxoCharClass, RxoNegCharClass, RxoCharProperty
			CharBytes	bytes;
			const UTF8*	utf8;
		} text;
		RxStationID	alternate;	// RxoSplit, RxoCount
		short		capture_number;	// RxoCaptureStart, RxoCaptureEnd (0..maxCapture)
		short		group_number;	// RxoSubroutineCall
	};
	RxRepetitionRange repetition;		// RxoCount
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
		instr.capture_number = *(unsigned char*)nfa_p++ - 1;
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
RxMatch::addthread(Thread thread, CharNum offset, CharNum duplicates_allowed)
{
	int	duplicates = 0;
	for (int i = 0; i < next_count; i++)
	{
		if (next_stations[i].station == thread.station)
		{
			// Check for an exact duplicate first
			if ((duplicates_allowed		// If duplicates are allowed, we know there must be a counter
				&& thread.result.counter_top()
				&& *next_stations[i].result.counter_top()
				&& *thread.result.counter_top() == *next_stations[i].result.counter_top())
			 || ++duplicates > duplicates_allowed)
			{
				// REVISIT: Should we have a policy of deleting the duplicate with the lowest count?
				return;		// We already have this thread
			}
		}
	}

	// If the opcode at the new thread is Jump or Split, add the target(s). If it's Save, do that and skip over it
	RxDecoded	instr;
	program.decode(thread.station, instr);
	switch (instr.op)
	{
	case RxOp::RxoJump:
		addthread({instr.next, thread.result}, offset);
		break;

	case RxOp::RxoSplit:
		// REVISIT: Do ee need greedy and non-greedy versions of RxoSplit, with these in opposite order (same for RxoCount?)
		addthread({instr.alternate, thread.result}, offset);
		addthread({instr.next, thread.result}, offset);
		break;

	case RxOp::RxoCaptureStart:
		addthread({instr.next, thread.result.capture_set(instr.capture_number*2, offset)}, offset);
		break;

	case RxOp::RxoCaptureEnd:
		addthread({instr.next, thread.result.capture_set(instr.capture_number*2+1, offset)}, offset);
		break;

	case RxOp::RxoCount:	// This instruction follows a repeated item, so the pre-incremented counter tells how many we've passed
	{
		short	counter = thread.result.counter_incr();

		if (counter <= instr.repetition.max)	// Greedily repeat
			addthread({instr.alternate, thread.result}, offset, instr.repetition.min);

		if (counter >= instr.repetition.min && counter <= instr.repetition.max)
		{					// Wait at least until minimum count before trying to continue
			RxResult	continuation = thread.result;
			continuation.counter_pop();	// The continuing thread has no further need of the counter
			addthread({instr.next, continuation}, offset, instr.repetition.min);
		}
		thread.result.clear();
	}
		break;

	default:
		assert(next_count < program.maxStation());
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
	Thread*		thread_p;
	RxDecoded	instr;

	// Start where directed, and step forward one character at a time, following all threads until successful or there are no threads left
	next_count = 0;
	addthread({start, RxResult(program)}, offset);
	std::swap(current_stations, next_stations);
	current_count = next_count;
	next_count = 0;
	for (; current_count > 0 && offset <= target.length(); offset++)
	{
		{
		decode_next:
			RxStationID	pc = thread_p->station;
			program.decode(pc, instr);

			switch (instr.op)	// break from here to continue with instr.next, continue to ignore it.
			{
			case RxOp::RxoAccept:			// Termination condition
			{
				CharNum	new_result_start = thread_p->result.offset();
				if (!result.succeeded()			// We don't have any result yet
				 || new_result_start < result.offset()	// New result starts earlier
				 || (offset-new_result_start > result.length()		// New result is longer
				  && new_result_start == result.offset()))		// and starts at the same place
				{		// Got a better result
					result = thread_p->result;	// Make a new reference to this RxResult
					thread_p->result.clear();	// Clear the old reference
					result.capture_set(1, offset);	// Before we finalise it
				}
			}
				continue;

			case RxOp::RxoBOL:			// Beginning of Line
				if (offset == 0 || target[offset-1] == '\n')
				{
					thread_p->station = instr.next;
					goto decode_next;
				}
				thread_p->result.clear();
				continue;

			case RxOp::RxoEOL:			// End of Line
				if (offset == target.length() || target[offset] == '\n')
				{
					thread_p->station = instr.next;
					goto decode_next;
				}
				thread_p->result.clear();
				continue;

			case RxOp::RxoAny:			// Any single char
				// As long as we aren't at the end, all is good
				// REVISIT: Add except-newline mode behaviour
				if (offset < target.length())
					break;
				thread_p->result.clear();
				continue;

			case RxOp::RxoChar:			// A specific UCS4 character
				// REVISIT: Add case-insensitive mode behaviour
				if (target[offset] == instr.character)
					break;
				thread_p->result.clear();
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

			case RxOp::RxoNegLookahead:		// (?!...) match until EndGroup and return the opposite
				// REVISIT: Act like the negative lookahead has succeeded
				thread_p->station = instr.next;
				goto decode_next;
#if 0
			{
				CharNum 	start_offset = offset;	// Remember the target offset and reset it for each alternate
				const char*	next_p = nfa_p-1 + UTF8Get(nfa_p) + 1;	// Start of this instr, plus size, plus the EndGroup
				if (match_at(match, bt, nfa_p, offset))
					return RxResult();
				// Not matched, as requested, so continue with the next instruction at the current offset
				offset = start_offset;
				nfa_p = next_p;
				thread_p->result.clear();
				continue;
			}
#endif

			case RxOp::RxoSubroutineCall:		// Subroutine call to a named group
				break;	// REVISIT: Act like the subroutine has completed
#if 0
			{
				// Match the subroutine group and push_back subroutineMatches
				RxMatch		m;
				// REVISIT: Implement RxoSubroutineCall
				// match_at(...
				return RxResult();
			}
#endif

			case RxOp::RxoZero:
				// push a zero to this thread's counter stack
				thread_p->result.counter_push_zero();
				thread_p->station = instr.next;
				goto decode_next;

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
			case RxOp::RxoCount:
				assert(0 == "Should not happen");
				return RxResult();
			}
			addthread({instr.next, thread_p->result}, offset+1);
			thread_p->result.clear();
		}

// REVISIT: Should not now be necessary, because we clear as we go
//		// Wipe old result references, we already copied the ones we need into next_stations:
//		for (Thread* thread_p = current_stations; thread_p < current_stations+current_count; thread_p++)
//			assert((RxResultBody*)thread_p->result.body == (RxResultBody*)0);

		std::swap(current_stations, next_stations);
		current_count = next_count;
		next_count = 0;
	}

	// If we have a result, it's in `result`.
	// If not, we ran out of threads that could move forward, or ran out of input to move forward on.
	// REVISIT: Might it be useful to know on which Station(s) we ran out of input?

	// Wipe old result references, we already copied the one we chose:
	for (thread_p = current_stations; thread_p < current_stations+current_count; thread_p++)
		thread_p->result.clear();
	return result;
}

RxResultBody::RxResultBody(short c_max, short p_max)
: counter_max(c_max)
, capture_max(p_max)
, counters(new CharNum[counter_max+capture_max*2-1])
, counters_used(0)
{
	memset(counters, 0, sizeof(CharNum)*(counter_max+capture_max*2-1));
}

RxResultBody::RxResultBody(const RxResultBody& to_copy)
: counter_max(to_copy.counter_max)
, capture_max(to_copy.capture_max)
, counters(new CharNum[counter_max+capture_max*2-1])
, counters_used(to_copy.counters_used)
{
	memcpy(counters, to_copy.counters, sizeof(CharNum)*(counter_max+capture_max*2-1));
}

void
RxResultBody::counter_push_zero()
{
	assert(counters_used < counter_max);
	if (counters_used == counter_max)
		return;
	counters[counters_used++] = 0;
}

CharNum
RxResultBody::counter_incr()
{
	assert(counters_used > 0);
	return ++counters[counters_used-1];
}

void
RxResultBody::counter_pop()
{
	assert(counters_used > 0);
	if (counters_used == 0)
		return;
	counters_used--;
}

const CharNum*
RxResultBody::counter_top()
{
	return counters_used > 0 ? &counters[counters_used-1] : 0;
}

CharNum
RxResultBody::capture(int index)
{
	assert(!(index < 1 || index >= capture_max*2));
	if (index < 1 || index >= capture_max*2)
		return 0;
	return counters[counter_max+index-1];
}

CharNum
RxResultBody::capture_set(int index, CharNum val)
{
	assert(!(index < 1 || index >= capture_max*2));
	if (index < 1 || index >= capture_max*2)
		return 0;		// Ignore captures outside the defined range
	return counters[counter_max+index-1] = val;
}

RxResult::RxResult(const RxProgram& program)
: body(new RxResultBody(program))
{
}

RxResult::RxResult()		// Failed result
: body(0)
, cap0(0)
{
}

RxResult::RxResult(const RxResult& s1)
: body(s1.body)
, cap0(s1.cap0)
{	// Normal copy constructor
}

RxResult& RxResult::operator=(const RxResult& s1)
{
	body = s1.body;
	cap0 = s1.cap0;
	return *this;
}

void
RxResult::clear()
{
	body = (RxResultBody*)0;	// Clear the reference and its refcount
	cap0 = 0;
}

void
RxResult::Unshare()
{
	assert(body);
	if (body.GetRefCount() <= 1)
		return;
	body = new RxResultBody(*body);	// Copy the result body before making changes
}

RxResult::~RxResult()
{
}

CharNum
RxResult::capture(int index) const
{
	if (index == 0)
		return cap0;
	assert(body);
	return body->capture(index);
}

RxResult&
RxResult::capture_set(int index, CharNum val)
{
	if (index == 0)
	{
		cap0 = val;
		return *this;
	}
	assert(body);
	Unshare();
	body->capture_set(index, val);
	return *this;
}

void
RxResult::counter_push_zero()	// Push a zero counter
{
	Unshare();
	assert(body);
	body->counter_push_zero();
}

CharNum
RxResult::counter_incr()	// Increment and return top counter of stack
{
	Unshare();
	assert(body);
	return body->counter_incr();
}

void
RxResult::counter_pop()		// Discard the top counter of the stack
{
	Unshare();
	assert(body);
	body->counter_pop();
}

const CharNum*
RxResult::counter_top()
{
	return body->counter_top();
}
