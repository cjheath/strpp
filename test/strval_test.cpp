#include	<strval.h>
#include	<stdio.h>

int
main(int argc, const char** argv)
{
	StrVal		s;
	StrVal		foo("foo");

	const UTF8*		f = foo.asUTF8();
	printf("f=`%s`\n", f);
}
