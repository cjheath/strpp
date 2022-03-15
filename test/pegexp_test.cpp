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

#include	<pegexp.h>
#include	<stdio.h>

#if	defined(PEGEXP_UNICODE)
#include	<utf8pointer.h>
#endif

// Pegexp matching engine for UTF8 strings
typedef	Pegexp<UTF8P, UCS4>	PegexpUTF8;

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
#if	defined(PEGEXP_UNICODE)
		UTF8P   	text(*subject);
		PegexpUTF8	pegexp(argv[1]);
#else
		const char*	text(*subject);
		Pegexp<>	pegexp(argv[1]);
#endif

		int		length = pegexp.match(text);
		fprintf(stdout, "%s\t%s\t", argv[1], *subject);
		if (length >= 0)
			fprintf(stdout, "+%d\t%.*s\n", (int)(text - *subject), length, static_cast<const char*>(text));
		else
			fprintf(stdout, "failed\n");
	}

	return 0;
}

