/*
 * Pegex code size test
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<pegexp.h>

// #define	PEG_UNICODE 1

#if	defined(PEG_UNICODE)
using	Source = PegexpPointerInput<GuardedUTF8Ptr>;
typedef	Pegexp<Source, UCS4>	TestPegexp;
#else
using	Source = PegexpPointerInput<>;
typedef	Pegexp<Source>		TestPegexp;
#endif

int
main(int argc, const char** argv)
{
	TestPegexp		pegexp(argv[1]);
	Source			p(argv[2]);
	return pegexp.match_here(p) ? 0 : 1;
}
