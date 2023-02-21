/*
 * Pegex code size test
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<pegexp.h>

#if	defined(PEG_UNICODE)
typedef	Pegexp<UTF8P, UCS4>	TestPegexp;
#else
typedef	Pegexp<>		TestPegexp;
#endif

int
main(int argc, const char** argv)
{
	TestPegexp		pegexp(argv[1]);
	TextPtrChar		p(argv[2]);
	return !pegexp.match_here(p);
}
