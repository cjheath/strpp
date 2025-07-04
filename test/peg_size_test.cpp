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
	TestPeg::Rule	rules[] = { { "TOP", "" } };	// Null rule set

	TestPeg		peg(rules, 1);
	TestPeg::Match	match;
	TestPeg::Source	source(argv[1]);

	match = peg.parse(source);
	return match.is_failure() ? 1 : 0;
}
