/*
 * Unicode String regular expressions compiler
 */
#include	<strregex.h>
#include	<string.h>

RxCompiler::RxCompiler(StrVal _re, RxFeature features, RxFeature reject_features)
: re(_re)
, features_enabled((RxFeature)(features & ~reject_features))
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
	if ((features_rejected & feat) != 0)
	{
		// It would be nice to know the feature here, but we don't have format() yet
		error_message = "Rejected feature";
		return false;
	}
	return enabled(feat);
}

bool RxCompiler::enabled(RxFeature feat) const
{
	return (features_enabled & feat) != 0;
}

/*
 * Ok ima apologise right now for the nested switch statements and especially the gotos.
 * But I have been very careful about how those work, and it would have made the code
 * hugely more complicated and incomprehensible to do it any other way.
 * If you reckon you can improve on it, you're welcome to try.
 */
bool RxCompiler::scan_rx(const std::function<bool(const RxStatement& instr)> func)
{
	int		i = 0;		// Regex character offset
	UCS4		ch;		// A single character to match
	StrVal		param;		// A string argument
	int		close;		// String offset of the character ending a param
	int		int_error = 0;
	CharNum		scanned;
	bool		ok = true;
	int		min, max;
	bool		char_class_negated;
	StrVal		delayed;	// A sequence of ordinary characters to be matched.
	auto		flush = [&delayed, func]() {
				if (delayed.length() > 0)
				{
					bool	ok = func(RxStatement(RxOp::RxoLiteral, delayed));
					delayed = StrVal::null;
					return ok;
				}
				return true;
			};

	ok = func(RxStatement(RxOp::RxoStart));
	for (; ok && i < re.length(); i++)
	{
		switch (ch = re[i])	// Breaking from this switch indicates an error and stops the scan
		{
		case '^':
			if (!supported(BOL))
				goto simple_char;
			if (!(ok = flush())) continue;
			ok = func(RxStatement(RxOp::RxoBOL));
			continue;

		case '$':
			if (!supported(EOL))
				goto simple_char;
			if (!(ok = flush())) continue;
			ok = func(RxStatement(RxOp::RxoEOL));
			continue;

		case '.':
			if (enabled(AnyIsQuest))
				goto simple_char;	// if using ?, . is a simple char
			if (!(ok = flush())) continue;
			ok = func(RxStatement(RxOp::RxoAny));
			continue;

		case '?':
			if (!(ok = flush())) continue;
			if (enabled(AnyIsQuest))
				ok = func(RxStatement(RxOp::RxoAny));	// Shell-style any-char wildcard
			else
				ok = func(RxStatement(RxOp::RxoRepetition, 0, 1));	// Previous elem is optional
			continue;

		case '*':
			if (!supported(ZeroOrMore))
				goto simple_char;
			if (!(ok = flush())) continue;
			if (enabled(ZeroOrMoreAny))
				ok = func(RxStatement(RxOp::RxoAny));
			ok = func(RxStatement(RxOp::RxoRepetition, 0, 0));
			continue;

		case '+':
			if (!supported(OneOrMore))
				goto simple_char;
			if (!(ok = flush())) continue;
			ok = func(RxStatement(RxOp::RxoRepetition, 1, 0));
			continue;

		case '{':
			if (!supported(CountRepetition))
				goto simple_char;
			if (!(ok = flush())) continue;
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

			ok = func(RxStatement(RxOp::RxoRepetition, min, max));
			continue;

		case '\\':		// Escape sequence
			switch (ch = re[++i])
			{
			case 'b': if (!enabled(CEscapes)) goto simple_escape; ch = '\b'; goto escaped_char;
			case 'e': if (!enabled(CEscapes)) goto simple_escape; ch = '\033'; goto escaped_char;
			case 'f': if (!enabled(CEscapes)) goto simple_escape; ch = '\f'; goto escaped_char;
			case 'n': if (!enabled(CEscapes)) goto simple_escape; ch = '\n'; goto escaped_char;
			case 't': if (!enabled(CEscapes)) goto simple_escape; ch = '\t'; goto escaped_char;
			case 'r': if (!enabled(CEscapes)) goto simple_escape; ch = '\r'; goto escaped_char;

			case '0': case '1': case '2': case '3':	// Octal char constant
			case '4': case '5': case '6': case '7':
				if (!enabled(OctalChar))
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
				if (!enabled(HexChar))
					goto simple_escape;
				// REVISIT: Support \x{XX}
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
				if (!enabled(UnicodeChar))
					goto simple_escape;
				// REVISIT: Support \u{XX}
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
				if (!enabled(Shorthand))
					goto simple_escape;
				if (!(ok = flush())) continue;
				// REVISIT: Also support \w, \d, \h, etc - are these effectively character classes?
				ok = func(RxStatement(RxOp::RxoCharProperty, ' '));
				continue;

			case 'p':			// Posix character type
				if (!enabled(PropertyChars))
					goto simple_escape;
				if (!(ok = flush())) continue;
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
				ok = func(RxStatement(RxOp::RxoCharProperty, param));
				continue;

			default:
		simple_escape:
				ch = re[i];
		delay_escaped:
				switch (re[i+1])	// Can't delay if this escape is repeated or optional
				{	// Note this is conservative; these repetitions might not be enabled
				case '*': case '+': case '?': case '{':
					if (!(ok = flush())) continue;	// So flush it first
				}
				if (delayed.numBytes() >= RxMaxLiteral)
					if (!(ok = flush())) continue;	// Limit length of delayed text
				delayed += ch;
				continue;
			}
			break;

		case '[':		// Character class
			if (!supported(CharClasses))
				goto simple_char;
			if (!(ok = flush())) continue;

			/*
			 * A Character class is compiled as a string containing pairs of characters.
			 * Each pair determines an (inclusive) subrange of UCS4 characters.
			 * LATER: Character Property groups use non-Unicode UCS4 characters to denote the group.
			 * REVISIT: methods for allocating property group codes are under consideration
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
				// REVISIT: Add support for Posix groups or Unicode Properties
				if (ch == '\\')		// Any single Unicode char can be escaped. REVISIT for other escapes
				{
					if ((ch = re[++i]) == '\0')
						goto bad_class; // RE ends with the backslash
					// REVISIT: Support \escapes in character classes? Which ones?
				}

				param += ch;	// Start of range pair
				if (re[i+1] == '-' && re[i+2] != ']')	// Allow a single - before the closing ]
				{		// Character range
					if ((ch = re[i += 2]) == '\0')
						goto bad_class;
					if (ch == '\\')		// Any single Unicode char can be escaped. REVISIT for other escapes
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

			ok = func(RxStatement(char_class_negated ? RxOp::RxoNegCharClass : RxOp::RxoCharClass, param));
			continue;

	bad_class:	error_message = "Bad character class";
			break;

		case '|':
			if (!supported(Alternates))
				goto simple_char;
			if (!(ok = flush())) continue;
			ok = func(RxStatement(RxOp::RxoAlternate));
			continue;

		case '(':
			if (!supported(Group))
				goto simple_char;
			if (!(ok = flush())) continue;
			if (!enabled((RxFeature)(Capture|NonCapture|NegLookahead|Subroutine)) // not doing fancy groups
			 || re[i+1] != '?')					// or this one is plain
			{		// Anonymous group
				ok = func(RxStatement(RxOp::RxoNonCapturingGroup));
				continue;
			}

			i += 2;	// Skip the '(?'
			if (supported(Capture) && re[i] == '<')		// Capture
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
				ok = func(RxStatement(RxOp::RxoNamedCapture, param));
				continue;
			}
			else if (supported(NonCapture) && re[i] == ':')	// Noncapturing group
			{
				ok = func(RxStatement(RxOp::RxoNonCapturingGroup));
				continue;
			}
			else if (supported(NegLookahead) && re[i] == '!') // Negative Lookahead
			{
				ok = func(RxStatement(RxOp::RxoNegLookahead));
				continue;
			}
			else if (re[i] == '&')				// Subroutine
			{
				param = re.substr(++i);
				close = param.find(')');
				if (close <= 0)
					goto bad_capture;
				param = param.substr(0, close); // Truncate name to before >
				ok = func(RxStatement(RxOp::RxoSubroutineCall, param));
				i += close;	// Now pointing at the ')'
				continue;
			}
			else
			{
				error_message = "Illegal group type";
				break;
			}
			continue;

		case ')':
			if (!enabled(Group))
				goto simple_char;
			if (!(ok = flush())) continue;
			ok = func(RxStatement(RxOp::RxoEndGroup));
			continue;

		case ' ': case '\t': case '\n': case '\r':
			if (!enabled(ExtendedRE))
				goto simple_char;
			if (re[i+1] == '#')	// Comment to EOL
				while (++i < re.length() && re[i] != '\n')
					;
			continue;
			
		default:
		simple_char:
			switch (re[i+1])	// Can't delay if this escape is repeated or optional
			{	// Note this is conservative; these repetitions might not be enabled
			case '*': case '+': case '?': case '{':
				if (!(ok = flush())) continue;	// So flush it first
			}

			if (delayed.numBytes() >= RxMaxLiteral)
				if (!(ok = flush())) continue;	// Limit length of delayed text

			delayed += re.substr(i, 1);
			continue;
		}

		break;
	}
	if (ok)
		ok = flush();
	if (ok && !error_message && i >= re.length())	// If we didn't complete, there was an error
	{
		ok = func(RxStatement(RxOp::RxoAccept));
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
 * - The RxoStart node includes the list of group names. Subroutine calls reference a group by array index
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
 * This problem doesn't occur with subroutine calls since those go via the names table.
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
		uint8_t		group_num;
		CharBytes	start;		// Index in NFA to group-start opcode (2nd pass) or boolean (1st pass)
		CharBytes	last_split;	// Index in NFA to the Split offset for the previous alternate
		CharBytes	last_jump;	// Index in NFA to the Jump at the end of the most recent alternate. Signals an alternate chain.
	} stack[RxMaxNesting];
	int		nesting = 0;		// Stack pointer
	auto		tos = [&]() -> Stack& { return stack[nesting-1]; };	// Top of stack is stack[nesting-1]
	auto		group_op = [&]() -> RxOp { return tos().op; };		// What kind of group is at the top?
	int		max_nesting;		// How deeply are groups nested? (we actually need to know the count of nested repetitions)
	int		station_count = 0;	// How many parallel threads can a nesting-first search have?
	std::vector<StrVal>	names;		// Sequence of group names in order of occurrence
	std::vector<StrVal>::iterator	iter;	// An iterator for the group names
	UTF8*		ep;			// NFA output pointer, not used until 2nd pass

	/*
	 * In first pass, we must count how many offsets and how many other bytes are needed to store the NFA
	 */
	CharBytes	bytes_required = 0;	// Count how many bytes we will need
	CharBytes	offsets_required = 0;	// Count how many UTF8-encoded offsets we will need (these may vary in size)
	// Adjust for the size of a string to be emitted
	auto	add_string_size =
		[&](StrVal str)
		{
			CharBytes	utf8_count;	// Used for sizing string storage
			(void)str.asUTF8(utf8_count);
			bytes_required += UTF8Len(utf8_count);	// Length encoded as UTF-8
			bytes_required += utf8_count;	// UTF8 bytes
		};
	auto	add_string_storage =			// Just the string bytes, excluding the length
		[&](StrVal str)
		{
			CharBytes	utf8_count;	// Used for sizing string storage
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
			stack[nesting].last_split = ep-nfa;
			stack[nesting].last_jump = 0;
			nesting++;
			if (max_nesting < nesting)
				max_nesting = nesting;
			return true;
		};

	bool		repeatable = false;	// Cannot start with a repetition
	nfa = ep = 0;
	auto	first_pass =
		[&](const RxStatement& instr) -> bool
		{
			bool		is_atom = false;
			bytes_required++;			// One byte to store this RxOp

			switch (instr.op)
			{
			case RxOp::RxoFirstAlternate:
			case RxOp::RxoNull:
			case RxOp::RxoChar:
			case RxOp::RxoJump:
			case RxOp::RxoSplit:
			case RxOp::RxoCaptureStart:
			case RxOp::RxoCaptureEnd:
				break;				// Never sent

			case RxOp::RxoStart:			// Place to start the DFA
				offsets_required++;		// station_count
				bytes_required += 1;		// max_nesting
				offsets_required += 3;		// start_station, search_station
				bytes_required += 3;		// max_nesting, max_capture, name_count, size of the names array. Names will get added later
				station_count = 0;
				push(instr.op, 0);
				break;

			case RxOp::RxoAccept:			// Termination condition
				// Add room for the RxoAny RxoSplit for search_station
				bytes_required += 3;
				offsets_required += 2;

				// Null at end
				bytes_required++;
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

			case RxOp::RxoSubroutineCall:		// Subroutine call to a named group
				// The named subroutine might not yet have been declared, so we can't check it
				station_count++;
				is_atom = true;
				bytes_required += 1;		// We will store the index in the names array
				break;

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
				bytes_required++;		// RxoZero
				if (instr.repetition.min > 0)
				{		// We have a mandatory repetition first
					bytes_required++;	// RxoJump to the repeated thing
					offsets_required++;
				}
				bytes_required++;		// RxoCount, offset back to repeated thing
				offsets_required++;
				break;
			}
			repeatable = is_atom;
			return true;
                };

	bool		ok;			// Is everything still ok to continue?
	ok = scan_rx(first_pass);
	if (ok && nesting > 0)
	{
		error_message = "Not all groups were closed";
		ok = false;
	}
	if (!ok)
		return false;

	// What is a maximum sufficient size (in bytes) of an offset, if all offsets were the maximum 4 bytes?
	// Note that zig-zag encoding means an offset may be twice the maximum size of the NFA.
	CharBytes	offset_max_bytes = UTF8Len(2*(bytes_required+offsets_required*4));	// Upper limit on offset size
	offset_max_bytes = UTF8Len(2*(bytes_required+offsets_required*offset_max_bytes));	/// Based on actual offset size

	// The maximum size of the nfa is thus (bytes_required+offsets_required*offset_max_bytes)
	// Now we know this, we know the maximum size of an actual offset and can get close to the actual NFA size.
	// This might be a tad high as some offsets might shrink, but not by much.
	bytes_required += offsets_required * UTF8Len(bytes_required+offsets_required*offset_max_bytes);
printf("Allocating %d bytes for the NFA\n", bytes_required);

	nfa = new UTF8[bytes_required+1];
	ep = nfa;

	auto	zigzag =
		[](int i) -> CharBytes
		{
			return (abs(i)<<1) | (i < 0 ? 1 : 0);
		};

	auto	zagzig =
		[](int i) -> int
		{
			return (i>>1) * ((i&01) ? -1 : 1);
		};

	// Emit an offset, advancing np, and returning the number of UTF8 bytes emitted
	auto	emit_offset =
		[&](char*& np, int offset) -> CharBytes
		{
			char*	cp = np;

			UTF8Put(np, zigzag(offset));
			return (CharBytes)(np-cp);
		};

	// Emit an offset, advancing np, and returning the number of UTF8 bytes emitted
	auto	get_offset =
		[&](const char*& np) -> CharBytes
		{
			return (CharBytes)zagzig(UTF8Get(np));
		};

	// Emit a zigzag integer, but always advance np by offset_max_bytes
	auto	emit_padded_offset =
		[&](char*& np, int offset = 0)
		{
			char*	cp = np;
			UTF8Put(cp, zigzag(offset));
			np += offset_max_bytes;
		};

	// Emit a StrVal, advancing ep
	auto	emit_string =
		[&](StrVal str)
		{
			const UTF8*	sp;		// The UTF8 bytes of a string we'll copy into the NFA
			CharBytes	utf8_count;	// Used for sizing string storage
			sp = str.asUTF8(utf8_count);	// Get the bytes and byte count
			UTF8Put(ep, utf8_count);	// Encode the byte count as UTF-8
			memcpy(ep, sp, utf8_count);	// Output the bytes to the NFA
			ep += utf8_count;		// And advance past them
		};

	auto	patch_value_at =
		[&](CharBytes patch_location, int value) -> int
		{
			CharBytes	byte_count = UTF8Len(zigzag(value));
			CharBytes	shrinkage = offset_max_bytes - byte_count;
printf("Patching %d with value %d (offs to %d), with %d shrinkage\n", patch_location, value, (patch_location+value), shrinkage);

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

	auto	patch_offset_at =
		[&](CharBytes patch_location, CharBytes offset_to) -> int
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
			CharBytes	byte_count = UTF8Len(zigzag(offset));
			CharBytes	shrinkage = offset_max_bytes - byte_count;
			if (byte_count < offset_max_bytes	// We'll shuffle
			 && patch_location < offset_to)		// And the patch location is prior to the target
				offset -= shrinkage;		// The offset is now smaller

			return patch_value_at(patch_location, offset);
		};

	auto	insert_split =
		[&](CharBytes location)
		{		// The new split is 1 byte & 1 offset. Make room for it
			char*	new_split = nfa+location;
			memmove(new_split+1+offset_max_bytes, new_split, ep-new_split);
			ep += 1+offset_max_bytes;
			*new_split++ = (char)RxOp::RxoSplit;
			tos().last_split = new_split-nfa;
			emit_padded_offset(new_split);			// and make room for it
		};

	auto	fixup_alternates =
		[&]()
		{
			if (tos().last_jump != 0)
			{		// Fixup Alternates
				// If patching the Split shrinks the offset, pull last_jump() back by the shrinkage
				tos().last_jump -=
					patch_offset_at(tos().last_split, tos().last_jump+offset_max_bytes);

				CharBytes	jump = tos().last_jump;	// Offset inside the last Jump
				for (;;)
				{
					const char*	cp = nfa+jump;
					int		prev = get_offset(cp);	// Get offset to previous jump

					// REVISIT: Handle shrinkage here!

					patch_offset_at(jump, (ep-nfa));
					if (!prev)
						break;
					jump += prev;
				}
			}
		};

	UTF8*		last_atom_start = ep;	// Pointer to the thing to which a repetition applies
	int		next_group = 0;	// Number of the next named group

	auto	second_pass =
		[&](const RxStatement& instr) -> bool
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
			case RxOp::RxoFirstAlternate:
			case RxOp::RxoNull:
				break;				// Never sent

			case RxOp::RxoStart:			// Place to start the DFA
				nesting = 0;			// Clear the stack
				push(instr.op, next_group++);	// and open this "group"
				emit_padded_offset(ep);		// search_station; will be patched on RxoAccept
				emit_padded_offset(ep);		// start_station
				emit_offset(ep, station_count);
				*ep++ = max_nesting;		// REVISIT: Not all nesting involves counters
				*ep++ = names.size() + 1;	// max_capture; 0 is the overall match, first group starts at 1
				*ep++ = names.size() + 1;	// num_names
				for (iter = names.begin(); iter != names.end(); iter++)
					emit_string(*iter);
				tos().last_split = ep-nfa;	// Record where the first alternate must start
				patch_offset_at(1+offset_max_bytes, ep-nfa);	// start_station
				break;

			case RxOp::RxoAccept:			// Termination condition
			{
				ep--;
				fixup_alternates();
				*ep++ = (char)RxOp::RxoAccept;

				// Fixup RxoStart block
				const char*	cp = nfa+1+offset_max_bytes;	// Location of start_station
				cp -= patch_offset_at(1, ep-nfa);	// Patch search_station (not actually an offset!)

				CharBytes	start_station = (cp-nfa)+get_offset(cp);

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
				ep--;
				push(instr.op, 0);
				break;

			case RxOp::RxoNamedCapture:		// (?<name>...)
				ep[-1] = (char)RxOp::RxoCaptureStart;
				push(instr.op, next_group++);
				*ep++ = next_group-1;
				tos().last_split = ep-nfa;	// Record where the first alternate must start
				break;

			case RxOp::RxoNegLookahead:		// (?!...)
				push(instr.op, 0);
				emit_padded_offset(ep);		// Pointer to next station if the lookahead allows us to proceed
				tos().last_split = ep-nfa;	// Record where the first alternate must start
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
					patch_offset_at(tos().start+1, ep-nfa);
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
				nesting--;
				break;

			case RxOp::RxoSubroutineCall:		// Subroutine call to a named group
			{
				int i = 1;
				for (iter = names.begin(); iter != names.end(); iter++, i++)
				{
					if (*iter == instr.str)
						break;
				}
				if (i > names.size())
				{
					error_message = "Function call to undeclared group";
					return false;
				}
				*ep++ = i;
				break;
			}

			case RxOp::RxoAlternate:		// |
				ep--;	// Don't output RxoAlternate

				if (tos().last_jump == 0)
				{		// End of first alternate. tos().last_split is the start of the group contents
					// Insert a split at last_split
					insert_split(tos().last_split);		// Sets new last_split

					// Append a new jump with a zero value to terminate a new list
					*ep++ = (char)RxOp::RxoJump;
					tos().last_jump = ep-nfa;		// Prepare for the next jump
					emit_padded_offset(ep);
				}
				else
				{		// End of subsequent alternate. tos().last_split is where we'll find the offset in the previous split
					CharBytes	last_split = tos().last_split;
					CharBytes	new_split;

					// We need to insert a new split after the last jump whose offset is at last_jump
					new_split = tos().last_jump+offset_max_bytes;		// The newsplit goes after the last Jump+offset
					insert_split(new_split);		// Sets new last_split

					// Patch the last_split to point to the start of this new split
					patch_offset_at(last_split, new_split);

					// Append a new jump that points back to the previous one
					*ep++ = (char)RxOp::RxoJump;
					emit_padded_offset(ep, tos().last_jump-(ep-nfa));
					tos().last_jump = (ep-nfa)-offset_max_bytes;
				}
				break;

//========================================================================================================================
			case RxOp::RxoRepetition:		// {n, m}
				ep--;
#if 0
				ep[-1] = (char)RxOp::RxoEndGroup;
				// Shuffle NFA down by 3 bytes to insert this opcode and the repetition limits before last_atom_start
				memmove(last_atom_start+3, last_atom_start, ep-last_atom_start);
				ep += 3;
				*last_atom_start++ = (char)RxOp::RxoRepetition;
				*last_atom_start++ = (char)instr.repetition.min + 1;	// Add 1 to avoid NUL characters
				*last_atom_start++ = (char)instr.repetition.max + 1;	// Add 1 to avoid NUL characters
#endif
				break;
			}
			assert(ep < nfa+bytes_required);
			last_atom_start = this_atom_start;
			return true;
		};

	ok = scan_rx(second_pass);
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
