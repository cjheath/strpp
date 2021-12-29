/*
 * Unit test driver for matching Regular Expressions
 */
#include	<stdio.h>

#define	protected	public
#include	<strregex.h>

#include	"memory_monitor.h"

bool	verbose = false;
int	automated_tests();

int
main(int argc, char** argv)
{
	if (argc > 1 && 0 == strcmp("-v", argv[1]))
	{
		verbose = true;
		argc--; argv++;
	}

	RxMatcher*	matcher = 0;
	char*		nfa = 0;
	for (--argc, ++argv; argc > 0; argc--, argv++)
	{
		if (**argv == '/')
		{
			StrVal		re(*argv+1);

			if (re.tail(1) == "/")	// Remove the trailing /
				re = re.shorter(1);
			printf("Compiling \"%s\"\n", re.asUTF8());

			RxCompiler	rx(re, (RxFeature)(RxFeature::AllFeatures | RxFeature::ExtendedRE));
			bool		scanned_ok;
			delete nfa;
			nfa = 0;
			if (!rx.compile(nfa))
			{
				printf("Regex scan failed: %s\n", rx.ErrorMessage());
				matcher = 0;
				continue;
			}
			rx.dump(nfa);

			delete matcher;
			matcher = new RxMatcher(nfa);
		}
		else if (matcher)
		{
			RxMatch	match = matcher->match_after(*argv);

			if (match.succeeded)
			{
				StrVal	matched_substr = match.target.substr(match.offset, match.length);
				printf("\t\"%s\" matched at [%d, %d]: %s\n", match.target.asUTF8(), match.offset, match.length, matched_substr.asUTF8());
				continue;
			}
			printf("\t\"%s\" failed\n", *argv);
		}
		else
			printf("\"%s\" has no regex to match\n", *argv);
	}
	return 0;
}
