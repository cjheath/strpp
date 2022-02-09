/*
 * Unicode Strings
 * Regular expressions - the matcher
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<strregex.h>
#include	<string.h>
#include	<utility>

#ifdef	TRACK_RESULTS
#define	TRACK(arglist)	printf arglist
#else
#define	TRACK(arglist)	
#endif

/*
 * To make it easy to pass around and modify capture sets, we use a reference counted body.
 * These never get changed except when the reference count is one (1), and get deleted
 * when the ref_count decrements to zero.
 *
 * These objects are not publicly visible. The end-user API is in RxResult.
 *
 * A single allocated array contains both counters and captures
 * Each counter has two values: an input offset (where the count started) and a counter value
 */
class RxCaptures
: public RefCounted
{
public:
	RxCaptures(short c_max, short p_max);		// Needed when we want to truncate the captures
	RxCaptures(const RxProgram& _program)	// program is needed to initialise counter_max&capture_max
			: RxCaptures(_program.maxCounter(), _program.maxCapture()) {}
	RxCaptures(const RxCaptures& _to_copy);	// Used by RxResult::Unshare
	~RxCaptures() { if (counters) delete[] counters; }

	int		counterNum() const { return counters_used/2; }
	int		counterGet(int i) const { return counters[counters_used - i*2 - 1]; }
	void		counterPushZero(CharNum offset);
	CharNum		counterIncr(CharNum offset);
	void		counterPop();
	const RxResult::Counter	counterTop();

	int		captureMax() const;
	CharNum		capture(int index);
	CharNum		captureSet(int index, CharNum val);

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
		Thread() : station(0), result() {}
		Thread(RxStationID s, RxResult r);
		RxStationID	station;
		RxResult	result;
#ifdef TRACK_RESULTS
		static	int	next_thread;
		int		thread_number;
#endif
	};

public:
	RxMatch(const RxProgram& _program, StrVal _target);
	~RxMatch();

	const RxResult	matchAt(RxStationID start, CharNum& offset);

private:
	const RxProgram& program;		// The NFA to execute
	StrVal		target;			// The string we are searching
	RxResult	result;			// Result from the successful thread

	// Concurrent threads of execution:
	Thread*		current_stations;
	RxStationID	current_count;		// How many of the above are populated
	Thread*		next_stations;
	RxStationID	next_count;		// How many of the above are populated
	void		addthread(Thread thread, CharNum offset, CharNum *shunts, CharNum num_shunt, CharNum max_duplicates_allowed = 0);
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
	TRACK(("search_station %d start_station %d max_station %d max_counter %d max_capture %d, num_names %d, next %d\n",
		search_station, start_station, max_station, max_counter, max_capture, (int)names.size(), (int)(nfa_p-nfa)));
	assert(start_station == nfa_p-nfa);
}

RxProgram::~RxProgram()
{
	if (nfa && nfa_owned) delete(nfa);
}

const RxResult
RxProgram::matchAfter(StrVal target, CharNum offset) const
{
	RxMatch		match(*this, target);
	RxResult	result = match.matchAt(searchStation(), offset);
	return result;
}

const RxResult
RxProgram::matchAt(StrVal target, CharNum offset) const
{
	RxMatch		match(*this, target);
	RxResult	result = match.matchAt(startStation(), offset);
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
	int	i = program.maxStation();
	current_stations = new Thread[i];
	current_count = 0;
	next_stations = new Thread[i];
	next_count = 0;
}

RxMatch::~RxMatch()
{
	delete[] current_stations;
	delete[] next_stations;
}

RxMatch::Thread::Thread(RxStationID s, RxResult r)
: station(s)
, result(r)
#ifdef TRACK_RESULTS
, thread_number(s ? ++next_thread : 0)
#endif
{}

#ifdef TRACK_RESULTS
int	RxMatch::Thread::next_thread;
#endif

void
RxMatch::addthread(Thread thread, CharNum offset, CharNum* shunts, CharNum num_shunt, CharNum max_duplicates_allowed)
{
	// Recursion control, prevents looping over loops
	for (int i = 0; i < num_shunt; i++)
		if (shunts[i] == thread.station)
		{
			TRACK(("\t\tthread %d would recurse\n", thread.thread_number));
			return;
		}
	assert(num_shunt < RxMaxNesting);
	if (num_shunt == RxMaxNesting)
	{
		TRACK(("\t\tthread %d would exceed nesting limit\n", thread.thread_number));
		return;
	}
	shunts[num_shunt++] = thread.station;

	int	duplicates = 0;
	for (int i = 0; i < next_count; i++)
	{
		if (next_stations[i].station == thread.station)
		{
			if ((max_duplicates_allowed			// Check for an exact duplicate first
				&& thread.result.countersSame(next_stations[i].result))
			 || ++duplicates > max_duplicates_allowed)	// Or too many duplicates
			{
				// The above condition is not perfect - it fails with 5x nested counted repetitions for example.
				// Failing to find a solution in such a pathological case doesn't bother me.
				// Should we have a policy of deleting the duplicate with the lowest count?

#ifdef TRACK_RESULTS
				TRACK(("\t\tthread %d at %d is a duplicate", thread.thread_number, thread.station));
				if (thread.result.hasCounter()) TRACK((" (count %d)", thread.result.counterTop().count));
				TRACK((" of thread %d", next_stations[i].thread_number));
				if (next_stations[i].result.hasCounter())
				{
					TRACK((" (counters"));
					for (int i = 0; i < next_stations[i].result.counterNum(); i++)
						TRACK((" %d", next_stations[i].result.counterGet(i)));
					TRACK((")"));
				}
				TRACK(("\n"));
#endif
				return;		// We already have this thread
			}
		}
	}

	/*
	 * If the opcode at the new thread won't consume text, evaluate it either by looping (tail recursion) or a recursive call
	 */
	RxDecoded	instr;
next:
	program.decode(thread.station, instr);
	switch (instr.op)
	{
	case RxOp::RxoBOL:			// Beginning of Line
		if (offset == 0 || target[offset-1] == '\n')
		{
			thread.station = instr.next;
			goto next;
		}
		break;

	case RxOp::RxoEOL:			// End of Line
		if (offset == target.length() || target[offset] == '\n')
		{
			thread.station = instr.next;
			goto next;
		}
		break;

	case RxOp::RxoJump:
		thread.station = instr.next;
		goto next;

	case RxOp::RxoSplit:
		// REVISIT: For non-greedy repetition, we need these addthread calls in opposite order (same for RxoCount)
		addthread(Thread(instr.alternate, thread.result), offset, shunts, num_shunt);
		addthread(Thread(instr.next, thread.result), offset, shunts, num_shunt);
		break;

	case RxOp::RxoCaptureStart:
		thread.station = instr.next;
		TRACK(("\t\tthread %d saves start %d=%d\n", thread.thread_number, instr.capture_number, offset));
		thread.result = thread.result.captureSet(instr.capture_number*2, offset);
		goto next;

	case RxOp::RxoCaptureEnd:
		thread.station = instr.next;
		TRACK(("\t\tthread %d saves end %d=%d\n", thread.thread_number, instr.capture_number, offset));
		thread.result = thread.result.captureSet(instr.capture_number*2+1, offset);
		break;

	case RxOp::RxoZero:
		thread.result.counterPushZero(offset);
		thread = Thread(instr.next, thread.result);
		goto next;

	case RxOp::RxoCount:	// This instruction follows a repeated item, so the pre-incremented counter tells how many we've passed
	{
		RxResult::Counter	previous = thread.result.counterTop();

		short	counter = thread.result.counterIncr(offset);

		// Don't repeat unless the offset has increased since the last repeat (instead, just continue with next).
		if (previous.offset == offset)
			TRACK(("\t\tRepetition at shunt %d did not proceed from offset %d\n", thread.station, offset));

		if ((counter <= instr.repetition.max || instr.repetition.max == 0)	// Greedily repeat
		 && previous.offset <= offset)		// Only if we advanced
			addthread(Thread(instr.alternate, thread.result), offset, shunts, num_shunt, instr.repetition.min);

		if ((counter >= instr.repetition.min || previous.offset == offset)	// Reached minimum count, or will without advancing
		 && (instr.repetition.max == 0 || counter <= instr.repetition.max))
		{
			RxResult	continuation = thread.result;
			continuation.counterPop();	// The continuing thread has no further need of the counter
			addthread(Thread(instr.next, continuation), offset, shunts, num_shunt, instr.repetition.min);
		}
		thread.result.clear();
	}
		break;

	default:
#ifdef TRACK_RESULTS
		TRACK(("\t\t... adding thread %d at instr %d, offset %d with captures ", thread.thread_number, thread.station, offset));
		for (int i = 0; i < thread.result.captureMax(); i++)
			TRACK(("%s(%d,%d)", i ? ", " : "", thread.result.capture(i*2), thread.result.capture(i*2+1)));
		if (thread.result.hasCounter())
		{
			TRACK((" (counters"));
			for (int i = 0; i < thread.result.counterNum(); i++)
				TRACK((" %d", thread.result.counterGet(i)));
			TRACK((")"));
		}
		TRACK(("\n"));
#endif
		assert(next_count < program.maxStation());
		if (next_count < program.maxStation())
			next_stations[next_count++] = thread;
		// else the compiler let us down; silently ignore this possible path and hope something else matches correctly
	}
};

/*
 * Continue matching the target string from offset, from the NFA instruction at RxStationID.
 * Populate the RxMatch where necessary.
 */
const RxResult
RxMatch::matchAt(RxStationID start, CharNum& offset)
{
	Thread*		thread_p;
	RxDecoded	instr;
	CharNum		shunts[RxMaxNesting];	// Controls recursion in addthread

	// Start where directed, and step forward one character at a time, following all threads until successful or there are no threads left
	next_count = 0;
	addthread(Thread(start, RxResult(program)), offset, shunts, 0);
	std::swap(current_stations, next_stations);
	current_count = next_count;
	next_count = 0;
	for (; current_count > 0 && offset <= target.length(); offset++)
	{
		TRACK(("\ncycle at offset %d with %d threads looking at '%s'\n", offset, current_count, target.substr(offset, 1).asUTF8()));
		for (thread_p = current_stations; thread_p < current_stations+current_count; thread_p++)
		{
		decode_next:
			RxStationID	pc = thread_p->station;
			program.decode(pc, instr);
			UCS4		ch = target[offset];

#ifdef TRACK_RESULTS
			TRACK(("\tthread %d instr %d op %c", thread_p->thread_number, pc, (char)instr.op));
			if (instr.op == RxOp::RxoChar)
				TRACK((" '%c'", instr.character & 0xFF));
			if (thread_p->result.hasCounter())
				TRACK((", counter %d", thread_p->result.counterTop().count));
			TRACK((": "));
#endif
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
					result.captureSet(1, offset);	// Before we finalise it
					TRACK(("accepts selected (%d, %d)\n", new_result_start, offset));
				}
				else
					TRACK(("accepts but not preferred (%d, %d)\n", new_result_start, offset));
			}
				continue;

			case RxOp::RxoAny:			// Any single char
				// As long as we aren't at the end, all is good
				// REVISIT: Add except-newline mode behaviour
				if (offset < target.length())
				{
					TRACK(("succeeds\n"));
					break;
				}
				TRACK(("fails\n"));
				thread_p->result.clear();
				continue;

			case RxOp::RxoChar:			// A specific UCS4 character
				// REVISIT: Add case-insensitive mode behaviour
				if (ch == instr.character)
				{
					TRACK(("succeeds\n"));
					break;
				}
				TRACK(("fails\n"));
				thread_p->result.clear();
				continue;			// Dead-end this thread

			case RxOp::RxoCharClass:		// Character class
			case RxOp::RxoNegCharClass:		// Negated Character class
			{
				if (ch == 0)
					continue;

				// Check that the next characters match these ones
				StrBody		body(instr.text.utf8, false, instr.text.bytes);
				StrVal		expected(&body);
				CharNum		length = expected.length();	// Count the chars in the literal
				assert(length > 0);

				bool		matches_class = false;
				for (int i = 0; i < length; i += 2)
					if (ch >= expected[i] && ch <= expected[i+1])
					{
						matches_class = true;
						break;
					}
				if (matches_class != (instr.op == RxOp::RxoCharClass))
				{
					TRACK(("fails\n"));
					continue;
				}
				TRACK(("succeeds\n"));
				break;
			}

			case RxOp::RxoCharProperty:		// A character with a named Unicode property
			{
				// Check that the next characters match these ones
				StrBody		body(instr.text.utf8, false, instr.text.bytes);
				StrVal		expected(&body);
				CharNum		length = expected.length();	// Count the chars in the literal
				assert(length > 0);

				if (length == 1)
				{
					switch (expected[0])
					{
					case 's':
						if (UCS4IsWhite(ch))
							break;
						TRACK(("succeeds\n"));
						continue;

					case 'd':	// digit
						if (UCS4Digit(ch) >= 0)
							break;
						TRACK(("succeeds\n"));
						continue;

					case 'h':	// hex digit
						if (UCS4Digit(ch) >= 0 || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))
							break;
						TRACK(("succeeds\n"));
						continue;

					// REVISIT: Implement more built-in CharProperties?
					default:
						break;
					}
				}
				else
				{
					// REVISIT: Use a callback?
				}
				TRACK(("fails\n"));
				break;
			}

			case RxOp::RxoNegLookahead:		// (?!...) match until EndGroup and return the opposite
				// REVISIT: Implement RxoSubroutineCall. Until then, act like the negative lookahead has succeeded
				thread_p->station = instr.next;
				goto decode_next;
#if 0
			{
				CharNum 	start_offset = offset;	// Remember the target offset and reset it for each alternate
				const char*	next_p = nfa_p-1 + UTF8Get(nfa_p) + 1;	// Start of this instr, plus size, plus the EndGroup
				if (matchAt(match, bt, nfa_p, offset))
					return RxResult();
				// Not matched, as requested, so continue with the next instruction at the current offset
				offset = start_offset;
				nfa_p = next_p;
				thread_p->result.clear();
				continue;
			}
#endif

			case RxOp::RxoSubroutineCall:		// Subroutine call to a named group
				break;	// REVISIT: Implement RxoSubroutineCall. Until then, act like the subroutine has completed
#if 0
			{
				// Match the subroutine group and push_back subroutineMatches
				RxMatch		m;
				// matchAt(...
				return RxResult();
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

			// Shunts (handled in addthread)
			case RxOp::RxoBOL:			// Beginning of Line
			case RxOp::RxoEOL:			// End of Line
			case RxOp::RxoJump:
			case RxOp::RxoSplit:
			case RxOp::RxoCaptureStart:
			case RxOp::RxoCaptureEnd:
			case RxOp::RxoZero:
			case RxOp::RxoCount:
				assert(0 == "Should not happen");
				return RxResult();
			}
			addthread(Thread(instr.next, thread_p->result), offset+1, shunts, 0);
			thread_p->result.clear();
		}

		std::swap(current_stations, next_stations);
		current_count = next_count;
		next_count = 0;
	}

	// If we have a result, it's in `result`.
	// If not, we ran out of threads that could move forward, or ran out of input to move forward on.
	// REVISIT: Might it be useful to know on which Station(s) we ran out of input? Or callback to get more?

	// Wipe old result references, we already copied the one we chose:
	for (thread_p = current_stations; thread_p < current_stations+current_count; thread_p++)
		thread_p->result.clear();
	return result;
}

RxCaptures::RxCaptures(short c_max, short p_max)
: counter_max(c_max)
, capture_max(p_max)
, counters(new CharNum[(counter_max+capture_max)*2-1])
, counters_used(0)
{
	memset(counters, 0, sizeof(CharNum)*((counter_max+capture_max)*2-1));
}

RxCaptures::RxCaptures(const RxCaptures& to_copy)
: counter_max(to_copy.counter_max)
, capture_max(to_copy.capture_max)
, counters(new CharNum[(counter_max+capture_max)*2-1])
, counters_used(to_copy.counters_used)
{
	memcpy(counters, to_copy.counters, sizeof(CharNum)*((counter_max+capture_max)*2-1));
}

void
RxCaptures::counterPushZero(CharNum offset)
{
	assert(counters_used < counter_max*2);
	if (counters_used == counter_max*2)
		return;
	// TRACK(("Pushed new counter %d = 0\n", counters_used));
	counters[counters_used++] = offset;
	counters[counters_used++] = 0;
}

CharNum
RxCaptures::counterIncr(CharNum offset)
{
	assert(counters_used > 0);
	// TRACK(("Incrementing counter %d = %d\n", counters_used-1, counters[counters_used-1]+1));
	counters[counters_used-2] = offset;
	return ++counters[counters_used-1];
}

void
RxCaptures::counterPop()
{
	assert(counters_used > 1);
	if (counters_used <= 1)
		return;
	// TRACK(("Discarding counter %d (was %d)\n", counters_used-1, counters[counters_used-1]));
	counters_used -= 2;
}

const RxResult::Counter
RxCaptures::counterTop()
{
	assert(counters_used > 0);
	return {counters[counters_used-1], counters[counters_used-2]};
}

int
RxCaptures::captureMax() const
{
	return capture_max;
}

CharNum
RxCaptures::capture(int index)
{
	assert(!(index < 1 || index >= capture_max*2));		// REVISIT: Delete this when capture limiting is implemented
	if (index < 1 || index >= capture_max*2)
		return 0;
	return counters[counter_max*2+index-1];
}

CharNum
RxCaptures::captureSet(int index, CharNum val)
{
	assert(!(index < 1 || index >= capture_max*2));		// REVISIT: Delete this when capture limiting is implemented
	if (index < 1 || index >= capture_max*2)
		return 0;		// Ignore captures outside the defined range
	return counters[counter_max*2+index-1] = val;
}

RxResult::RxResult(const RxProgram& program)
: captures(new RxCaptures(program))
, cap0(0)
{
}

RxResult::RxResult()		// Failed result
: captures(0)
, cap0(0)
{
}

RxResult::RxResult(const RxResult& s1)
: captures(s1.captures)
, cap0(s1.cap0)
{	// Normal copy constructor
}

RxResult& RxResult::operator=(const RxResult& s1)
{
	captures = s1.captures;
	cap0 = s1.cap0;
	return *this;
}

void
RxResult::clear()
{
	captures = (RxCaptures*)0;	// Clear the reference and its refcount
	cap0 = 0;
}

void
RxResult::Unshare()
{
	assert(captures);
	if (captures.GetRefCount() <= 1)
		return;
	captures = new RxCaptures(*captures);	// Copy the result captures before making changes
}

RxResult::~RxResult()
{
}

int
RxResult::captureMax() const
{
	return captures ? captures->captureMax() : 1;
}

CharNum
RxResult::capture(int index) const
{
	if (index == 0)
		return cap0;
	assert(captures);
	return captures->capture(index);
}

RxResult&
RxResult::captureSet(int index, CharNum val)
{
	if (index == 0)
	{
		cap0 = val;
		return *this;
	}
	assert(captures);
	Unshare();
	captures->captureSet(index, val);
	return *this;
}

bool
RxResult::countersSame(RxResult& other) const
{
	if (counterNum() != other.counterNum())
		return false;
	for (int i = 0; i < counterNum(); i++)
		if (counterGet(i) != other.counterGet(i))
			return false;
	return true;
}

int
RxResult::counterNum() const	// How many counters are in use?
{
	return captures ? captures->counterNum() : 0;
}

int
RxResult::counterGet(int i) const	// get the nth top counter
{
	return captures ? captures->counterGet(i) : 0;
}

void
RxResult::counterPushZero(CharNum offset)	// Push a zero counter
{
	Unshare();
	assert(captures);
	captures->counterPushZero(offset);
}

CharNum
RxResult::counterIncr(CharNum offset)	// Increment and return top counter of stack
{
	Unshare();
	assert(captures);
	return captures->counterIncr(offset);
}

void
RxResult::counterPop()		// Discard the top counter of the stack
{
	Unshare();
	assert(captures);
	captures->counterPop();
}

const RxResult::Counter
RxResult::counterTop()
{
	return captures->counterTop();
}
