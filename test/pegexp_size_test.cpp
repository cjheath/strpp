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
#else
using	TestSource = PegexpPointerSource<NulGuardedCharPtr>;
#endif

using	TestResult = PegexpDefaultResult<TestSource>;
using	TestContext = PegexpNullContext<TestResult>;

typedef	Pegexp<TestContext>		TestPegexp;

int
main(int argc, const char** argv)
{
	TestPegexp		pegexp(argv[1]);
	TestSource		p(argv[2]);
	return pegexp.match_sequence(p) ? 0 : 1;
}
