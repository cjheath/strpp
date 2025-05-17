/*
 * PEG parser code size test
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
	using R = TestPeg::Rule;

	TestPeg		peg((R*)0, 0);
	peg.parse(argv[1]);
}
