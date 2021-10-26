## strpp - StringPlusPlus - pronounced "strip"

A value-oriented character string library with reference-counting to manage shared slices

## Features

- By-value semantics (use StrRef like int, no explicit allocation, pass by reference/pointer, etc)
- Copies and substrings (aka slices) are free (do not copy the data)
- Any StrRef may be mutated - it will safely make a private copy of any shared data
- Content sharing is thread-safe and SMP safe using atomic reference counting and garbage collection
- All strings are stored as UTF-8
- All references to individual characters are UCS4 (UTF-32, aka Runes)
- All string indexing is by character position not byte offsets
- String scanning and indexing is made efficient through the use of bookmarks

## Example


	// This example creates precisely three strings, but with four references
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

