/*
 * Pegular expressions, aka Pegexp, formally "regular PEGs"
 *
 * Possessive regular expressions using prefix operators
 *
 * Copyright 2022 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 *
 * Pegexp's use Regex-style operators, but in prefix position
 */
#define	PEGEXP_UNICODE	1

#if	defined(PEGEXP_UNICODE)
#include	<utf8pointer.h>
#endif

#include	<pegexp.h>
#include	<stdio.h>

#if	defined(PEGEXP_UNICODE)
using	Source = UTF8P;
using	PegexpT = Pegexp<UTF8P, UCS4>;
#else
using	Source = typename char;
using	PegexpT = typename Pegexp<>;
#endif

int
main(int argc, const char** argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s pegexp subject ...\n", *argv);
		return 1;
	}

	for (const char** subject = argv+2; subject < argv+argc; subject++)
	{
		Source			text(*subject);
		PegexpT			pegexp(argv[1]);

		PegexpT::Result		result = pegexp.match(text);
		int			length = result ? result.state.text-result.state.origin : -1;

		fprintf(stdout, "%s\t%s\t", argv[1], *subject);
		if (length >= 0)
			fprintf(stdout, "+%d\t%.*s\n", (int)(result.state.origin-text), length, static_cast<const char*>(result.state.origin));
		else
			fprintf(stdout, "failed\n");
	}

	return 0;
}

