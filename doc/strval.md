## Unicode strings: the StrVal class

`#include	<strval.h>`

- By-value semantics (use StrVal like int, no explicit allocation, pass by reference/pointer, etc)
- Copies and substrings are slices (they do not copy the data)
- All strings are stored as UTF-8
- All references to individual characters are UCS4 (UTF-32, aka Runes)
- All string indexing is by character position, not byte offsets
- String scanning and indexing is efficient, with internal use of bookmarks
- Content sharing is SMP and thread-safe using atomic reference counting and garbage collection
- Any StrVal may be mutated - it will safely make a private copy of any shared data

Read the header file for the full API.

Example:

	// This example creates precisely three strings, but with four references
	#include        <stdio.h>
	#include        <strval.h>

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
