## strpp - StringPlusPlus - pronounced "strip"

A value-oriented Unicode string library with reference-counting to manage shared slices

## Features

- By-value semantics (use StrRef like int, no explicit allocation, pass by reference/pointer, etc)
- Copies and substrings (aka slices) are free (do not copy the data)
- Any StrRef may be mutated - it will safely make a private copy of any shared data
- Content sharing is thread-safe and SMP safe using atomic reference counting and garbage collection
- All strings are stored as UTF-8
- All references to individual characters are UCS4 (UTF-32, aka Runes)
- All string indexing is by character position not byte offsets
- String scanning and indexing is made efficient through the use of bookmarks

Regular Expressions:
- Literal strings
- Escapes \0 \b \e \f \n \r \t \177 \xAB \u12345 \s
- Beginning/End of line ^, $
- Any-character "." (optionally "?", configurable)
- Repetition ?, *, +, {n,m}
- Alternates a|b
- Character classes [a-z], [^a-z] (TBD)
- Character properties \p{PropertyName} by callout to external functions (TBD)
- Non-capturing groups (abc) or (?:abc)
- Negative Lookahead (?!abc)
- Named capture groups (?<foo>abc) (capture return TBD)
- Subroutine calls (?&foo) (TBD)
- Extended regular expressions (skips whitespace and #comment-to-eol)
- Optional: "Any" excludes newline (TBD)
- Optional: * means .* as in the shell (TBD)
- Optional: case-insensitive matching (TBD)
- Optional: backtrack limiting (TBD)

Each regular expression feature may be enabled or disabled at compile time.

The compiler produces an NFA as a NUL-terminated character string
you can strlen() or put into a #define, so you can exclude the
compiler from your program.

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

