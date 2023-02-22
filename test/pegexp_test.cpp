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

#define	protected public

#include	<pegexp.h>
#include	<stdio.h>
#include	<string.h>

#if	defined(PEGEXP_UNICODE)
using	PegexpChar = UCS4;
using	Source = PegexpPointerInput<GuardedUTF8Ptr>;
#else
using	PegexpChar = char;
using	Source = PegexpPointerInput<>;
#endif

class	PegexpTestSource
: public Source
{
public:
	PegexpTestSource(const char* cp) : Source(cp) {}
	PegexpTestSource(const Source& c) : Source(c) {}
	PegexpTestSource(const PegexpTestSource& c) : Source(c) {}

	/* Additional methods for the test program */
	int		operator-(const char* start)			// Length from start to current location
			{ return rest()-start; }
	int		operator-(const PegexpPointerInput& start)	// Length from start to current location
			{ return rest()-start.data; }
	const char*	rest()
			{ return Source::data; }
};

using	PegexpT = Pegexp<PegexpTestSource, PegexpChar>;

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
		PegexpTestSource	text(*subject);
		PegexpT			pegexp(argv[1]);

		PegexpT::State		result = pegexp.match(text);	// text is advanced to the start of the match
		const char*		match_start = text.rest(); // static_cast<const char*>(text);
		int			offset = result ? text-*subject : -1;
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
				fprintf(stdout, "+%d\t%.*s\n", offset, length, match_start);
			}
		}
	}

	return failure_count == 0 ? 0 : 1;
}

