/*
 * Unicode String regular expressions
 */
#include	<strregex.h>
#include	<string.h>
#include	<vector>

using std::vector;

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
bool RxCompiler::scan_rx(const std::function<bool(const RxInstruction& instr)> func)
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
					bool	ok = func(RxInstruction(RxOp::RxoLiteral, delayed));
					delayed = StrVal::null;
					return ok;
				}
				return true;
			};

	ok = func(RxInstruction(RxOp::RxoStart));
	for (; ok && i < re.length(); i++)
	{
		switch (ch = re[i])	// Breaking from this switch indicates an error and stops the scan
		{
		case '^':
			if (!supported(BOL))
				goto simple_char;
			if (!(ok = flush())) continue;
			ok = func(RxInstruction(RxOp::RxoBOL));
			continue;

		case '$':
			if (!supported(EOL))
				goto simple_char;
			if (!(ok = flush())) continue;
			ok = func(RxInstruction(RxOp::RxoEOL));
			continue;

		case '.':
			if (enabled(AnyIsQuest))
				goto simple_char;	// if using ?, . is a simple char
			if (!(ok = flush())) continue;
			ok = func(RxInstruction(RxOp::RxoAny));
			continue;

		case '?':
			if (!(ok = flush())) continue;
			if (enabled(AnyIsQuest))
				ok = func(RxInstruction(RxOp::RxoAny));	// Shell-style any-char wildcard
			else
				ok = func(RxInstruction(RxOp::RxoRepetition, 0, 1));	// Previous elem is optional
			continue;

		case '*':
			if (!supported(ZeroOrMore))
				goto simple_char;
			if (!(ok = flush())) continue;
			if (enabled(ZeroOrMoreAny))
				ok = func(RxInstruction(RxOp::RxoAny));
			ok = func(RxInstruction(RxOp::RxoRepetition, 0, 0));
			continue;

		case '+':
			if (!supported(OneOrMore))
				goto simple_char;
			if (!(ok = flush())) continue;
			ok = func(RxInstruction(RxOp::RxoRepetition, 1, 0));
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

			ok = func(RxInstruction(RxOp::RxoRepetition, min, max));
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
				ok = func(RxInstruction(RxOp::RxoCharProperty, ' '));
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
				ok = func(RxInstruction(RxOp::RxoCharProperty, param));
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

			ok = func(RxInstruction(char_class_negated ? RxOp::RxoNegCharClass : RxOp::RxoCharClass, param));
			continue;

	bad_class:	error_message = "Bad character class";
			break;

		case '|':
			if (!supported(Alternates))
				goto simple_char;
			if (!(ok = flush())) continue;
			ok = func(RxInstruction(RxOp::RxoAlternate));
			continue;

		case '(':
			if (!supported(Group))
				goto simple_char;
			if (!(ok = flush())) continue;
			if (!enabled((RxFeature)(Capture|NonCapture|NegLookahead|Subroutine)) // not doing fancy groups
			 || re[i+1] != '?')					// or this one is plain
			{		// Anonymous group
				ok = func(RxInstruction(RxOp::RxoNonCapturingGroup));
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
				ok = func(RxInstruction(RxOp::RxoNamedCapture, param));
				continue;
			}
			else if (supported(NonCapture) && re[i] == ':')	// Noncapturing group
			{
				ok = func(RxInstruction(RxOp::RxoNonCapturingGroup));
				continue;
			}
			else if (supported(NegLookahead) && re[i] == '!') // Negative Lookahead
			{
				ok = func(RxInstruction(RxOp::RxoNegLookahead));
				continue;
			}
			else if (re[i] == '&')				// Subroutine
			{
				param = re.substr(++i);
				close = param.find(')');
				if (close <= 0)
					goto bad_capture;
				param = param.substr(0, close); // Truncate name to before >
				ok = func(RxInstruction(RxOp::RxoSubroutine, param));
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
			ok = func(RxInstruction(RxOp::RxoEndGroup));
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

			delayed += re.substr(i, 1);
			continue;
		}

		break;
	}
	if (ok)
		ok = flush();
	if (ok && !error_message && i >= re.length())	// If we didn't complete, there was an error
	{
		ok = func(RxInstruction(RxOp::RxoEnd));
		return ok;
	}
	return false;
}

/*
 * Compilation strategy is as follows:
 * - Scan through the RE once, accumulating the maximum required size for the NFA and error-checking
 * - In the first pass, we build an array of all named groups
 * - Allocate the data and scan through again, building the compiled NFA
 * - The start node includes the list of groups. Subroutine calls reference a group by array index
 *
 * The finished NFA is a series of packed characters and numbers. Both are packed using UTF-8 coding.
 * The opcodes are all ASCII (single UTF-8 byte), with one bit indicating that repetition is involved.
 *
 * As the NFA is built, there are places where numbers that aren't yet known must be later patched in.
 * Because the UTF-8 encoding of these numbers is variable in size, the maximum required space is
 * reserved, so patching involves shuffling the rest of the NFA down to meet it.
 * - The repeat opcode and min&max number (unsigned char) of repetitions allowed, 3 bytes total.
 * - Byte offset from an alternate or the start of a group, to the next alternate or end of the group.
 *
 * This problem doesn't occur with subroutine calls since those go via the names table.
 *
 * The first pass involves two counts: the number of offset numbers to encode, and the number of other
 * bytes.  If this indicates that all offsets will fit in 1 byte (or 2 bytes, etc) the maximum required
 * size is calculated accordingly.
 *
 * When repetition is seen, the entire NFA from the start of the repeated item to the end gets shuffled
 * down three bytes to make room.
 */
bool
RxCompiler::compile(char*& nfa)
{
	CharBytes	bytes_required = 0;	// Count how many bytes we will need
	CharBytes	offsets_required = 0;	// Count how many UTF8-encoded offsets we will need
	struct {
		uint8_t		group_num;
		CharBytes	start;		// Offset to group-start opcode
		CharBytes	previous;	// Index in nfa of the offset in the group-start or previous alternative
		CharBytes	next;		// Index in nfa of the opcode following the group-start
	} stack[16];	// Maximum nesting depth for groups
	int		depth;			// Stack pointer
	CharBytes	byte_count;		// Used for sizing string storage
	RxOp		last = RxOp::RxoStart;
	bool		repeatable = false;	// Cannot start with a repetition
	bool		ok;			// Is everything still ok to continue?
	vector<StrVal>	names;
	vector<StrVal>::iterator	iter;

	auto	first_pass =
		[&](const RxInstruction& instr) -> bool
                {
			bool		is_atom = false;
			bytes_required++;			// One byte to store the RxOp

			switch (instr.op)
			{
			case RxOp::RxoNull: break;
			case RxOp::RxoStart:			// Place to start the DFA
				// Memory allocation instructions can also go in the start block, and in each target of a subroutine call.
				bytes_required += 1;		// Store the number of names in the names array
				offsets_required++;		// Need an offset to the second alternate, like any group
				stack[0].group_num = 0;
				stack[0].start = 1;
				depth = 1;
				break;

			case RxOp::RxoEnd:			// Termination condition
				bytes_required++;		// Null at end
				depth--;
				break;

			case RxOp::RxoEndGroup:			// End of a group
				if (--depth < 0)
				{
					error_message = "Too many closing parentheses";
					return false;
				}
				is_atom = true;
				break;

			case RxOp::RxoAlternate:		// |
				offsets_required++;
				break;

			case RxOp::RxoNonCapturingGroup:	// (...)
			case RxOp::RxoNegLookahead:		// (?!...)
				offsets_required++;
				if (depth >= sizeof(stack)/sizeof(stack[0]))
				{
		too_deep:		error_message = "Nesting too deep";
					return false;
				}
				stack[depth].group_num = 0;
				stack[depth].start = 1;		// Before any Alternate
				depth++;
				break;

			case RxOp::RxoNamedCapture:		// (?<name>...)
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

				offsets_required++;
				if (depth >= sizeof(stack)/sizeof(stack[0]))
					goto too_deep;
				stack[depth].group_num = names.size();	// First entry is 1. 0 means un-named
				stack[depth].start = 1;		// Before any Alternate
				depth++;
				goto str_param;

			case RxOp::RxoLiteral:			// A specific string
			case RxOp::RxoCharProperty:		// A char with a named Unicode property
			case RxOp::RxoCharClass:		// Character class; instr contains a string of character pairs
			case RxOp::RxoNegCharClass:		// Negated Character class
				is_atom = true;
		str_param:	(void)instr.str.asUTF8(byte_count);
				bytes_required += UTF8Len(byte_count);	// Length encoded as UTF-8
				bytes_required += byte_count;	// UTF8 bytes
				break;

			case RxOp::RxoSubroutine:		// Subroutine call to a named group
				// The named subroutine might not yet have been declared, so we can't check it
				is_atom = true;
				bytes_required += 1;		// We will store the index in the names array
				break;

			case RxOp::RxoBOL:			// Beginning of Line
				break;				// Nothing special to see here, move along
			case RxOp::RxoEOL:			// End of Line
				break;				// Nothing special to see here, move along
			case RxOp::RxoAny:			// Any single char
				break;				// Nothing special to see here, move along

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
				bytes_required += 2;
				break;
			}
			last = instr.op;
			repeatable = is_atom;
			return true;
                };

	ok = scan_rx(first_pass);
	if (ok && depth > 0)
	{
		error_message = "Not all groups were closed";
		ok = false;
	}
	if (!ok)
		return false;

	CharBytes	offset_max_bytes = UTF8Len(bytes_required+offsets_required*4);

	// Now we know the maximum size of the FA, we know the maximum size of an offset:
	bytes_required += offsets_required * UTF8Len(bytes_required+offsets_required*4);

	nfa = new UTF8[bytes_required+1];
	UTF8*		ep = nfa;
	UTF8*		last_atom_start = ep;
	UTF8*		cp;
	const UTF8*	sp;
	CharBytes	offset;
	int		i;
	int		next_group = 0;

	auto	second_pass =
		[&](const RxInstruction& instr) -> bool
                {
			UTF8*	this_atom_start = ep;		// Pointer to the thing a following repetition will repeat
			*ep++ = (UTF8)instr.op;
			switch (instr.op)
			{
			case RxOp::RxoNull:
				break;

			case RxOp::RxoStart:			// Place to start the DFA
				depth = 0;
				stack[depth].group_num = next_group++;
				stack[depth].start = ep-1-nfa;
				stack[depth].previous = ep-nfa;	// I.e. 1 :)
				depth++;
				UTF8PutLong0(ep, offset_max_bytes);	// Reserve space for a patched offset
				*ep++ = names.size() + 1;		// Add 1 to avoid NUL characters
				for (iter = names.begin(); iter != names.end(); iter++)
				{
					sp = iter->asUTF8(byte_count);
					UTF8Put(ep, byte_count);
					memcpy(ep, sp, byte_count);
					ep += byte_count;
				}
				stack[depth-1].next = ep-nfa;
				break;

			case RxOp::RxoEnd:			// Termination condition
			case RxOp::RxoEndGroup:			// End of a group
				this_atom_start = nfa+stack[depth-1].start;	// This is what a following repetition repeats
				// Fall through
			case RxOp::RxoAlternate:		// |
				/*
				 * The previous alternate/group-start is at stack[depth-1].previous.
				 * We patch an offset there to point to ep-1 (the RxOp byte we just output to *ep).
				 * If the offset fits in fewer than offset_max_bytes UTF-8 bytes, we must shuffle down.
				 * This reduces the offset, which can cause it to cross a UTF-8 coding boundary,
				 * taking one fewer bytes, a process that can only happen once.
				 */
				offset = (ep-nfa) - stack[depth-1].previous;
				byte_count = UTF8Len(offset);
				if (byte_count < offset_max_bytes)	// We'll shuffle
				{
					offset -= (offset_max_bytes-byte_count);
					byte_count = UTF8Len(offset);
				}

				cp = nfa+stack[depth-1].previous;	// The patch location
				if (byte_count < offset_max_bytes)
				{		// Shuffle down, this offset is smaller than the offset_max_bytes
					// We reserved offset_max_bytes, but only need byte_count.
					memmove(cp+byte_count, cp+offset_max_bytes, ep-(cp+offset_max_bytes));
					ep -= offset_max_bytes-byte_count;	// the nfa contains fewer bytes now
				}
				UTF8Put(cp, offset);		// Patch done

				if (instr.op == RxOp::RxoAlternate)
				{
					stack[depth-1].previous = ep-nfa;
					UTF8PutLong0(ep, offset_max_bytes);	// Reserve space for a patched offset
				}
				else
					depth--;		// Pop the stack, this group is done

				break;

			case RxOp::RxoNonCapturingGroup:	// (...)
			case RxOp::RxoNegLookahead:		// (?!...)
				stack[depth].group_num = 0;
				stack[depth].start = ep-1-nfa;
				stack[depth].previous = ep-nfa;
				depth++;
				UTF8PutLong0(ep, offset_max_bytes);	// Reserve space for a patched offset
				stack[depth-1].next = ep-nfa;
				break;

			case RxOp::RxoNamedCapture:		// (?<name>...)
				stack[depth].group_num = next_group;	// Index in names table + 1
				stack[depth].start = ep-1-nfa;
				stack[depth].previous = ep-nfa;
				depth++;
				UTF8PutLong0(ep, offset_max_bytes);	// Reserve space for a patched offset
				*ep++ = next_group++;
				stack[depth-1].next = ep-nfa;
				break;

			case RxOp::RxoLiteral:			// A specific string
			case RxOp::RxoCharProperty:		// A char with a named Unicode property
			case RxOp::RxoCharClass:		// Character class; instr contains a string of character pairs
			case RxOp::RxoNegCharClass:		// Negated Character class
				sp = instr.str.asUTF8(byte_count);	// Get the bytes and byte count
				UTF8Put(ep, byte_count);	// Encode the byte counnt as UTF-8
				memcpy(ep, sp, byte_count);	// Output the bytes
				ep += byte_count;		// And advance past them
				break;

			case RxOp::RxoSubroutine:		// Subroutine call to a named group
				i = 1;
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

			case RxOp::RxoBOL:			// Beginning of Line
				break;				// Nothing special to see here, move along
			case RxOp::RxoEOL:			// End of Line
				break;				// Nothing special to see here, move along
			case RxOp::RxoAny:			// Any single char
				break;				// Nothing special to see here, move along

			case RxOp::RxoRepetition:		// {n, m}
				ep--;				// We aren't going to record this here
				// Shuffle NFA down by 3 bytes to insert this opcode and the repetition limits before last_atom_start
				memmove(last_atom_start+3, last_atom_start, ep-last_atom_start);
				ep += 3;
				*last_atom_start++ = (UTF8)RxOp::RxoRepetition;
				*last_atom_start++ = (UTF8)instr.repetition.min + 1;	// Add 1 to avoid NUL characters
				*last_atom_start++ = (UTF8)instr.repetition.max + 1;	// Add 1 to avoid NUL characters
				break;
			}
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

void
RxCompiler::dump(const char* nfa)		// Dump binary code to stdout
{
	if (!nfa)
	{
		printf("No NFA to dump\n");
		return;
	}

	const	UTF8*	np;
	for (np = nfa; np < nfa+nfa_size; np++)
		printf("%02X ", *np&0xFF);
	printf("\n");

	int		depth = 0;
	int		num_names;
	vector<StrVal>	names;			// Build the names for printing
	const UTF8*	name;
	CharBytes	byte_count;		// Used for sizing string storage
	int		offset_this;
	int		offset_next;
	int		group_num;
	uint8_t		op_num;
	const char*	op_name;
	int		min, max;
	for (np = nfa; np < nfa+nfa_size;)
	{
		offset_this = np-nfa;
		printf("%d\t", offset_this);
		for (int i = 1; i < depth; i++)
			printf("\t");
		op_num = *np++ & 0xFF;
		switch ((RxOp)op_num)
		{
		case (RxOp)'\0':
			if (np == nfa+nfa_size)
			{
				printf("Null termination\n");
				break;			// Valid ending
			}
			// Fall through
		default:
			printf("Illegal NFA opcode %02X\n", op_num);
			return;

		case RxOp::RxoStart:			// Place to start the DFA
			offset_next = UTF8Get(np);
			num_names = (*np++ & 0xFF) - 1;
			printf("NFA Start(%02X), next->%d", op_num, offset_next);
			if (num_names > 0)
			{
				printf(", names:");
				for (int i = 0; i < num_names; i++)
				{
					byte_count = UTF8Get(np);
					printf(" %.*s", byte_count, np);
					names.push_back(StrVal(np, byte_count));
					np += byte_count;
				}
			}
			printf("\n");
			depth++;
			break;

		case RxOp::RxoEnd:			// Termination condition
			printf("End(%02X)\n", op_num);
			depth--;
			break;

		case RxOp::RxoEndGroup:			// End of a group
			printf("RxoEndGroup(%02X)\n", op_num);
			depth--;
			break;

		case RxOp::RxoAlternate:		// |
			offset_next = UTF8Get(np);
			printf("RxoAlternate(%02X) next=(+%d)->%d\n", op_num, offset_next, offset_this+offset_next);
			break;

		case RxOp::RxoNonCapturingGroup:	// (...)
			offset_next = UTF8Get(np);
			printf("\tRxoNonCapturingGroup(%02X) offset=(+%d)->%d\n", op_num, offset_next, offset_this+offset_next);
			depth++;
			break;

		case RxOp::RxoNegLookahead:		// (?!...)
			offset_next = UTF8Get(np);
			printf("\tRxoNegLookahead(%02X) offset=(%d)->%d\n", op_num, offset_next, offset_this+offset_next);
			depth++;
			break;

		case RxOp::RxoNamedCapture:		// (?<name>...)
			offset_next = UTF8Get(np);
			group_num = (*np++ & 0xFF) - 1;
			name = names[group_num].asUTF8(byte_count);
			printf("\tRxoNamedCapture(%02X) '%.*s'(%d), offset=(%d)->%d\n", op_num, byte_count, name, group_num, offset_next, offset_this+offset_next);
			depth++;
			break;

		case RxOp::RxoLiteral:			// A specific string
			op_name = "RxoLiteral";
			goto string;
		case RxOp::RxoCharProperty:		// A char with a named Unicode property
			op_name = "RxoCharProperty";
			goto string;
		case RxOp::RxoCharClass:		// Character class; instr contains a string of character pairs
			op_name = "RxoCharClass";
			goto string;
		case RxOp::RxoNegCharClass:		// Negated Character class
			op_name = "RxoNegCharClass";
		string:
			byte_count = UTF8Get(np);
			printf("\t%s(%02X), '%.*s'\n", op_name, op_num, byte_count, np);
			np += byte_count;
			break;

		case RxOp::RxoSubroutine:		// Subroutine call to a named group
			group_num = (*np++ & 0xFF) - 1;
			name = names[group_num].asUTF8(byte_count);
			printf("\tRxoSubroutine(%02X) call to '%.*s'(%d)\n", op_num, byte_count, name, group_num);
			break;

		case RxOp::RxoBOL:			// Beginning of Line
			printf("\tRxoBOL(%02X)\n", op_num);
			break;

		case RxOp::RxoEOL:			// End of Line
			printf("\tRxoEOL(%02X)\n", op_num);
			break;

		case RxOp::RxoAny:			// Any single char
			printf("\tRxoAny(%02X)\n", op_num);
			break;

		case RxOp::RxoRepetition:		// {n, m}
			min = (*np++ & 0xFF) - 1;
			max = (*np++ & 0xFF) - 1;
			printf("\tRxoRepetition(%02X) min=%d max=%d\n", op_num, min, max);
			break;
		}
	}
}
