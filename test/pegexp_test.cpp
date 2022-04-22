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
#include	<string.h>

#if	defined(PEGEXP_UNICODE)
using	Source = UTF8P;
using	PegexpT = Pegexp<UTF8P, UCS4>;
#else
using	Source = TextPtrChar;
using	PegexpT = Pegexp<TextPtrChar, char>;
#endif

int
main(int argc, const char** argv)
{
	bool	verbose = false;

	if (argc > 2 && strcmp(argv[1], "-v") == 0)
	{
		verbose = true;
		argc--;
		argv++;
	}

	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s pegexp subject ...\n", *argv);
		return 1;
	}

	int	success_count = 0;
	int	failure_count = 0;
	for (const char** subject = argv+2; subject < argv+argc; subject++)
	{
		Source			text(*subject);
		PegexpT			pegexp(argv[1]);

		PegexpT::State		result = pegexp.match(text);	// text is advanced to the start of the match
		int			length = result ? result.text-text : -1;

		if (length < 0)
		{
			failure_count++;
			fprintf(stdout, "%s\t%s\tfailed", argv[1], *subject);
		}
		else
		{
			success_count++;
			if (verbose)
			{
				fprintf(stdout, "%s\t%s\t", argv[1], *subject);
				fprintf(stdout, "+%d\t%.*s\n", (int)(text - *subject), length, static_cast<const char*>(text));
			}
		}
	}

	return failure_count == 0 ? 0 : 1;
}

