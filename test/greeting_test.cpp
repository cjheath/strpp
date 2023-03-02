/*
 * Simple greeting test with emoji manipulation and case conversion
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include        <stdio.h>
#include        <strval.h>

void greet(StrVal greeting)
{       
	StrVal  decorated = greeting + "! 🎉🍾\n";
	fputs(decorated.asUTF8(), stdout);

	greeting.toUpper();
	fputs(greeting.asUTF8(), stdout);
	printf("\n");
}

int main()
{
	StrVal  hello("Hello, world");

	greet(hello);
}
