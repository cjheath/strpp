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

bool RxCompiled::scan_rx(bool (*func)(const RxInstruction&))
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
				if (int_error)
					goto bad_repetition;
			} // else min == 0
			i += close+1;	// Skip the ',' or '}'

			if (re[i-1] == ',')		// There is a second number
			{
				param = re.substr(i);
				close = param.find('}');
				if (close < 0)
					goto bad_repetition;
				max = param.substr(0, close).asInt32(&int_error, 10, &scanned);
				if (int_error == STRERR_NO_DIGITS)
					max = 0;
				else if (int_error)
					goto bad_repetition;
				i += close+1;
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
				param = re.substr(++i, 5);
				ch = param.asInt32(&int_error, 16, &scanned);
				if ((int_error && int_error != STRERR_TRAIL_TEXT) || scanned == 0)
				{
					error_message = "Illegal Unicode escape";
					break;
				}
				i += scanned-1;	// The loop will advance one character
		escaped_char:	delayed += ch;
				ok = true;
				continue;

			case 's':			// Whitespace
				if (!enabled(Shorthand))
					goto simple_escape;
				if (!(ok = flush())) continue;
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
				i += close;	// Now pointing at the '}'
				ok = func(RxInstruction(RxOp::RxoCharProperty, param));
				continue;

			default:
		simple_escape:
				delayed += re[i];
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
				if (ch == '\\')		// Any single Unicode char can be escaped
					if ((ch = re[++i]) == '\0')
						goto bad_class; // RE ends with the backslash

				param += ch;	// Start of range pair
				if (re[i+1] == '-' && re[i+2] != ']')
				{		// Character range
					if ((ch = re[i += 2]) == '\0')
						goto bad_class;
					if (ch == '\\')		// Any single Unicode char can be escaped
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
				i += close+1;
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

		default:
		simple_char:
			if (delayed.length() == 0)
				delayed = re.substr(i, 1);	// Start an re substring - lower cost
			else if (delayed.isShared())		// Continue an re substring
				delayed = re.substr(i-delayed.length(), delayed.length()+1);
			else					// Not using an re substring
			{
				printf("Adding simple_char '%c'\n", (char)re[i]);
				delayed += re.substr(i, 1);
			}
			continue;
		}

		break;
	}
	(void)flush();
	if (!error_message && i >= re.length())	// If we didn't complete, there was an error
	{
		ok = func(RxInstruction(RxOp::RxoEnd));
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
