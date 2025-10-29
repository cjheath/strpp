## strpp - StringPlusPlus - pronounced "strip" - Regular Expressions

### Regular Expressions

`#include	<strregex.h>`

The Regular Expression implementation uses the Unicode String slice class.
It implements the Thompson/Pike algorithm (extended with counters) using a compiled VM approach,
so it has linear runtime and strictly predictable (and limited) memory usage.
You can even precompile expressions to a C string that can be included as a simple #define in your runtime program.

An instance of <strong>RxCompiler</strong> compiles from a StrVal
to a plain null-terminated C string containing optimised instructions
for the <strong>RxProgram</strong> to execute against some StrVal,
yielding an <strong>RxResult</strong>.

Almost supported features may be de/configured by passing a bitmask of <strong>RxFeature</strong> to the compiler:
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
- Negative Lookahead (?!abc)
- Optional: "Any" excludes newline (TBD)
- Optional: * means .* as in the shell (TBD)
- Optional: case-insensitive matching (TBD)

### LICENSE

The MIT License. See the LICENSE file for details.
