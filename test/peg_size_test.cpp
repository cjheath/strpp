/*
 * PEG parser code size test.
 * This is not intended to be run, just to use `size` on.
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<peg.h>

#if	defined(PEG_UNICODE)
typedef	Peg<GuardedUTF8Ptr>	TestPeg;
#else
typedef	Peg<>			TestPeg;
#endif

int
main(int argc, const char** argv)
{
	TestPeg::Rule	rules[] = { { "TOP", "", { 0 } } };	// Null rule set

	TestPeg		peg(rules, 1);
	TestPeg::Result	result;
	TestPeg::State	finish = peg.parse(argv[1], &result);
}
