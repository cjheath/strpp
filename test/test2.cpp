#include        <stdio.h>
#include        "strpp.h"

void greet(StrVal greeting)
{       
	StrVal  decorated = greeting + "! 🎉🍾\n";
	fputs(decorated.asUTF8(), stdout);
}

int main()
{
	StrVal  hello("Hello, world");

	greet(hello);
}
