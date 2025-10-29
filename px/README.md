## Px, a PEG parser generator language

Px is a grammar-description language and compiler for parsers and code generators,
which also generates grammar documentation and (in future) IDE syntax highlighting.

The grammars are recursive-descent LL(*), expressed using Parsing Expression Grammar
patterns (but with operators in the prefix position, unlike BNF), using unlimited
look-ahead assertions that have the full power of the grammar. This makes it possible
to parse languages that cannot be handled by most other parsing approaches.

### Rules

A parser production rule in Px consists of a name applying to a
[BNF](https://en.wikipedia.org/wiki/Backus–Naur_form) (Backus-Naur form) modified by
omitting angle brackets and moving repetition operators (_?_, _*_, _+_) into the
prefix position, followed by a list of capture symbols that designate the semantic
elements of the rule. Each rule is terminated by at least one blank line. The token
_//_ indicates the start of a comment to end of line.

Example:
```
// JSON grammar in Px. See http://json.org/ and ECMA-262 Ed.5
// Unicode uses 16-bit format, so Emoji's require a surrogate pair, see ISO/IEC 10646
// High surrogate first: D800 to DBFF, Low surrogate: D800 to DBFF
// https://www.ecma-international.org/wp-content/uploads/ECMA-404_2nd_edition_december_2017.pdf

s	    = *[ \t\r\n]				// Zero or more space characters

TOP	    = s (| object |array |string |'true':v |'false':v |'null':v |number ) s
	-> object, array, string, v, number

object	= '{' *(|(string:k ':' TOP:v *(',' string:k ':' TOP:v)) | s) '}'
	-> k, v

array	= '[' (|(TOP:e *(',' TOP:e)) |s) ']'
	-> e

string	= s ["] *(|[^"\\\u{0}-\u{1F}]:c |escape:c) ["] s
	-> c

escape	= [\\] (|["\\/bfnrt] | 'u' [0-9a-fA-F] [0-9a-fA-F] [0-9a-fA-F] [0-9a-fA-F] )

number	= ?'-' (|'0' | [1-9] *[0-9]) ?('.' +[0-9]) ?([eE] ?[-+] +[0-9])
```

### Operators

Px rules may make use of the following operators and tokens:

-	.	any character (alternately, byte), including a newline
-	?	Zero or one of the following expression
-	*	Zero or more of the following expression
-	+	One or more of the following expression
-	(expr)	Group subexpressions (does not imply capture)
-	|A|B...	Either A or B (or ...)
-	&A	Continue only if A succeeds
-	!A	Continue only if A fails
-   'any string'
-   [character class] (may start with ^ for negation, and include character properties)
-   \C where C is a character property
-   the name of any rule

Any atom may be followed by a label preceded by a colon character. These labels are used in designating captures.

Not (yet) included in Px are:
-	^	start of the input or start of any line
-	$	end of the input or the end of any line

Strings and classes may use:
-   a character property escape, see the list below
-   \\\\      match a literal backslash
-	\char	match the escaped character (including the operators, 0 b e f n r t, and any other char)
-	\177	match the specified octal character
-	\xXX	match the specified hexadecimal (0-9a-fA-F)
-	\x{1-2}	match the specified hexadecimal (0-9a-fA-F)
-	\u1234	match the specified 1-4 digit Unicode character (only if compiled for Unicode support)
-	\u{1-8}	match the specified 1-8 digit Unicode character (only if compiled for Unicode support)
-	anychar	Any other character not preceded by a backslash

Character properties:
-	\a	alpha character (alternately, byte)
-	\d	digit character (alternately, byte)
-	\h	hexadecimal
-	\s	whitespace character (alternately, byte)
-	\L	lowercase character
-	\U	uppercase character
-	\w	word (alpha or digit) character (alternately, byte)

### Captures

A rule may be followed by the symbol _->_ and a comma-separated list of rule names.
These names designate elements of the rule which carry the semantic content of the
grammar, and hence should be captured into an abstract syntax tree. During parsing,
all occurrences of the captured items are accumulated into an ordered array of items,
even when the same item occurs more than once in a rule. This makes it convenient to
capture all items in a comma-separated list, for example.

Note how in the above example, the rule _object_ will capture two correlated arrays
of keys and values respectively, _array_ will capture one array of elements, and
_string_ will capture an array of items which encode one character each.

### Command line

Command-line parsing for Px is rudimentary and will change. For now, the options are:

`px [ -r [ -x excluded] ... ] file.px`

-   with no options, _px_ generates a parse table in C++
- -r    generate an HTML file with embedded Javascript calls to generate railroad diagrams
- -x xx Exclude rule xx from the railroad diagrams (e.g. for non-significant whitespace). May be repeated
