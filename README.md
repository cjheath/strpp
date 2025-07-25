## strpp - StringPlusPlus - pronounced "strip"

A value-oriented C++ library implementing maps, arrays and strings, with
slices, Unicode processing, a Variant type, pattern-matching and parsing.

This lightweight library implements *value semantics* on shared objects
and sliced arrays using atomic reference-counting with copy-on-write.

Values may be passed by (atomically) copying references, which means you
can pass or "copy" complex structures cheaply without fear of aliasing,
memory leaks, or object lifetime violations.

A ReDOS-safe (Thompson algorithm) implementation of regular expressions
is provided, but is deprecated in favour of greedy PEG expressions
with look-ahead assertions, which do not require a compilation step for
efficient execution. These _Pegular expressions_ are composed into full
PEG grammars with a parser generator, and can use the Variant class
to construct Abstract Syntax Trees for any grammar specified in the
Px language.

Designed to minimise dynamic memory allocation, _strpp_ provides efficient
but advanced functionality on machines with restricted memory, such as
many embedded systems.

## Raw Unicode character processing

`#include <char_encoding.h>`

<pre>
typedef char		UTF8;		// We don't assume un/signed
typedef uint16_t	UTF16;		// Used in Unicode 2, and 3 with surrogates
typedef	char32_t	UCS4;		// A UCS4 character, aka UTF-32, aka Rune
</pre>

The full 32-bit range of UCS4 may be encoded using six-byte UTF-8
(not just 31 bits as in some implementations).  But beware, because an
illegal UTF-8 sequence is processed as a series of replacement characters,
encoded as the `xx` bits in 0x800000xx. In this way there is no need for
an exception to be thrown on an illegal sequence.

A number of functions deal with classifying UCS4 values. A short list
is here, but read the header file for details:

UCS4IsAlphabetic, UCS4IsIdeographic, UCS4IsDecimal, UCS4Digit, UCS4ToUpper,
UCS4ToLower, UCS4ToTitle, UCS4IsWhite, UCS4IsASCII, UCS4IsLatin1,
UCS4IsUTF16, UCS4IsUnicode, and others.

The actual classification functions use reduced tables. You might wish
to expand these for fully legal Unicode processing (or a future implementation
may provide them as a compile-time option).

There are three useful inline functions for dealing with UTF8 pointers:
<pre>
UCS4		UTF8Get(const UTF8*& cp);	// Get next UCS4 or replacement, advancing cp
const UTF8*	UTF8Backup(const UTF8* cp, const UTF8* limit = 0); // Backup cp to start of previous char
void		UTF8Put(UTF8*& cp, UCS4 ch);	// Put ch as 1-6 bytes of UTF8, advancing cp
</pre>

Similar methods exist for UTF16, but we don't like using those.

## Error and ErrNum type

`#include <error.h>`

Error is a lightweight type intended to encapsulate success or failure reason from a function.
It includes:
- a 32-bit ErrNum (which subsumes <strong>errno</strong> and <strong>HRESULT</strong>)
- a <strong>const char*</strong> to a default message text format string
- an optional counted reference to an array of variable typed arguments to be inserted into a format string (this array type is not yet implemented)

Error and the ErrNum class has a constexpr cast to <strong>int32_t</strong>
which allows the values to be used in switch statements.  The actual
number is made up of a 16-bit subsystem identifier (a message set number)
and a 14-bit message number within that set.  Message Numbers in each set
should be defined in a message catalog file, and generated to #defines
in a header file.  The intention is that each message may contain text
in one or more natural languages, allowing a message to be formatted with
parameter values in the user's locale.

The tooling to automate this will be included in this repository later.

Example:

	#include	<subsys_errors.h>
		....
		return SubsysErrorOfSomeType(param1, param2, ...)
		....

		switch (Error e = DoSomeWork())
		{
		case 0:
			// That seemed to go ok
			break;
		case ENOENT:
			// No such file or directory!
			break;

		case SubsysErrorOfSomeTypeNum:
			// Handle the error
			Complain(e);
			return;
		}

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

## Array slices

`#include	<array.h>`

The Array<T> template creates a slice into an array of type T.
New slices (and copies) onto the same ArrayBody are inexpensive (using atomic reference-counting),
but any attempt to modify a slice first creates a copy of the Body, leaving other slices unaffected.
The ArrayBody itself is only accessible as a constant, and a new Array may be created over a static body.

Read the header file for the API.

The StrVal class is a specialisation of this template, which provides its storage and reference counting.

## Prefix Regular Expression pattern matching

`#include	<pegexp.h>`

This template class implements Prefix Regular Expressions (aka <strong>pegular expressions</strong> or pegexp).

Regular expressions describe text patterns, backtracking after a failure caused by greedy repetition.

Pegular expressions also match text, but never backtrack, which requires avoiding excessive repetition using look-aheads.
Repetition and optional operators precede the atom they affect (hence <strong>prefix</strong>).
This prefix notation means no pre-compiler is needed for efficient execution, which reduces memory demand.

Alternates and repetition are possessive and will never backtrack.
 * Once an alternate has matched, no subsequent alterative will be tried in that group.
 * Once a repetition has been made, it will never be unwound.
 * It is your responsibility to ensure these possessive operators never match unless it's final.
 * You should use negative assertions to control inappropriate greed

The advantage of Pegexp over Regexp is there is no easy ReDOS attack, and no computational or memory expense to avoiding that.

The syntax implemented here (unlike regexp, BNF and normal PEG grammars) uses <strong>prefix notation</strong>,
which allows an efficient interpreter without needing a compiler.

|	Atom	|	Matches	|
|	---	|	---	|
|	^	|	start of the input or start of any line	|
|	$	|	end of the input or the end of any line	|
|	.	|	any character (alternately, byte), including a newline	|
|	?	|	Zero or one of the following expression	|
|	*	|	Zero or more of the following expression	|
|	+	|	One or more of the following expression	|
|	(expr)	|	Group subexpressions (does not capture; use a label for that)	|
|	\|A\|B...	|	Either A or B (or ...)	|
|	&A	|	Continue only if A succeeds	|
|	!A	|	Continue only if A fails	|
|	anychar	|	match that non-operator character	|
|	\char	|	match the escaped character (including the operators, 0 b e f n r t, and any other char)	|
|	\a	|	alpha character (alternately, byte)	|
|	\d	|	digit character (alternately, byte)	|
|	\h	|	hexadecimal	|
|	\s	|	whitespace character (alternately, byte)	|
|	\w	|	word (alpha or digit) character (alternately, byte)	|
|	\177	|	match the specified octal character	|
|	\xXX	|	match the specified hexadecimal (0-9a-fA-F)	|
|	\x{1-2}	|	match the specified hexadecimal (0-9a-fA-F)	|
|	\u12345	|	match the specified 1-5 digit Unicode character (only if compiled for Unicode support)	|
|	\u{1-5}	|	match the specified 1-5 digit Unicode character (only if compiled for Unicode support)	|
|	[a-z]	|	Normal character (alternately, byte) class (a literal hyphen may occur at start)	|
|	[^a-z]	|	Negated character (alternately, byte) class. Characters may include the \escapes listed above	|
|	`	|	Use the byte alternative (not UTF-8) for the following atom	|
|	! @ # % _ ; <	|	Call the extended_match function, which defaults to just match that character	|
|	control-character	|	Call the extended_match function	|
|	:name:	|	Capture the text matched by the previous atom to the named variable (postfix!)	|

Note: alternates and repetition are possessive, they will never backtrack.
Once an alternate has matched, no subsequent alterative will be tried in that group.
Once a repetition has been made, it will never be unwound.
It is your responsibility to ensure these possessive operators never match unless it's final.
You should use assertions to control inappropriate greed.

All template parameters may be omitted to use defaults:
<pre>
template&lt;typename DataPtr = const UTF8*, typename _Char = UCS4&gt; class PegexpPointerSource;
template&lt;typename Source;gt; class PegexpDefaultMatch;
template&lt;typename Match&gt; class PegexpDefaultContext;
template&lt;typename Context&gt; class Pegexp;
</pre>

A Context may accumulate data on Captures and failure locations.
It should have a nested Context::Match type.
A PegexpDefaultContext is provided to demonstrate the minimum API. Read the code.

A Match reports details on success or failure.
Match should have a nested Match::Source type.
A PegexpDefaultMatch is provided to demonstrate the minimum API. Read the code.

A Source provides a location in a stream of data, and can read data, but forwards only.
Source should have a nested Source::Char type. You can use any scalar data type for Char (often char but defaults to UCS4).
A PegexpPointerSource class	is provided to demonstrate the minimum API, with some extra features added.

The structure of Source is designed to be able to process data that's arriving on an ephemeral stream,
such as a network socket.  The only extra processing required is that when any copy of a Source is made,
it must be possible to proceed from that position in the stream. When a Source is deleted,
no further access will be required to data from that position - unless an older copy still exists.
This "stream memory" behaviour is being implemented in a <strong>StreamFork</strong> class.

You can subclass Pegexp\<\> to override match_extended&skip_extended to handle special command characters.

Read the header file for more details.

## PEG parsing

`#include	<peg.h>`

<pre>
template&lt;typename Source&gt; class PegDefaultMatch;
template&lt;typename Match&gt; class PegContextNoCapture;
template&lt;typename Source, typename Match, typename Context&gt; class Peg;
</pre>

The Peg parser templates make use of Pegexp to provide a powerful PEG parsing engine for arbitrary grammars.
No memoization is performed, so a badly constructed grammar can cause long runtimes.
Peg uses a new Context for each nested call to a Pegexp.
The default Context detects left recursion and reports failure.

The grammar is expressed using a compact in-memory table. No executable code needs to be generated.
No heap memory allocation is required during execution (and minimal stack),
unless you use a Context that saves text captures, builds Abstract Syntax Trees, or records failure tokens and locations.

The preferred way to use this is to compile a grammar expressed in the BNF-like language [Px](grammars/px.px),
which (will) emit C++ data definitions for the parser engine to interpret.

Like the Pegexp template, Peg\<\> processes data from a Source, which may be a stream.

You should define a Context which implements whatever result capture you require.
Context should have a nested Context::Rule type, which can be refined where necessary.

The [Peg parser](test/peg_test.cpp) for [Px](grammars/px.px) works and shows how to generate an AST from captures.
There is not a code generator for Px yet.

## COWMap

The Copy-on-write Map template uses a C++ STL map template class (sorted red-black trees)
under a reference-counted zero side-effect implementation. Any modification to a
map via a reference will first copy the map if there are any other references.
Looking up an entry in a map returns a *copy* of the entry, so it is necessary
to explicitly put a modified entry back into the map.

The COWMap template is functional but rudimentary, still under development.

## Variant Data Type

The Variant class offers a type-safe way to manage numeric and StrVal types in a compact union,
along with Array<StrVal>, Array<Variant>, and a StrVal-keyed COWMap to Variant.
This allows building arbitrary data structures, which support asJSON() to render the whole structure as a string.

## Regular Expressions

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

## LICENSE

The MIT License. See the LICENSE file for details.
