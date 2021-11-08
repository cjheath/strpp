#include	<stdio.h>

#define	protected	public
#include	<strregex.h>

bool re_lex(RxCompiled::RxOp op, StrVal param)
{
	const char*	op_name;

	switch (op)
	{
	case RxCompiled::RxOp::RxoStart:		// Place to start the DFA
		op_name = "Start"; break;
	case RxCompiled::RxOp::RxoChar:			// A specific char
		op_name = "Char"; break;
	case RxCompiled::RxOp::RxoCharProperty:		// A char with a named Unicode property
		op_name = "CharProperty"; break;
	case RxCompiled::RxOp::RxoBOL:			// Beginning of Line
		op_name = "BOL"; break;
	case RxCompiled::RxOp::RxoEOL:			// End of Line
		op_name = "EOL"; break;
	case RxCompiled::RxOp::RxoCharClass:		// Character class
		op_name = "CharClass"; break;
	case RxCompiled::RxOp::RxoNegCharClass:		// Negated Character class
		op_name = "NegCharClass"; break;
	case RxCompiled::RxOp::RxoAny:			// Any single char
		op_name = "Any"; break;
	case RxCompiled::RxOp::RxoZeroOrOne:		// ?
		op_name = "ZeroOrOne"; break;
	case RxCompiled::RxOp::RxoZeroOrMore:		// *
		op_name = "ZeroOrMore"; break;
	case RxCompiled::RxOp::RxoOneOrMore:		// +
		op_name = "OneOrMore"; break;
	case RxCompiled::RxOp::RxoCountedRepetition:	// {n, m}
		op_name = "CountedRepetition"; break;
	case RxCompiled::RxOp::RxoNonCapturingGroup:	// (...)
		op_name = "NonCapturingGroup"; break;
	case RxCompiled::RxOp::RxoNamedCapture:		// (?<name>...)
		op_name = "NamedCapture"; break;
	case RxCompiled::RxOp::RxoNegLookahead:		// (?!...)
		op_name = "NegLookahead"; break;
	case RxCompiled::RxOp::RxoAlternate:		// |
		op_name = "Alternate"; break;
	case RxCompiled::RxOp::RxoSubroutine:		// Subroutine call to a named group
		op_name = "Subroutine"; break;
	case RxCompiled::RxOp::RxoEndGroup:		// End of a group
		op_name = "EndGroup"; break;
	case RxCompiled::RxOp::RxoEnd:			// Termination condition
		op_name = "End"; break;
	default:
		break;
	}
	if (param)
		printf("\t%s('%s')\n", op_name, param.asUTF8());
	else
		printf("\t%s\n", op_name);
	return true;	// Ok to continue
}

int
main(int argc, char** argv)
{
	for (--argc, ++argv; argc > 0; argc--, argv++)
	{
		RxCompiled	rx(*argv);
		bool		scanned_ok;

		printf("Compiling '%s'\n", *argv);

		scanned_ok = rx.scan_rx(re_lex);

		if (!scanned_ok)
			printf("Regex scan failed: %s\n", rx.ErrorMessage());
	}
}
