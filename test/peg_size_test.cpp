/*
 * PEG parser code size test.
 * This is not intended to be run, just to use `size` on.
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<char_encoding.h>
#include	<peg.h>
#include	<variant.h>
#include	<peg_ast.h>

#if	defined(PEG_UNICODE)
// This is probably wrong (emplate params have changed), but isn't being used
typedef Peg<GuardedUTF8Ptr, PegMatch, PegContext>	TestPegParent;
#else
typedef Peg<PegMemorySource, PegMatch, PegContext>	TestPegParent;
#endif

class TestPeg
: public TestPegParent
{
	static	Rule	rules[];
	static	int	num_rule;
public:
	TestPeg()
	: TestPegParent(rules, num_rule) {}
};

TestPeg::Rule	TestPeg::rules[] = { { "TOP", "", 0 } };	// Null rule set
int		TestPeg::num_rule = 1;

int
main(int argc, const char** argv)
{
	TestPeg		peg;
	TestPeg::Match	match;
	TestPeg::Source	source(argv[1]);

	match = peg.parse(source);
	return match.is_failure() ? 1 : 0;
}
