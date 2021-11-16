#include	<stdio.h>

#define	protected	public
#include	<strregex.h>

bool re_lex(const RxCompiled::RxInstruction& instr)
{
	const char*	op_name = 0;
	CharBytes	len;
	const UTF8*	cp = 0;
	if (instr.str)
		cp = instr.str.asUTF8(len);

	switch (instr.op)
	{
	case RxCompiled::RxOp::RxoStart:		// Place to start the DFA
		op_name = "Start"; break;
	case RxCompiled::RxOp::RxoLiteral:		// A specific string
		op_name = "Literal"; break;
	case RxCompiled::RxOp::RxoCharProperty:		// A char with a named Unicode property
		op_name = "CharProperty"; break;
	case RxCompiled::RxOp::RxoBOL:			// Beginning of Line
		op_name = "BOL"; break;
	case RxCompiled::RxOp::RxoEOL:			// End of Line
		op_name = "EOL"; break;
	case RxCompiled::RxOp::RxoCharClass:		// Character class
		op_name = "CharClass";			// REVISIT: show clar class
		// Fall through
	case RxCompiled::RxOp::RxoNegCharClass:		// Negated Character class
		if (!op_name)
			op_name = "NegCharClass";
		printf("\t%s[ REVISIT ]\n", op_name);
		return true;
	case RxCompiled::RxOp::RxoAny:			// Any single char
		op_name = "Any"; break;
	case RxCompiled::RxOp::RxoRepetition:		// {n, m}
		op_name = "Repetition";
		printf("\t%s(min=%d, max=%d)\n", op_name, instr.repetition.min, instr.repetition.max);
		return true;
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
	if (cp)
		printf("\t%s('%.*s')\n", op_name, len, cp);
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
