/*
 * Unicode String regular expressions
 */
#include	<strregex.h>
#include	<string.h>

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
	std::vector<StrVal>	names;			// Build the names for printing
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
