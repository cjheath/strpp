/*
 * Unicode Strings
 * Regular expression compiler
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<strregex.h>
#include	<string.h>

/*
 * Structure of the virtual machine:
 *
 * Each Instruction has a one-byte opcode followed by any parameters.
 * Number parameters are non-negative integers, encoded using UTF-8 encoding.
 * String parameters are a byte count (UTF-8 integer) followed by the UTF-8 bytes
 * An offset parameter is a byte offset within the NFA, relative to the start of the offset.
 * Offsets are stored using zigzag notation; LSB is the sign bit, shift right for the magnitude!
 *
 * An Instruction is either a Station or a Shunt. Threads stop at Stations, but never at Shunts.
 *
 * Stations:
 * - Char(UCS4)
 * - Character Class/Negated class(#bytes as UTF8, String) (The string is pairs of UTF8 characters, each representing a range)
 * - Character Property(#bytes as UTF8, String) (name of the Property)
 * - BOL/EOL (Line assertions)
 * - Any
 * Shunts:
 * - Start(Number of stations, Maximum Counter Nesting, offset to End, Number of names, array of names as consecutive Strings)
 * - Accept
 * - Capture Start(uchar capture#)
 * - Capture End(uchar capture#)
 * - Jump(Offset)
 * - Split(Offset to alternate path)
 * - Zero (push new zero counter to top of stack)
 * - Count(uchar min, uchar max, Offset to the repetition path)
 *
 * Negative lookahead requires a recursive call to the matcher
 *
 * Any Instruction that consumes text is counted as a Station. Start, End, Jump, Split, Zero, Count, Success are not stations.
 * There may be as many as one parallel thread per Station in the NFA.
 */

#define	ParenFeatures		((RxFeature)((int32_t)RxFeature::Capture|(int32_t)RxFeature::NonCapture|(int32_t)RxFeature::NegLookahead))

class RxToken
{
public:
	RxOp		op;		// The instruction code
	RxRepetitionRange repetition;	// How many repetitions?
	StrVal		str;		// RxoNamedCapture, RxoCharClass, RxoNegCharClass

	RxToken(RxOp _op) : op(_op) { }
	RxToken(RxOp _op, StrVal _str) : op(_op), str(_str) { }
	RxToken(RxOp _op, int min, int max) : op(_op) { repetition.min = min; repetition.max = max; }
};

RxCompiler::RxCompiler(StrVal _re, RxFeature features, RxFeature reject_features)
: re(_re)
, features_enabled((RxFeature)((int32_t)features & ~(int32_t)reject_features))
, features_rejected(reject_features)
, error_message(0)
, nfa_size(0)
{
}

RxCompiler::~RxCompiler()
{
}

bool RxCompiler::supported(RxFeature feat)
{
	if (((int32_t)features_rejected & (int32_t)feat) != 0)
	{
		// It would be nice to know the feature here, but we don't have format() yet
		error_message = "Rejected feature";
		return false;
	}
	return enabled(feat);
}

bool RxCompiler::enabled(RxFeature feat) const
{
	return ((int32_t)features_enabled & (int32_t)feat) != 0;
}

/*
 * Ok ima apologise right now for the nested switch statements and especially the gotos.
 * But I have been very careful about how those work, and it would have made the code
 * hugely more complicated and incomprehensible to do it any other way.
 * If you reckon you can improve on it, you're welcome to try.
 */
bool RxCompiler::scanRegex(const std::function<bool(const RxToken& instr)> func)
{
	int		i = 0;		// Regex character offset
	UCS4		ch;		// A single character to match
	StrVal		param;		// A string argument
	int		close;		// String offset of the character ending a param
	ErrNum		int_error;
	StrValIndex 	scanned;
	bool		ok = true;
	int		min, max;
	bool		char_class_negated;
	StrVal		delayed;	// A sequence of ordinary characters to be matched.
	auto		flush = [&delayed, func]() {
				if (delayed.length() > 0)
				{
					bool	ok = func(RxToken(RxOp::RxoLiteral, delayed));
					delayed = StrVal::null;
					return ok;
				}
				return true;
			};

	ok = func(RxToken(RxOp::RxoStart));
	if (!ok) error_offset = i;
	for (; ok && i < re.length(); i++)
	{
		switch (ch = re[i])
		{
		case '^':
			if (!supported(RxFeature::BOL))
				goto simple_char;
			if (!(ok = flush())) break;
			ok = func(RxToken(RxOp::RxoBOL));
			break;

		case '$':
			if (!supported(RxFeature::EOL))
				goto simple_char;
			if (!(ok = flush())) break;
			ok = func(RxToken(RxOp::RxoEOL));
			break;

		case '.':
			if (enabled(RxFeature::AnyIsQuest))
				goto simple_char;	// if using ?, . is a simple char
			if (!(ok = flush())) break;
			ok = func(RxToken(RxOp::RxoAny));
			break;

		case '?':
			if (!(ok = flush())) break;
			if (enabled(RxFeature::AnyIsQuest))
				ok = func(RxToken(RxOp::RxoAny));	// Shell-style any-char wildcard
			else
				ok = func(RxToken(RxOp::RxoRepetition, 0, 1));	// Previous elem is optional
			break;

		case '*':
			if (!supported(RxFeature::ZeroOrMore))
				goto simple_char;
			if (!(ok = flush())) break;
			if (enabled(RxFeature::ZeroOrMoreAny))
				ok = func(RxToken(RxOp::RxoAny));
			ok = func(RxToken(RxOp::RxoRepetition, 0, 0));
			break;

		case '+':
			if (!supported(RxFeature::OneOrMore))
				goto simple_char;
			if (!(ok = flush())) break;
			ok = func(RxToken(RxOp::RxoRepetition, 1, 0));
			break;

		case '{':
			if (!supported(RxFeature::CountRepetition))
				goto simple_char;
			if (!(ok = flush())) break;
			param = re.substr(++i);
			min = max = 0;
			// Find the end of the first number:
			close = param.find(',');
			if (close < 0)
				close = param.find('}'); // Just one number?
			if (close < 0)
			{
		bad_repetition:	error_message = "Bad repetition count";
				break;
			}
			if (close > 0)
			{
				min = param.substr(0, close).asInt32(&int_error, 10, &scanned);
				if ((int_error && int_error != STRERR_NO_DIGITS) || min < 0)
					goto bad_repetition;
			}
			i += close;	// Skip the ',' or '}'

			if (re[i] == ',')		// There is a second number
			{
				param = re.substr(++i);
				close = param.find('}');
				if (close < 0)
					goto bad_repetition;
				max = param.substr(0, close).asInt32(&int_error, 10, &scanned);
				if ((int_error && int_error != STRERR_NO_DIGITS) || (max && max < min))
					goto bad_repetition;
				i += close;
			}
			else
				max = min;		// Exact repetition count

			ok = func(RxToken(RxOp::RxoRepetition, min, max));
			break;

		case '\\':		// Escape sequence
			switch (ch = re[++i])
			{
			case 'b': if (!enabled(RxFeature::CEscapes)) goto simple_escape; ch = '\b'; goto escaped_char;
			case 'e': if (!enabled(RxFeature::CEscapes)) goto simple_escape; ch = '\033'; goto escaped_char;
			case 'f': if (!enabled(RxFeature::CEscapes)) goto simple_escape; ch = '\f'; goto escaped_char;
			case 'n': if (!enabled(RxFeature::CEscapes)) goto simple_escape; ch = '\n'; goto escaped_char;
			case 't': if (!enabled(RxFeature::CEscapes)) goto simple_escape; ch = '\t'; goto escaped_char;
			case 'r': if (!enabled(RxFeature::CEscapes)) goto simple_escape; ch = '\r'; goto escaped_char;

			case '0': case '1': case '2': case '3':	// Octal char constant
			case '4': case '5': case '6': case '7':
				if (!enabled(RxFeature::OctalChar))
					goto simple_escape;
				param = re.substr(i, 3);
				ch = param.asInt32(&int_error, 8, &scanned);
				if ((int_error && int_error != STRERR_TRAIL_TEXT) || scanned == 0)
				{
					error_message = "Illegal octal character";
					break;
				}
				i += scanned-1;
				goto escaped_char;

			case 'x':			// Hex byte (1 or 2 hex digits follow)
				if (!enabled(RxFeature::HexChar))
					goto simple_escape;
				// REVISIT: Support \x{XX}?
				param = re.substr(++i, 2);
				ch = param.asInt32(&int_error, 16, &scanned);
				if ((int_error && int_error != STRERR_TRAIL_TEXT) || scanned == 0)
				{
					error_message = "Illegal hexadecimal character";
					break;
				}
				i += scanned-1;
				goto escaped_char;

			case 'u':			// Unicode (1-5 hex digits follow)
				if (!enabled(RxFeature::UnicodeChar))
					goto simple_escape;
				// REVISIT: Support \u{XX}?
				param = re.substr(++i, 5);
				ch = param.asInt32(&int_error, 16, &scanned);
				if ((int_error && int_error != STRERR_TRAIL_TEXT) || scanned == 0)
				{
					error_message = "Illegal Unicode escape";
					break;
				}
				i += scanned-1;	// The loop will advance one character
		escaped_char:	ok = true;
				goto delay_escaped;

			case 's':			// Whitespace
			case 'd':			// Digit
			case 'h':			// Hexdigit
				if (!enabled(RxFeature::Shorthand))
					goto simple_escape;
				if (!(ok = flush())) break;
				// REVISIT: Also support \w, etc
				ok = func(RxToken(RxOp::RxoCharProperty, StrVal(ch)));
				break;

			case 'p':			// Posix character type
				if (!enabled(RxFeature::PropertyChars))
					goto simple_escape;
				if (!(ok = flush())) break;
				if (re[++i] != '{')
				{
		bad_posix:		error_message = "Illegal Posix character specification";
					break;
				}
				param = re.substr(i+1);
				close = param.find('}');
				if (close <= 0)
					goto bad_posix;
				param = param.substr(0, close); // Truncate name to before '}'
				i += close+1;	// Now pointing at the '}'
				ok = func(RxToken(RxOp::RxoCharProperty, param));
				break;

			default:
		simple_escape:
				ch = re[i];
		delay_escaped:
				switch (re[i+1])	// Can't delay if this escape is repeated or optional
				{	// Note this is conservative; these repetitions might not be enabled
				case '*': case '+': case '?': case '{':
					if (!(ok = flush())) break;	// So flush it first
				}
				delayed += ch;
				break;
			}
			break;

		case '[':		// Character class
			if (!supported(RxFeature::CharClasses))
				goto simple_char;
			if (!(ok = flush())) break;

			/*
			 * A Character class is compiled as a string containing pairs of characters.
			 * Each pair determines an (inclusive) subrange of UCS4 characters.
			 * REVISIT: Use a non-Unicode UCS4 character to store a Character Property group number
			 */
			char_class_negated = re[++i] == '^';
			if (char_class_negated)
				i++;

			param = StrVal::null;

			// A char class that starts with - or ] mean those literally:
			ch = re[i];
			if (ch == '-' || ch == ']')
			{
				param += ch;
				param += ch;
				ch = re[++i];
			}
			while (ch != '\0' && ch != ']')
			{
				if (ch == '\\')		// Any single Unicode char can be escaped
				{
					if ((ch = re[++i]) == '\0')
						goto bad_class; // RE ends with the backslash
					// REVISIT: Support \escapes in character classes? Which ones?
				}

				// REVISIT: Add support for Posix groups or Unicode Properties

				param += ch;	// Start of range pair
				if (re[i+1] == '-' && re[i+2] != ']')	// Allow a single - before the closing ]
				{		// Character range
					if ((ch = re[i += 2]) == '\0')
						goto bad_class;
					if (ch == '\\')		// Any single Unicode char can be escaped
					{
						if ((ch = re[++i]) == '\0')
							goto bad_class; // RE ends with the backslash
						// REVISIT: Support \escapes in character classes? Which ones?
					}
				}
				param += ch;
				ch = re[++i];
			}
			if (ch == '\0')
				goto bad_class;

			ok = func(RxToken(char_class_negated ? RxOp::RxoNegCharClass : RxOp::RxoCharClass, param));
			break;

	bad_class:	error_message = "Bad character class";
			break;

		case '|':
			if (!supported(RxFeature::Alternates))
				goto simple_char;
			if (!(ok = flush())) break;
			ok = func(RxToken(RxOp::RxoAlternate));
			break;

		case '(':
			if (!supported(RxFeature::Group))
				goto simple_char;
			if (!(ok = flush())) break;
			if (!enabled(ParenFeatures)
			 || re[i+1] != '?')					// or this one is plain
			{		// Anonymous group
				ok = func(RxToken(RxOp::RxoNonCapturingGroup));
				break;
			}

			i += 2;	// Skip the '(?'
			if (supported(RxFeature::Capture) && re[i] == '<')		// Capture
			{
				param = re.substr(++i);
				close = param.find('>');
				if (close <= 0)
				{
		bad_capture:		error_message = "Invalid group name";
					break;
				}
				param = param.substr(0, close);		// Truncate name to before >
				i += close;	// Point at the >
				ok = func(RxToken(RxOp::RxoNamedCapture, param));
				break;
			}
			else if (supported(RxFeature::NonCapture) && re[i] == ':')	// Noncapturing group
			{
				ok = func(RxToken(RxOp::RxoNonCapturingGroup));
				break;
			}
			else if (supported(RxFeature::NegLookahead) && re[i] == '!') // Negative Lookahead
			{
				ok = func(RxToken(RxOp::RxoNegLookahead));
				break;
			}
			else
				error_message = "Illegal group type";
			break;

		case ')':
			if (!enabled(RxFeature::Group))
				goto simple_char;
			if (!(ok = flush())) break;
			ok = func(RxToken(RxOp::RxoEndGroup));
			break;

		case ' ': case '\t': case '\n': case '\r':
			if (!enabled(RxFeature::ExtendedRE))
				goto simple_char;
			if (re[i+1] == '#')	// Comment to EOL
				while (++i < re.length() && re[i] != '\n')
					;
			break;
			
		default:
		simple_char:
			switch (re[i+1])	// Can't delay if this escape is repeated or optional
			{	// Note this is conservative; these repetitions might not be enabled
			case '*': case '+': case '?': case '{':
				if (!(ok = flush())) break;	// So flush it first
			}

			delayed += re.substr(i, 1);
			break;
		}
		if (!ok || error_message)
			error_offset = i;
	}
	if (ok)
		ok = flush();
	if (ok && !error_message && i >= re.length())	// If we didn't complete, there was an error
	{
		ok = func(RxToken(RxOp::RxoAccept));
		return ok;
	}
	return false;
}

/*
 * Compilation strategy is as follows:
 * - Scan through the RE once, accumulating the maximum required size for the NFA and error-checking
 * - In the first pass, we:
 *   + build an array of all named groups
 *   + accumulate the maximum nesting of repetitions (actuall, just maximum nesting)
 *   + Count the number of Stations
 * - Allocate the data and scan through again, building the compiled NFA
 * - The RxoStart node includes the list of group names.
 *
 * The finished NFA is a series of packed characters and numbers. Both are packed using UTF-8 coding.
 * The opcodes are all ASCII (single UTF-8 byte)
 * Signed numbers are stored using zig-zag coding (sign bit is LSB, magnitude is doubled)
 *
 * As the NFA is built, there are places where numbers (especially station numbers aka NFA offsets)
 * that aren't yet known must be later patched in.  Because the UTF-8 encoding of these numbers is
 * variable in size, the maximum required space is reserved, so patching involves shuffling the rest
 * of the NFA down to meet it. This must be done in such away that other offsets aren't affected, or
 * those are also patched.
 *
 * The first pass involves two counts that affect the allocation size:
 * - the number of offset numbers to encode, and
 * - the number of other bytes.
 * If this indicates that all offsets will fit in 1 byte (or 2 bytes, etc) the maximum required
 * size is calculated accordingly.
 */
bool
RxCompiler::compile(char*& nfa)
{
	// Each stack entry contains enough information to find the start station,
	// and to close off the group, including finalising a chain of alternates.
	struct Stack {
		RxOp		op;
		uint8_t		group_num;	// 0 = the whole NFA. 1 is the first named group
		StrValIndex	start;		// Index in NFA to group-start opcode (2nd pass) or boolean (1st pass)
		StrValIndex	contents;	// Index in NFA to the start of the group contents
		StrValIndex	last_jump;	// Index in NFA to the Jump at the end of the most recent alternate. Signals an alternate chain.
	} stack[RxMaxNesting];
	int		nesting = 0;		// Stack pointer
	auto		tos = [&]() -> Stack& { return stack[nesting-1]; };	// Top of stack is stack[nesting-1]
	auto		group_op = [&]() -> RxOp { return tos().op; };		// What kind of group is at the top?
	int		max_nesting = 0;	// How deeply are groups nested? (we actually need to know the count of nested repetitions)
	int		station_count = 0;	// How many parallel threads can a nesting-first search have?
	std::vector<StrVal>	names;		// Sequence of group names in order of occurrence
	std::vector<StrVal>::iterator	iter;	// An iterator for the group names
	UTF8*		ep;			// NFA output pointer, not used until 2nd pass

	error_message = 0;			// Start with a bit of optimism
	error_offset = 0;

	/*
	 * In first pass, we must count how many offsets and how many other bytes are needed to store the NFA
	 */
	StrValIndex	bytes_required = 0;	// Count how many bytes we will need
	StrValIndex	offsets_required = 0;	// Count how many UTF8-encoded offsets we will need (these may vary in size)
	// Adjust for the size of a string to be emitted
	auto	add_string_size =
		[&](StrVal str)
		{
			StrValIndex	utf8_count;	// Used for sizing string storage
			(void)str.asUTF8(utf8_count);
			bytes_required += UTF8Len(utf8_count);	// Length encoded as UTF-8
			bytes_required += utf8_count;	// UTF8 bytes
		};
	auto	add_string_storage =			// Just the string bytes, excluding the length
		[&](StrVal str)
		{
			StrValIndex	utf8_count;	// Used for sizing string storage
			(void)str.asUTF8(utf8_count);
			bytes_required += utf8_count;	// UTF8 bytes
		};

	auto	push =
		[&](RxOp op, int group_num = 0) -> bool
		{
			if (nesting >= RxMaxNesting)
				return false;
			stack[nesting].op = op;
			stack[nesting].group_num = group_num;
			stack[nesting].start = ep-nfa-1;	// Always called immediately after the RxOp has been emitted.
			stack[nesting].contents = ep-nfa;
			stack[nesting].last_jump = 0;
			nesting++;
			if (max_nesting < nesting)
				max_nesting = nesting;
			return true;
		};

	bool		repeatable = false;	// Cannot start with a repetition
	nfa = ep = 0;
	auto	first_pass =
		[&](const RxToken& instr) -> bool
		{
			bool		is_atom = false;
			bytes_required++;			// One byte to store this RxOp

			switch (instr.op)
			{
			case RxOp::RxoNull:
			case RxOp::RxoChar:
			case RxOp::RxoJump:
			case RxOp::RxoSplit:
			case RxOp::RxoCaptureStart:
			case RxOp::RxoCaptureEnd:
			case RxOp::RxoCount:
			case RxOp::RxoZero:
				break;				// Never sent

			case RxOp::RxoStart:			// Place to start the DFA
				bytes_required += 2+3;		// RxoCaptureStart 0, max_nesting, max_capture, num_names
				offsets_required += 3;		// search_station, start_station, station_count
				// Names will get added later
				station_count = 0;
				push(instr.op, 0);
				break;

			case RxOp::RxoAccept:			// Termination condition
				// RxoAny/RxoSplit/RxoJump for search_station, and a trailing NUL
				bytes_required += 3+1;
				offsets_required += 2;
				station_count++;
				nesting--;
				break;

			case RxOp::RxoLiteral:			// A specific string
				is_atom = true;
				bytes_required += instr.str.length();	// One RxoChar for each char
				bytes_required += instr.str.numBytes();	// and the data
				// REVISIT: This is pessimistic. A string has one station plus the number of times its prefix re-occurs
				station_count += instr.str.numBytes();
				break;

			case RxOp::RxoBOL:			// Beginning of Line
				station_count++;
				break;				// Nothing special to see here, move along

			case RxOp::RxoEOL:			// End of Line
				station_count++;
				break;				// Nothing special to see here, move along

			case RxOp::RxoAny:			// Any single char
				station_count++;
				is_atom = true;
				break;				// Nothing special to see here, move along

			case RxOp::RxoCharClass:		// Character class; instr contains a string of character pairs
			case RxOp::RxoNegCharClass:		// Negated Character class
			case RxOp::RxoCharProperty:		// A char with a named Unicode property
				station_count++;
				is_atom = true;
		str_param:	add_string_size(instr.str);
				break;

			case RxOp::RxoNonCapturingGroup:	// (...)
				if (!push(instr.op, 0))
				{
		too_deep:		error_message = "Nesting too deep";
					return false;
				}
				break;

			case RxOp::RxoNegLookahead:		// (?!...)
				station_count++;
				if (!push(instr.op, 0))
					goto too_deep;
				offsets_required++;		// Offset to the continuation point
				break;

			case RxOp::RxoNamedCapture:		// (?<name>...)
				/*
				 * The name must be unique. Add it to the names table
				 */
				if (names.size() >= 254)
				{
					error_message = "Too many named groups";
					return false;
				}
				for (iter = names.begin(); iter != names.end(); iter++)
					if (*iter == instr.str)
					{
						error_message = "Duplicate name";
						return false;
					}
				names.push_back(instr.str);

				if (!push(instr.op, names.size()))
					goto too_deep;

				bytes_required += 2;		// RxoCaptureStart, group#
				goto str_param;			// This string will be in the names table of RxoStart

			case RxOp::RxoEndGroup:			// End of a group
				if (nesting <= 0)
				{
					error_message = "Too many closing parentheses";
					return false;
				}

				switch (group_op())
				{
				default:	// Internal error
					return false;
				case RxOp::RxoNegLookahead:
					break;

				case RxOp::RxoNonCapturingGroup:
					bytes_required--;	// Delete the RxoEndGroup
					break;

				case RxOp::RxoNamedCapture:
					bytes_required--;	// Replace the RxoEndGroup
					bytes_required += 2;	// with RxoCaptureEnd+group#
					break;
				}

				nesting--;	// Pop the stack
				is_atom = true;
				break;

			case RxOp::RxoAlternate:		// |
				offsets_required++;
				if (tos().start)		// First alternate: Emit Jump, insert Split before the group contents
				{
					tos().start = 0;	// After first alternate now
					bytes_required++;
					offsets_required += 2;
					// REVISIT: This fixes a test for matching /|/. Is it needed only after an empty alternate? Why? Can we even detect that here?
					station_count++;		// Apparently needed only for /|/; REVISIT: Audit station_count estimates
				}
				else				// non-first alternate. Emit RxoJump, insert RxoSplit after previous Jump
				{
					bytes_required += 2;
					offsets_required += 2;
				}
				break;

			case RxOp::RxoRepetition:		// {n, m}
				if (!repeatable)
				{
					error_message = "Repeating a repetition is disallowed";
					return false;
				}
				if (instr.repetition.min > 254 || instr.repetition.max > 254)
				{
					error_message = "Min and Max repetition are limited to 254";
					return false;
				}

				if (instr.repetition.min == 0 && instr.repetition.max == 0)
				{
					bytes_required += 1;	// An inserted Split and a Jump
					offsets_required += 2;
				}
				else if (instr.repetition.min == 0 && instr.repetition.max == 1)
				{
					//bytes_required += 0;	// An inserted Split
					offsets_required += 1;
				}
				else if (instr.repetition.min == 1 && instr.repetition.max == 0)
				{
					offsets_required += 1;	// Just a Split
				}
				else
				{
					station_count += instr.repetition.min;
					bytes_required += 3;	// A Zero and a Count
					offsets_required += 1;
				}
				break;
			}
			repeatable = is_atom;
			return true;
                };

	bool		ok;			// Is everything still ok to continue?
	ok = scanRegex(first_pass);
	if (ok && nesting > 0)
	{
		error_message = "Not all groups were closed";
		ok = false;
	}
	if (!ok)
		return false;

	// What is a maximum sufficient size (in bytes) of an offset, if all offsets were the maximum 4 bytes?
	// Note that zig-zag encoding means an offset may be twice the maximum size of the NFA.
	StrValIndex	offset_max_bytes = UTF8Len(2*(bytes_required+offsets_required*4));	// Upper limit on offset size
	offset_max_bytes = UTF8Len(2*(bytes_required+offsets_required*offset_max_bytes));	/// Based on actual offset size

	// The maximum size of the nfa is thus (bytes_required+offsets_required*offset_max_bytes)
	// Now we know this, we know the maximum size of an actual offset and can get close to the actual NFA size.
	// This might be a tad high as some offsets might shrink, but not by much.
	bytes_required += offsets_required * UTF8Len(bytes_required+offsets_required*offset_max_bytes);
	// printf("Allocating %d bytes for the NFA\n", bytes_required);

	nfa = new UTF8[bytes_required+1];
	ep = nfa;

	auto	zigzag =
		[](int32_t i) -> StrValIndex
		{
			return (StrValIndex)(abs(i)<<1) | (i < 0 ? 1 : 0);
		};

	auto	zagzig =
		[](int32_t i) -> int32_t
		{
			return (i>>1) * ((i&01) ? -1 : 1);
		};

	// Emit an offset, advancing np, and returning the number of UTF8 bytes emitted
	auto	emit_offset =
		[&](char*& np, int offset) -> StrValIndex
		{
			char*	cp = np;

			UTF8Put(np, zigzag(offset));
			return (StrValIndex)(np-cp);
		};

	// Emit an offset, advancing np, and returning the number of UTF8 bytes emitted
	auto	get_offset =
		[&](const char*& np) -> int32_t
		{
			return zagzig(UTF8Get(np));
		};

	// Emit a zigzag integer, but always advance np by offset_max_bytes
	auto	emit_padded_offset =
		[&](char*& np, int offset = 0)
		{
			char*	cp = np;
			memset(cp, 0, offset_max_bytes);	// Ensure the entire space is zero
			UTF8Put(cp, zigzag(offset));		// But emit only what we need
			np += offset_max_bytes;			// Reserve all the space
		};

	// Emit a StrVal, advancing ep
	auto	emit_string =
		[&](StrVal str)
		{
			const UTF8*	sp;		// The UTF8 bytes of a string we'll copy into the NFA
			StrValIndex	utf8_count;	// Used for sizing string storage
			sp = str.asUTF8(utf8_count);	// Get the bytes and byte count
			UTF8Put(ep, utf8_count);	// Encode the byte count as UTF-8
			memcpy(ep, sp, utf8_count);	// Output the bytes to the NFA
			ep += utf8_count;		// And advance past them
		};

	auto	patch_value_at =
		[&](StrValIndex patch_location, int value) -> int
		{
			StrValIndex	byte_count = UTF8Len(zigzag(value));
			StrValIndex	shrinkage = offset_max_bytes - byte_count;

			// printf("Patching %d with value %d (offs to %d), with %d shrinkage\n", patch_location, value, (patch_location+value), shrinkage);

			char*	cp = nfa+patch_location;	// The patch location
			UTF8Put(cp, zigzag(value));		// Patch done
			if (shrinkage)
			{		// Shuffle down, this offset is smaller than the offset_max_bytes
				// We reserved offset_max_bytes, but only need byte_count.
				memmove(cp, cp+shrinkage, ep-(cp+shrinkage));
				ep -= shrinkage;	// the nfa contains fewer bytes now
			}
			return shrinkage;
		};

	auto	patch_offset =
		[&](StrValIndex patch_location, StrValIndex offset_to) -> int
		{
			/*
			 * We reserved offset_max_bytes at patch_location for an offset to the current location.
			 *
			 * Patch the offset there to point to offset_to.
			 * If the offset fits in fewer than offset_max_bytes UTF-8 bytes, we must shuffle down.
			 * This reduces the offset, which can cause it to cross a UTF-8 coding boundary,
			 * taking one fewer bytes, a process that can only happen once(?).
			 */
			int		offset = offset_to - patch_location;
			StrValIndex	byte_count = UTF8Len(zigzag(offset));
			StrValIndex	shrinkage = offset_max_bytes - byte_count;
			if (byte_count < offset_max_bytes	// We'll shuffle
			 && patch_location < offset_to)		// And the patch location is prior to the target
				offset -= shrinkage;		// The offset is now smaller

			return patch_value_at(patch_location, offset);
		};

	auto	insert_split =
		[&](StrValIndex location)
		{		// The new split is 1 byte & 1 offset. Make room for it
			char*	new_split = nfa+location;
			memmove(new_split+1+offset_max_bytes, new_split, ep-new_split);
			ep += 1+offset_max_bytes;
			*new_split++ = (char)RxOp::RxoSplit;
			emit_padded_offset(new_split);			// and make room for it
		};

	auto	fixup_alternates =
		[&]()
		{
			if (tos().last_jump == 0)
				return;

			/*
			 * Patching must occur strictly backwards, since each patch might shrink the NFA
			 * We patch the last Jump (after using its value to find the previous Jump&Split),
			 * then the previous Split, then the associated Jump, and continue.
			 * The last Jump points nowhere, but there is one more Split at the start of the
			 * group contents.
			 */
			StrValIndex	jump = tos().last_jump;	// location of the offset inside the last Jump

			for (;;)
			{
				const char*	cp = nfa+jump;
				int		prev = get_offset(cp);	// Get offset to previous jump

				int		shrinkage;
				assert(nfa[jump-1] == (char)RxOp::RxoJump);
				shrinkage = patch_offset(jump, (ep-nfa));	// Make the jump point to the end of the NFA
				StrValIndex	prev_split = prev ? jump+prev+(int)offset_max_bytes+1 : tos().contents+1;
				assert(nfa[prev_split-1] == (char)RxOp::RxoSplit);
				// Patch previous split to point at the split after the jump we just patched
				patch_offset(prev_split, jump+offset_max_bytes-shrinkage);

				if (!prev)
					break;
				assert(prev < 0);
				jump += prev;
			}
		};

	UTF8*		last_atom_start = ep;	// Pointer to the thing to which a repetition applies
	int		next_group = 0;	// Number of the next named group

	auto	second_pass =
		[&](const RxToken& instr) -> bool
		{
			char*	this_atom_start = ep;		// Pointer to the thing a following repetition will repeat
			*ep++ = (char)instr.op;
			switch (instr.op)
			{
			case RxOp::RxoChar:
			case RxOp::RxoJump:
			case RxOp::RxoSplit:
			case RxOp::RxoCaptureStart:
			case RxOp::RxoCaptureEnd:
			case RxOp::RxoNull:
			case RxOp::RxoCount:
			case RxOp::RxoZero:
				break;				// Never sent

			case RxOp::RxoStart:			// Place to start the DFA
				nesting = 0;			// Clear the stack
				push(instr.op, next_group++);	// and open this "group"
				emit_padded_offset(ep);		// search_station; will be patched on RxoAccept
				emit_padded_offset(ep);		// start_station
				emit_offset(ep, station_count);
				*ep++ = max_nesting;		// REVISIT: Not all nesting involves repetition that needs counters. But how to count nested repetitions?
				*ep++ = names.size() + 1;	// max_capture; 0 is the overall match, first group starts at 1
				*ep++ = names.size() + 1;	// num_names
				for (iter = names.begin(); iter != names.end(); iter++)
					emit_string(*iter);
				patch_offset(1+offset_max_bytes, ep-nfa);	// start_station

				*ep++ = (char)RxOp::RxoCaptureStart;
				*ep++ = 1;
				tos().contents = ep-nfa;	// Record where the first alternate must start
				break;

			case RxOp::RxoAccept:			// Termination condition
			{
				ep--;
				fixup_alternates();

				// The CaptureEnd is built-in to Accept handling
				// *ep++ = (char)RxOp::RxoCaptureEnd;
				// *ep++ = 0;

				*ep++ = (char)RxOp::RxoAccept;

				// Fixup RxoStart block
				const char*	cp = nfa+1+offset_max_bytes;	// Location of start_station
				cp -= patch_offset(1, ep-nfa);	// Patch search_station 

				StrValIndex	start_station = (cp-nfa)+get_offset(cp);

				// Add .* sequence for a search:
				cp = ep;
				*ep++ = (char)RxOp::RxoSplit;
				emit_offset(ep, (nfa+start_station)-ep);
				*ep++ = (char)RxOp::RxoAny;
				*ep++ = (char)RxOp::RxoJump;
				emit_offset(ep, cp-ep);
			}
				break;

			case RxOp::RxoBOL:			// Beginning of Line
				break;				// Nothing special to see here, move along
			case RxOp::RxoEOL:			// End of Line
				break;				// Nothing special to see here, move along
			case RxOp::RxoAny:			// Any single char
				break;				// Nothing special to see here, move along

			case RxOp::RxoLiteral:			// A specific string
				ep--;
				for (int i = 0; i < instr.str.length(); i++)
				{		// Emit each character one at a time
					*ep++ = (char)RxOp::RxoChar;
					UTF8Put(ep, instr.str[i]);
				}
				break;

			case RxOp::RxoCharProperty:		// A char with a named Unicode property
			case RxOp::RxoCharClass:		// Character class; instr contains a string of character pairs
			case RxOp::RxoNegCharClass:		// Negated Character class
				emit_string(instr.str);
				break;

			case RxOp::RxoNonCapturingGroup:	// (...)
				push(instr.op, 0);
				ep--;
				tos().contents = ep-nfa;
				break;

			case RxOp::RxoNamedCapture:		// (?<name>...)
				push(instr.op, ++next_group);
				ep[-1] = (char)RxOp::RxoCaptureStart;
				*ep++ = next_group;
				tos().contents = ep-nfa;	// Record where the first alternate must start
				break;

			case RxOp::RxoNegLookahead:		// (?!...)
				push(instr.op, 0);
				emit_padded_offset(ep);		// Pointer to next station if the lookahead allows us to proceed
				tos().contents = ep-nfa;	// Record where the first alternate must start
				break;

			case RxOp::RxoEndGroup:			// End of a group
				ep--;
				switch (group_op())
				{
				default:	// Internal error
					assert(0 == "Internal error in group management");
					return false;

				case RxOp::RxoNegLookahead:
					fixup_alternates();
					*ep++ = (char)RxOp::RxoAccept;	// Stops the recursion on the lookahead
					// Patch the continuation pointer in the lookahead to point here
					patch_offset(tos().start+1, ep-nfa);
					break;

				case RxOp::RxoNonCapturingGroup:
					fixup_alternates();
					break;

				case RxOp::RxoNamedCapture:
					fixup_alternates();
					*ep++ = (char)RxOp::RxoCaptureEnd;
					*ep++ = tos().group_num;
					break;
				}
				this_atom_start = nfa+tos().start;
				nesting--;
				break;		// Don't break and set last_atom_start wrongly

			case RxOp::RxoAlternate:		// |
				ep--;	// Don't output RxoAlternate

				/*
				 * Each new alternate inserts a split at the start of the previous alternate, and a
				 * Jump at the end of this one. The jumps are backward-chained during construction, and
				 * at end of group, we walk backward, patching final offsets and compacting the NFA.
				 * The first split remains at offset tos().contents
				 */
				if (tos().last_jump == 0)
				{		// Reached the end of the first alternate.
					insert_split(tos().contents);	// Insert a split at start of group contents

					// Append a new jump with a zero value to terminate a new list
					*ep++ = (char)RxOp::RxoJump;
					tos().last_jump = ep-nfa;	// The next jump will point back to this one
					emit_padded_offset(ep);
				}
				else
				{		// End of subsequent alternate. tos().last_jump is where we'll find the previous alternate
					StrValIndex	last_jump = tos().last_jump;

					// We need to insert a new split after the last jump whose offset is at last_jump
					insert_split(last_jump+offset_max_bytes);		// We'll find this and patch it later

					// Append a new jump that points back to the previous one
					*ep++ = (char)RxOp::RxoJump;
					tos().last_jump = ep-nfa;
					emit_padded_offset(ep, last_jump-(ep-nfa));
				}
				break;

			case RxOp::RxoRepetition:		// {n, m}
				ep--;

				/*
				 * Cases:
				 * No MIN, no MAX: Insert a Split (to the continuation) at the start, append a Jump back to the Split
				 * No MIN, 1 MAX: Split to bypass the atom
				 * MIN 1, no MAX: Append a Split (back to the start)
				 * other: Insert a Zero, append a Count
				 */
				if (instr.repetition.min == 0 && instr.repetition.max == 0)
				{
					char*	cp;
					insert_split(last_atom_start-nfa);
					*ep++ = (char)RxOp::RxoJump;
					cp = ep;	// Remember where we put the jump offset

					/*
					 * The offset in the split has the same magnitude as the negative one in the jump,
					 * but zigzag coding means the negative integer is one more than the positive.
					 * It might therefore take one more byte (at the boundary of UTF8 coding).
					 */
					StrValIndex	fwd_bytes = offset_max_bytes, rev_bytes = offset_max_bytes, last_rev;
					int		delta;		// offset value for the Split
					int		rev_delta;	// offset value for the Jump
					do {
						// last_atom_start+1 is the location of the Split offset
						// ep+rev_bytes is our guess at its target
						delta = (ep+rev_bytes) - (last_atom_start+1);	// The forward split offset
						fwd_bytes = UTF8Len(zigzag(delta));
						rev_delta = last_atom_start - (ep-(offset_max_bytes-fwd_bytes));
						last_rev = rev_bytes;
						rev_bytes = UTF8Len(zigzag(rev_delta));
					} while (rev_bytes < last_rev);

					patch_offset(last_atom_start+1-nfa, ep+rev_bytes-nfa);
					emit_offset(ep, rev_delta);
				}
				else if (instr.repetition.min == 0 && instr.repetition.max == 1)
				{
					insert_split(last_atom_start-nfa);
					patch_offset(last_atom_start+1-nfa, ep-nfa);
				}
				else if (instr.repetition.min == 1 && instr.repetition.max == 0)
				{
					*ep++ = (char)RxOp::RxoSplit;
					emit_offset(ep, last_atom_start - ep);
				}
				else
				{
					// Insert an RxoZero
					memmove(last_atom_start+1, last_atom_start, ep-last_atom_start);
					ep++;
					*last_atom_start = (char)RxOp::RxoZero;

					// Emit the repetition counter
					*ep++ = (char)RxOp::RxoCount;
					*ep++ = (char)instr.repetition.min + 1;	// Add 1 to avoid NUL characters
					*ep++ = (char)instr.repetition.max + 1;	// Add 1 to avoid NUL characters
					emit_offset(ep, last_atom_start+1 - ep);
				}
				break;
			}
			assert(ep < nfa+bytes_required);
			last_atom_start = this_atom_start;
			return true;
		};

	ok = scanRegex(second_pass);
	*ep++ = '\0';
	nfa_size = ep-nfa;

	if (!ok)
	{
		delete[] nfa;
		nfa = 0;
	}

	/*
	printf(
		"Required: Bytes %d, Offsets %d, max offset size=%d\n",
		bytes_required,
		offsets_required,
		offset_max_bytes
	);
	printf("NFA final size %d\n", nfa_size);

	assert(nfa_size <= bytes_required);
	*/

	return ok;
}
