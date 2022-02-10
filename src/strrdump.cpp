/*
 * Unicode Strings
 * Regular expression diagnostic dump functions.
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<strregex.h>
#include	<string.h>

void
RxCompiler::dump(const char* nfa) const		// Dump NFA to stdout
{
	dumpHex(nfa);

	const	UTF8*	np;
	for (np = nfa; np < nfa+nfa_size;)
		if (!dumpInstruction(nfa, np))
			break;
}

void
RxCompiler::dumpHex(const char* nfa) const		// Dump binary code to stdout
{
	const	UTF8*	np;
	for (np = nfa; np < nfa+nfa_size; np++)
		printf("%02X%s", *np&0xFF, (np+1-nfa)%5 ? " " : "   ");
	printf("\n");
}

bool
RxCompiler::dumpInstruction(const char* nfa, const char*& np)	// Disassenble NFA to stdout
{
	int		num_names;
	StrVal		name;
	CharBytes	byte_count;		// Used for sizing string storage
	int		offset_this;
	int		offset_next;
	int		offset_alternate;
	int		group_num;
	uint8_t		op_num;
	const char*	op_name;
	UCS4		ucs4char;
	int		min, max;

	offset_this = np-nfa;
	printf("%d\t", offset_this);

	auto	zagzig =
		[](int i) -> int
		{
			return (i>>1) * ((i&01) ? -1 : 1);
		};

	// Emit an offset, advancing np, and returning the number of UTF8 bytes emitted
	auto	get_offset =
		[&](const char*& np) -> CharBytes
		{
			return (CharBytes)zagzig(UTF8Get(np));
                };

	auto	get_name =
		[&](const char* nfa, int i) -> StrVal
		{
			if (i == 0) return "";
			assert(*nfa++ == (char)RxOp::RxoStart);
			get_offset(nfa);	// start_station
			get_offset(nfa);	// search_station
			UTF8Get(nfa);		// max_station
			nfa++;			// max_counter
			nfa++;			// max_capture
			int	num_names = (*nfa++ & 0xFF) - 1;
			if (i <= 0 || i > num_names)
				return "BAD NAME NUMBER";
			CharBytes	byte_count;
			while (--i > 0)
			{			// Skip the preceding names
				byte_count = UTF8Get(nfa);
				nfa += byte_count;
			}
			byte_count = UTF8Get(nfa);
			assert(byte_count > 0);
			return StrVal(nfa, byte_count);
		};

	op_num = *np++ & 0xFF;
	switch ((RxOp)op_num)
	{
	case RxOp::RxoNull:
		printf("Null termination\n");
		return false;

	default:
		printf("Illegal NFA opcode %02X\n", op_num);
		return false;

	case RxOp::RxoStart:			// Place to start the DFA
	{
		int search_station = np-nfa;
		search_station += get_offset(np);
		int start_station = np-nfa;
		start_station += get_offset(np);
		int station_count = UTF8Get(np);
		int max_nesting = (*np++ & 0xFF);
		int max_capture = (*np++ & 0xFF);
		printf("NFA Start(%02X), search->%d, start->%d, station_count=%d, max_nesting=%d, max_capture=%d",
			op_num,
			search_station,
			start_station,
			station_count,
			max_nesting,
			max_capture
		);
		int num_names = (*np++ & 0xFF) - 1;
		if (num_names > 0)
		{
			printf(", names: %d@(%d)", num_names, (int)(np-nfa));
			for (int i = 0; i < num_names; i++)
			{
				byte_count = UTF8Get(np);
				printf(" %.*s", byte_count, np);
				np += byte_count;
			}
		}
		printf("\n");
	}
		break;

	case RxOp::RxoAccept:			// Termination condition
		printf("Accept(%02X)\n", op_num);
		break;

	case RxOp::RxoChar:			// Literal character
	{
		const char*	cp = np;
		ucs4char = UTF8Get(np);
		printf("Char(%02X) %d='%.*s'\n", op_num, ucs4char, (int)(np-cp), cp);
	}
		break;

	case RxOp::RxoBOL:			// Beginning of Line
		printf("BOL(%02X)\n", op_num);
		break;

	case RxOp::RxoEOL:			// End of Line
		printf("EOL(%02X)\n", op_num);
		break;

	case RxOp::RxoAny:			// Any single char
		printf("Any(%02X)\n", op_num);
		break;

	case RxOp::RxoJump:
		offset_next = get_offset(np);
		printf("Jump(%02X) %+d->%d\n", op_num, offset_next, offset_this+1+offset_next);
		break;

	case RxOp::RxoSplit:
		offset_alternate = get_offset(np);
		printf("Split(%02X) %+d->%d\n", op_num, offset_alternate, offset_this+1+offset_alternate);
		break;

	case RxOp::RxoCaptureStart:
		group_num = (*np++ & 0xFF) - 1;
		name = get_name(nfa, group_num);
		printf("CaptureStart(%02X) group '%s'(%d)\n", op_num, name.asUTF8(), group_num);
		break;

	case RxOp::RxoCaptureEnd:
		group_num = (*np++ & 0xFF) - 1;
		name = get_name(nfa, group_num);
		printf("CaptureEnd(%02X) group '%s'(%d)\n", op_num, name.asUTF8(), group_num);
		break;

	case RxOp::RxoNegLookahead:		// (?!...)
		offset_next = get_offset(np);
		printf("NegLookahead(%02X) bypass=(%+d)->%d\n", op_num, offset_next, offset_this+1+offset_next);
		break;

	case RxOp::RxoCharProperty:		// A char with a named Unicode property
		op_name = "CharProperty";
		goto string;
	case RxOp::RxoCharClass:		// Character class; instr contains a string of character pairs
		op_name = "CharClass";
		goto string;
	case RxOp::RxoNegCharClass:		// Negated Character class
		op_name = "NegCharClass";
	string:
		byte_count = UTF8Get(np);
		printf("%s(%02X), '%.*s'\n", op_name, op_num, byte_count, np);
		np += byte_count;
		break;

	case RxOp::RxoSubroutineCall:		// Subroutine call to a named group
		group_num = (*np++ & 0xFF);	// We don't add one to these because you can't call group 0
		name = get_name(nfa, group_num).asUTF8();
		printf("SubroutineCall(%02X) call to '%s'(%d)\n", op_num, name.asUTF8(), group_num);
		break;

	case RxOp::RxoEndGroup:			// End of a group
		printf("ERROR: EndGroup(%02X) should not be emitted\n", op_num);
		break;

	case RxOp::RxoAlternate:		// |
		offset_next = UTF8Get(np);
		printf("ERROR: Alternate(%02X) next=(%+d)->%d should not be emitted\n", op_num, offset_next, offset_this+offset_next);
		break;

	case RxOp::RxoNonCapturingGroup:	// (...)
		offset_next = UTF8Get(np);
		printf("ERROR: NonCapturingGroup(%02X) offset=(%+d)->%d should not be emitted\n", op_num, offset_next, offset_this+offset_next);
		break;

	case RxOp::RxoNamedCapture:		// (?<name>...)
		offset_next = UTF8Get(np);
		group_num = (*np++ & 0xFF) - 1;
		name = get_name(nfa, group_num);
		printf("ERROR: NamedCapture(%02X) '%s'(%d), offset=(%+d)->%d should not be emitted\n", op_num, name.asUTF8(), group_num, offset_next, offset_this+offset_next);
		break;

	case RxOp::RxoLiteral:			// A specific string
		op_name = "Literal";
		byte_count = UTF8Get(np);
		printf("ERROR: Literal(%02X), '%.*s' should not be emitted\n", op_num, byte_count, np);
		np += byte_count;
		break;

	case RxOp::RxoRepetition:		// {n, m}
		min = (*np++ & 0xFF) - 1;
		max = (*np++ & 0xFF) - 1;
		printf("ERROR: Repetition(%02X) min=%d max=%d should not be emitted\n", op_num, min, max);
		break;

	case RxOp::RxoZero:
		printf("Zero(%02X)\n", op_num);
		break;

	case RxOp::RxoCount:
		min = (*np++ & 0xFF) - 1;
		max = (*np++ & 0xFF) - 1;
		offset_alternate = get_offset(np);
		printf("Count(%02X) min=%d max=%d repeating at %+d->%d\n", op_num, min, max, offset_alternate, offset_this+3+offset_alternate);
		break;
	}
	return true;
}
