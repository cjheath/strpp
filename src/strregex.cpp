/*
 * Unicode String regular expressions
 */
#include	<strregex.h>
#include	<vector>

using std::vector;

RxCompiled::RxCompiled(StrVal _re, RxFeature features, RxFeature reject_features)
: re(_re)
, features_enabled((RxFeature)(features & ~reject_features))
, features_rejected(reject_features)
, error_message(0)
{
	// estimate_limits();
}

RxCompiled::~RxCompiled()
{
}

bool RxCompiled::supported(RxFeature feat)
{
	if ((features_rejected & feat) != 0)
	{
		// It would be nice to know the feature here, but we don't have format() yet
		error_message = "Rejected feature";
		return false;
	}
	return enabled(feat);
}

bool RxCompiled::enabled(RxFeature feat) const
{
	return (features_enabled & feat) != 0;
}

/*
 * Ok ima apologise right now for the nested switch statements and especially the gotos.
 * But I have been very careful about how those work, and it would have made the code
 * hugely more complicated and incomprehensible to do it any other way.
 * If you reckon you can improve on it, you're welcome to try.
 */
bool RxCompiled::scan_rx(const std::function<bool(const RxInstruction& instr)> func)
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
				if ((int_error && int_error == STRERR_NO_DIGITS) || max < min)
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
				if (ch == '\\')		// Any single Unicode char can be escaped. REVISIT for other escapes
					if ((ch = re[++i]) == '\0')
						goto bad_class; // RE ends with the backslash

				param += ch;	// Start of range pair
				if (re[i+1] == '-' && re[i+2] != ']')	// Allow a single - before the closing ]
				{		// Character range
					if ((ch = re[i += 2]) == '\0')
						goto bad_class;
					if (ch == '\\')		// Any single Unicode char can be escaped. REVISIT for other escapes
						if ((ch = re[++i]) == '\0')
							goto bad_class; // RE ends with the backslash
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

			if (delayed.length() == 0)
				delayed = re.substr(i, 1);	// Start an re substring - lower cost
			else if (delayed.isShared())		// Continue an re substring
				delayed = re.substr(i-delayed.length(), delayed.length()+1);
			else					// Not using an re substring due to previous escape etc
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
 * - Scan through the RE once, accumulating the maximum required size for the NFA
 * - In the first pass, we build an array of all named groups
 * - Allocate the data and scan through again, building the compiled NFA
 * - The start node includes the list of groups. Subroutine calls reference a group by array index
 *
 * The finished NFA is a series of packed characters and numbers. Both are packed using UTF-8 coding.
 * The opcodes are all ASCII (single UTF-8 byte), with one bit indicating that repetition is involved.
 *
 * As the NFA is built, there are places where numbers that aren't yet known must be later patched in.
 * Because the UTF-8 encoding of these numbers might be variable in size, so the maximum required space
 * is reserved, so patching involves shuffling the rest of the NFA down to meet it.
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
RxCompiled::compile()
{
	CharBytes	bytes_required = 0;
	CharBytes	offsets_required = 0;
	CharBytes	byte_count;
	struct {
		uint8_t		group_num;
		CharBytes	start;		// Offset to group-start opcode
		CharBytes	previous;	// Offset to start or previous alternative
	} stack[16];	// Maximum nesting depth for groups
	int		depth = 1;
	RxOp		last = RxOp::RxoStart;
	bool		last_was_atom = false;
	bool		ok;
	vector<StrVal>	names;
	vector<StrVal>::iterator	iter;

	auto	first_pass =
		[&](const RxInstruction& instr) -> bool
                {
			bool		is_atom = false;
			bytes_required++;			// One byte to store the RxOp

			switch (instr.op)
			{
			case RxOp::RxoStart:			// Place to start the DFA
				// Memory allocation instructions can also go in the start block, and in each target of a subroutine call.
				bytes_required += 1;		// Store the number of names in the names array
				offsets_required++;		// Need an offset to the second alternate, like any group
				stack[depth++].group_num = 0;
				break;

			case RxOp::RxoEnd:			// Termination condition
				// REVISIT: patch the previous alternate/group-start to point here
				break;

			case RxOp::RxoNonCapturingGroup:	// (...)
				if (depth >= sizeof(stack)/sizeof(stack[0]))
				{
					error_message = "Nesting too deep";
					return false;
				}
				stack[depth].group_num = 0;
				// stack[depth].start = bytes_required;	// Not needed in first pass
				depth++;
				break;

			case RxOp::RxoNamedCapture:		// (?<name>...)
				if (depth >= sizeof(stack)/sizeof(stack[0]))
				{
					error_message = "Nesting too deep";
					return false;
				}
				for (iter = names.begin(); iter != names.end(); iter++)
					if (*iter == instr.str)
					{
						error_message = "Duplicate name";
						return false;
					}
				names.push_back(instr.str);
				stack[depth].group_num = names.size();	// First entry is 1. 0 means un-named
				// stack[depth].start = bytes_required;	// Not needed in first pass
				offsets_required++;
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
				if (!last_was_atom)
				{
					error_message = "Repeating a repetition is disallowed";
					return false;
				}
				if (instr.repetition.min > 255 || instr.repetition.max > 255)
				{
					error_message = "min and max repetition are limited to 255";
					return false;
				}
				bytes_required += 2;
				break;

			case RxOp::RxoNegLookahead:		// (?!...)
				if (depth >= sizeof(stack)/sizeof(stack[0]))
				{
					error_message = "Nesting too deep";
					return false;
				}
				stack[depth].group_num = 0;
				// stack[depth].start = bytes_required;	// Not needed in first pass
				depth++;
				break;

			case RxOp::RxoAlternate:		// |
				// REVISIT: patch the previous alternate/group-start to point here
				offsets_required++;
				break;

			case RxOp::RxoEndGroup:			// End of a group
				if (--depth < 0)
				{
					error_message = "too many closing parentheses";
					return false;
				}
				// REVISIT: patch the previous alternate/group-start to point here
				offsets_required++;
				is_atom = true;
				break;
			}
			last = instr.op;
			last_was_atom = is_atom;
			return true;
                };

	auto	second_pass =
		[&](const RxInstruction& instr) -> bool
                {
			return true;
		};

	ok = scan_rx(first_pass);
	if (ok)
		printf("Required: Bytes %d, Offsets %d\n", bytes_required, offsets_required);
	return ok;
}

/*
 * Evaluate the RE, counting the number of instructions, groups, captures, and parallel states
 * that are required to compile and execute it
void
RxCompiled::estimate_limits()
{
}
 */
