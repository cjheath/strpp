/*
 * Pegex code size test
 *
 * Using guarded pointers makes no difference here, because Pegexp's never back up.
 * NulGuarded pointers still check for end-of-string.
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<pegexp.h>
#include	<utf8_ptr.h>

#define	PEG_UNICODE 1

#if	defined(PEG_UNICODE)
using	TestSource = PegexpPointerSource<NulGuardedUTF8Ptr>;
typedef	Pegexp<TestSource>		TestPegexp;
#else
using	TestSource = PegexpPointerSource<NulGuardedCharPtr>;
typedef	Pegexp<TestSource>		TestPegexp;
#endif

int
main(int argc, const char** argv)
{
	TestPegexp		pegexp(argv[1]);
	TestSource		p(argv[2]);
	return pegexp.match_here(p).ok() ? 0 : 1;
}
