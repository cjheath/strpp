/*
 * Unicode String regular expressions
 */
#include	<strregex.h>

RxCompiled::RxCompiled(StrVal _re, RxFeature reject_features, RxFeature ignore_features)
: re(_re)
, features_enabled((RxFeature)(RxFeature::AllFeatures & ~(reject_features | ignore_features)))
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

bool RxCompiled::enabled(RxFeature feat)
{
	return (features_enabled & feat) != 0;
}

bool RxCompiled::scan_rx(bool (*func)(RxOp op, StrVal param))
{
	int		i = 0;		// Regex character offset
	UCS4		ch;		// A single character to match
	StrVal		param;		// A string argument
	int		close;		// String offset of the character ending a param
	int		int_error = 0;
	CharNum		scanned;
	bool		ok = true;
	int		min, max;

	ok = func(RxOp::RxoStart, StrVal::null);
	for (; i < re.length(); i++)
	{
		switch (ch = re[i])	// Breaking from this switch indicates an error and stops the scan
		{
		case '^':
			if (!supported(BOL))
				goto simple_char;
			ok = func(RxOp::RxoBOL, StrVal::null);
			continue;

		case '$':
			if (!supported(EOL))
				goto simple_char;
			ok = func(RxOp::RxoEOL, StrVal::null);
			continue;

		case '.':
			if (enabled(AnyIsQuest))
				goto simple_char;	// if using ?, . is a simple char
			ok = func(RxOp::RxoAny, StrVal::null);
			continue;

		case '?':
			if (enabled(AnyIsQuest))
				ok = func(RxOp::RxoAny, StrVal::null);	// Shell-style any-char wildcard
			else
				ok = func(RxOp::RxoZeroOrOne, StrVal::null);	// Previous elem is optional
			continue;

		case '*':
			if (!supported(ZeroOrMore))
				goto simple_char;
			if (enabled(ZeroOrMoreAny))
				ok = func(RxOp::RxoAny, StrVal::null);
			ok = func(RxOp::RxoZeroOrMore, StrVal::null);
			continue;

		case '+':
			if (!supported(OneOrMore))
				goto simple_char;
			ok = func(RxOp::RxoOneOrMore, StrVal::null);
			continue;

		case '{':
			if (!supported(CountRepetition))
				goto simple_char;
			param = re.substr(++i);
			min = max = -1;
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
				if (int_error)
					goto bad_repetition;
			} // else min == -1
			i += close+1;	// Skip the ',' or '}'

			if (re[i-1] == ',')		// There is a second number
			{
				param = re.substr(i);
				close = param.find('}');
				if (close < 0)
					goto bad_repetition;
				max = param.substr(0, close).asInt32(&int_error, 10, &scanned);
				if (int_error)
					goto bad_repetition;
				i += close+1;
			}
			else
				max = min;		// Exact repetition count

			// REVISIT: Yield counted repetition
			printf("RxoCountedRepetition(%d, %d)\n", min, max);
			// ok = func(RxOp::RxoOneOrMore, StrVal::null);
			break;

		case '\\':		// Escape sequence
			switch (re[++i])
			{
			case 'b': if (!enabled(CEscapes)) goto simple_escape; ok = func(RxOp::RxoChar, '\b'); continue;
			case 'e': if (!enabled(CEscapes)) goto simple_escape; ok = func(RxOp::RxoChar, '\e'); continue;
			case 'f': if (!enabled(CEscapes)) goto simple_escape; ok = func(RxOp::RxoChar, '\f'); continue;
			case 'n': if (!enabled(CEscapes)) goto simple_escape; ok = func(RxOp::RxoChar, '\n'); continue;
			case 't': if (!enabled(CEscapes)) goto simple_escape; ok = func(RxOp::RxoChar, '\t'); continue;
			case 'r': if (!enabled(CEscapes)) goto simple_escape; ok = func(RxOp::RxoChar, '\r'); continue;

			case '0': case '1': case '2': case '3':	// Octal char constant
			case '4': case '5': case '6': case '7':
				if (!enabled(OctalChar))
					goto simple_escape;
				param = re.substr(i, 3);
				ch = param.asInt32(&int_error, 8, &scanned);
				if (int_error || scanned == 0)
				{		// I don't think this can happen
					error_message = "Illegal octal character";
					break;
				}
				i += scanned;
				ok = func(RxOp::RxoChar, ch);
				continue;

			case 'x':			// Hex byte (1 or 2 hex digits follow)
				if (!enabled(HexChar))
					goto simple_escape;
				param = re.substr(++i, 2);
				ch = param.asInt32(&int_error, 16, &scanned);
				if (int_error || scanned == 0)
				{
					error_message = "Illegal hexadecimal character";
					break;
				}
				i += scanned;
				ok = func(RxOp::RxoChar, ch);
				continue;

			case 'u':			// Unicode (1-5 hex digits follow)
				if (!enabled(UnicodeChar))
					goto simple_escape;
				param = re.substr(++i, 5);
				ch = param.asInt32(&int_error, 16, &scanned);
				if (int_error || scanned == 0)
				{
					error_message = "Illegal escaped unicode character";
					break;
				}
				i += scanned;
				ok = func(RxOp::RxoChar, ch);
				continue;

			case 's':			// Whitespace
				if (!enabled(Shorthand))
					goto simple_escape;
				ok = func(RxOp::RxoCharProperty, ' ');
				continue;

			case 'p':			// Posix character type
				if (!enabled(PropertyChars))
					goto simple_escape;
				if (re[++i] != '{')
				{
			bad_posix:	error_message = "Illegal Posix character specification";
					break;
				}
				param = re.substr(i+1);
				close = param.find('}');
				if (close <= 0)
					goto bad_posix;
				param = param.substr(0, close); // Truncate name to before '}'
				i += close;	// Now pointing at the '}'
				ok = func(RxOp::RxoCharProperty, param);
				continue;

			default:
			simple_escape:
				ok = func(RxOp::RxoChar, re[i]);
				continue;
			}
			break;

		case '[':		// Character class
			// REVISIT: implement character classes
			break;

		case '|':
			if (!supported(Alternates))
				goto simple_char;
			ok = func(RxOp::RxoAlternate, StrVal::null);
			continue;

		case '(':
			if (!supported(Group))
				goto simple_char;
			if (!enabled((RxFeature)(Capture|NonCapture|NegLookahead|Subroutine)) // not doing fancy groups
			 || re[i+1] != '?')					// or this one is plain
			{		// Anonymous group
				ok = func(RxOp::RxoNonCapturingGroup, StrVal::null);
				continue;
			}

			i += 2;	// Skip the '(?'
			if (supported(Capture) && re[i] == '<')		// Capture
			{
				param = re.substr(++i);
				close = param.find('>');
				if (close <= 0)
				{
			bad_capture:	error_message = "Invalid group name";
					break;
				}
				param = param.substr(0, close);		// Truncate name to before >
				i += close+1;
				ok = func(RxOp::RxoNamedCapture, param);
				continue;
			}
			else if (supported(NonCapture) && re[i] == ':')	// Noncapturing group
			{
				ok = func(RxOp::RxoNonCapturingGroup, StrVal::null);
				continue;
			}
			else if (supported(NegLookahead) && re[i] == '!') // Negative Lookahead
			{
				ok = func(RxOp::RxoNegLookahead, StrVal::null);
				continue;
			}
			else if (re[i] == '&')				// Subroutine
			{
				param = re.substr(++i);
				close = param.find(')');
				if (close <= 0)
					goto bad_capture;
				param = param.substr(0, close); // Truncate name to before >
				ok = func(RxOp::RxoSubroutine, param);
				i += close;	// Now pointing at the ')'
				continue;
			}
			else
			{
				error_message = "illegal group type";
				break;
			}
			continue;

		case ')':
			if (!enabled(Group))
				goto simple_char;
			ok = func(RxOp::RxoEndGroup, StrVal::null);
			continue;

		default:
		simple_char:
			ok = func(RxOp::RxoChar, ch);
			continue;
		}

		break;
	}
	if (i >= re.length())	// If we didn't complete, there was an error
	{
		ok = func(RxOp::RxoEnd, StrVal::null);
		return true;
	}
	return false;
}

/*
 * Evaluate the RE, counting the number of instructions, groups, captures, and parallel states
 * that are required to compile and execute it
void
RxCompiled::estimate_limits()
{
}
 */
