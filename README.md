## strpp - StringPlusPlus - pronounced "strip"

A value-oriented Unicode string library with reference-counting to manage shared slices

## Unicode strings

- By-value semantics (use StrVal like int, no explicit allocation, pass by reference/pointer, etc)
- Copies and substrings (aka slices) are free (do not copy the data)
- All strings are stored as UTF-8
- All references to individual characters are UCS4 (UTF-32, aka Runes)
- All string indexing is by character position not byte offsets
- Efficient string scanning and indexing through internal use of bookmarks
- Content sharing is thread-safe and SMP safe using atomic reference counting and garbage collection
- Any StrVal may be mutated - it will safely make a private copy of any shared data

## Regular Expressions
- Literal strings
- Escapes \0 \b \e \f \n \r \t \177 \xAB \u12345
- Beginning/End of line ^, $
- Any-character "." (optionally "?", configurable)
- Repetition ?, *, +, {n,m}
- Alternates a|b
- Character classes [a-z], [^a-z]
- Character property shorthand: \s \d \h
- Character extended properties \p{PropertyName} by callout to external functions (TBD)
- Non-capturing groups (abc) or (?:abc)
- Named capture groups (?<foo>abc)
- Extended regular expression syntax (skips whitespace and #comment-to-eol)
- Negative Lookahead (?!abc) (TBD)
- Subroutine calls (?&foo) (TBD)
- Optional: "Any" excludes newline (TBD)
- Optional: * means .* as in the shell (TBD)
- Optional: case-insensitive matching (TBD)

Most regular expression features may be enabled or disabled at compile time.

The optimising compiler produces an NFA as a NUL-terminated character string
that you can strlen() or put into a #define, so you can exclude the compiler
from your program.

The regular expression matcher uses a version of Pike's breadth-first algorithm, extended
with counters, so it has linear runtime and strictly predictable (and limited) memory usage.

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

## LICENSE

The MIT License. See the LICENSE file for details.
