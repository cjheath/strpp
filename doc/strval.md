## Unicode strings: the StrVal class

`#include	<strval.h>`

- By-value semantics (use StrVal like int, no explicit allocation, pass by
  reference/pointer, etc)
- Copies and substrings are slices (they do not copy the data)
- All strings are stored as UTF-8
- All references to individual characters are UCS4 (UTF-32, aka Runes)
- All string indexing is by character position, not byte offsets
- String scanning and indexing is efficient, with internal use of bookmarks
- Content sharing is SMP and thread-safe using atomic reference counting and
  garbage collection
- Any StrVal may be mutated - it will safely make a private copy of any
  shared data

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

### Sharing, slices and NUL termination

A StrVal holds a reference to a reference-counted body, and a substring is a
slice of that body - it does not copy the characters. Two consequences worth
knowing:

- **While any slice of a body is alive, the body is shared**, so writing to
  any of them copies it first. That is what makes StrVal safe to pass by
  value, and it is also why a long-lived slice of a large string costs a
  copy when the string is next written to.

- **`asUTF8()` guarantees NUL termination; `asUTF8(Index& length)` does
  not.** The second form is for a byte range you already have a length for,
  and it will not copy the string to add a terminator. Where the bytes go to
  something that expects a C string, use the first form.

The empty string is the same for everyone: every `StrVal("")` refers to one
static empty body, which is why they can be built and copied freely.
