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
#include	<cstdio>
#include	<cstring>
#include	<array.h>

#include	<utf8_ptr.h>

#if	defined(PEGEXP_UNICODE)
using	TestSource = PegexpPointerSource<>;
#else
using	TestSource = PegexpPointerSource<char*, char>;	// REVISIT: Change template parameter to bool=true to request Unicode
#endif

class	PegexpTestSource
: public TestSource
{
public:
	PegexpTestSource() : TestSource(), start(0) {}
	PegexpTestSource(const char* cp) : TestSource(cp), start(cp) {}
	PegexpTestSource(const PegexpTestSource& c) : TestSource(c), start(c.start) {}

	/* Additional methods for the test program */
	int		operator-(const char* from)			// Length from "from" to current location
			{ return data-from; }
	int		operator-(const PegexpTestSource& from)		// Length from "from" to current location
			{ return data - from.rest(); }
	const char*	rest() const
			{ return TestSource::data; }
protected:
	const char*	start;
};

using	PegexpTestMatch = PegexpDefaultMatch<PegexpState<PegexpTestSource>>;

class	PegexpTestContext
{
public:
	using	Source = PegexpTestSource;
	using	Match = PegexpTestMatch;
	using	State = Match::State;

	PegexpTestContext()
	: capture_disabled(0)
	, repetition_nesting(0)
	, captures()
	{}

	// Captures are disabled inside a look-ahead (which can be nested). This holds the nesting count:
	int		capture_disabled;

	// Calls to capture() inside a repetition happen with in_repetition set. This holds the nesting.
	int		repetition_nesting;	// A counter of repetition nesting

	// Every capture gets a capture number, so we can roll it back if needed:
	int		capture_count() const { return 0; }

	// A capture is a named Match. capture() should return the capture_count afterwards.
	int		capture(PatternP name, int name_len, Match r, bool in_repetition)
			{
				captures.push({name, name_len, r.from.source, r.to.source-r.from.source});
				return captures.length();
			}

	// In some grammars, capture can occur within a failing expression, so we can roll back to a count:
	void		rollback_capture(int count) {}

	// When an atom of a Pegexp fails, the atom (pointer to start and end) and Source location are passed here
	// Recording this can be useful to see why a Pegexp didn't proceed further, for example.
	void		record_failure(PatternP op, PatternP op_end, Source location) {}

	Match		match_result(State _from, State _to)
			{ return match = Match(_from, _to); }

	Match		match_failure(State at)
			{ return match = Match(at, at); }

	struct Captured
	{
		Captured() : capture() {}
		Captured(PatternP _name, int _name_len, Source _capture, int _length)
		: name(_name), name_len(_name_len), capture(_capture), length(_length)
		{}

		PatternP	name;
		int		name_len;
		Source		capture;
		int		length;
	};
	Array<Captured>	captures;

	// On completion, the match source section (start and finish location) are stored here:
	Match		match;
};

using	TestPegexp = Pegexp<PegexpTestContext>;

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
		PegexpTestSource	origin(*subject);
		PegexpTestSource	source(origin);
		TestPegexp		pegexp(argv[1]);
		PegexpTestContext	context;
		PegexpTestMatch		match;
		off_t			offset;
		const char*		match_start;
		int			length;

		match = pegexp.match(source, &context);	// source is advanced to the start of the match

		if (match.is_failure())
		{
			failure_count++;
			fprintf(stdout, "%s\t%s\tfailed\n", argv[1], *subject);
		}
		else
		{
			offset = match.from.source - origin;
			match_start = context.match.from.source.rest();
			length = context.match.to.source.rest() - match_start;

//			if (context.captures.length()) ...  // REVISIT: Work out how to integrate this into the test automation

			success_count++;
			if (verbose)
			{
				fprintf(stdout, "%s\t%s\t", argv[1], *subject);
				fprintf(stdout, "+%d\t%.*s\n", (int)offset, length, match_start);
			}
		}
	}

	return failure_count == 0 ? 0 : 1;
}
