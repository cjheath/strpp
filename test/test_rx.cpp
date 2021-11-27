#include	<stdio.h>

#define	protected	public
#include	<strregex.h>

int
main(int argc, char** argv)
{
	for (--argc, ++argv; argc > 0; argc--, argv++)
	{
		RxCompiled	rx(*argv, (RxFeature)(RxFeature::AllFeatures | RxFeature::ExtendedRE));
		bool		scanned_ok;

		printf("Compiling '%s'\n", *argv);

		scanned_ok = rx.compile();

		if (!scanned_ok)
		{
			printf("Regex scan failed: %s\n", rx.ErrorMessage());
			continue;
		}

		rx.dump();

		// RxMatcher	rm(rx);
	}
}
