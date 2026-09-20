## Prefix Regular Expression pattern matching

`#include	<pegexp.h>`

This template class implements Prefix Regular Expressions (aka
<strong>pegular expressions</strong> or pegexp).

Regular expressions describe text patterns, backtracking after a failure
caused by greedy repetition.

Pegular expressions also match text, but never backtrack, which requires
avoiding excessive repetition using look-aheads. Repetition and optional
operators precede the atom they affect (hence <strong>prefix</strong>).
This prefix notation (unlike regexp, BNF and normal PEG grammars) allows an
efficient interpreter without needing a compiler, which reduces memory
demand.

The advantage of Pegexp over Regexp is there is no easy ReDoS attack, and no
computational or memory expense to avoiding that.

|	Atom	|	Matches	|
| --- | --- |
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
|	\L	|	lowercase character	|
|	\U	|	uppercase character	|
|	\w	|	word (alpha or digit) character (alternately, byte)	|
|	\177	|	match the specified octal character	|
|	\xXX	|	match the specified hexadecimal (0-9a-fA-F)	|
|	\x{1-2}	|	match the specified hexadecimal (0-9a-fA-F)	|
|	\u1234	|	match the specified 1-4 digit Unicode character (only if compiled for Unicode support)	|
|	\u{1-8}	|	match the specified 1-8 digit Unicode character (only if compiled for Unicode support)	|
|	[a-z]	|	Normal character (alternately, byte) class (a literal hyphen may occur at start)	|
|	[^a-z]	|	Negated character (alternately, byte) class. Characters may include the \escapes listed above	|
|	~ @ # % _ ; <	|	Call the extended_match function, which defaults to just match that character	|
|	control-character	|	Call the extended_match function	|
|	:name:	|	Capture the text matched by the previous atom to the named variable (postfix!)	|

Note: alternates and repetition are possessive, they will never backtrack.
Once an alternate has matched, no subsequent alternative will be tried in
that group. Once a repetition has been made, it will never be unwound. It
is your responsibility to ensure these possessive operators never match
unless it's final. You should use negative assertions to control
inappropriate greed.

All template parameters may be omitted to use defaults:

	template<typename DataPtr = const UTF8*, typename _Char = UCS4>
	class PegexpPointerSource;
	using PegexpDefaultSource = PegexpPointerSource<>;

	template<typename Source = PegexpDefaultSource>
	class PegexpState;

	template<typename State = PegexpState<>>
	class PegexpDefaultMatch;

	template<typename Match = PegexpDefaultMatch<>>
	class PegexpDefaultContext;

	template<typename Context = PegexpDefaultContext<>>
	class Pegexp;

A Context may accumulate data on Captures and failure locations. It should
have a nested Context::Match type. A PegexpDefaultContext is provided to
demonstrate the minimum API.

A Match reports details on success or failure. Match should have a nested
Match::Source type. A PegexpDefaultMatch is provided to demonstrate the
minimum API.

A Source provides a location in a stream of data, and can read data, but
forwards only. Source should have a nested Source::Char type. You can use
any scalar data type for Char (often char, but defaults to UCS4). A
PegexpPointerSource class is provided to demonstrate the minimum API, with
some extra features added.

The structure of Source is designed to be able to process data that's
arriving on an ephemeral stream, such as a network socket. The only extra
processing required is that when any copy of a Source is made, it must be
possible to proceed from that position in the stream. When a Source is
deleted, no further access will be required to data from that position
unless an older copy still exists.

### Public methods

Defined in [pegexp.h](https://github.com/cjheath/strpp/blob/main/include/pegexp.h).

`Pegexp`:

- `Pegexp(PatternP pattern)` - a matcher over the 8-bit pattern text.
- `match(Source& source, Context* context)` - match at or after the source's
  position, advance it to the end of the match, and answer the Match.
- `match_here(Source& source, Context* context)` - match at exactly that
  position, without scanning forward for somewhere the pattern does fit.
- `pattern` - the pattern text it was built from.
- `special` - the characters that must be backslash-escaped to be literal.

A subclass can override `match_extended` and `skip_extended` to give a meaning
of its own to the extension characters listed in the table above.

`PegexpPointerSource`, the Source to model yours on:

- `PegexpPointerSource(const DataPtr cp)` - a Source at the start of the data.
- `is_null()` - whether there is no data at all, which is how a null Source is
  told from one that has run out.
- `get_byte()`, `get_char()` - the next byte, or the next character as a UCS4,
  moving forward.
- `at_eof()`, `at_bol()` - whether the data has run out, and whether this is
  the beginning of a line.
- `same(other)` - whether another copy is at the same position.
- `bytes_from(origin)` - the bytes between two positions.
- `current_byte()`, `current_line()`, `current_column()` - where it is now, for
  error reporting.
- `string_to(other)` - the text between two positions, as a StrVal.

`PegexpDefaultContext`, the Context to model yours on:

- `capture(name, name_len, match, in_repetition)` - called for a labelled
  atom, and answers the capture count afterwards. The default forgets it.
- `capture_count()`, `rollback_capture(count)` - number the captures, and give
  the recent ones back when a path fails.
- `record_failure(op, op_end, location)` - called for an atom that did not
  match, with the place it was tried.
- `match_result(from, to)`, `match_failure(at)` - how a Context declares its
  answers.
- `capture_disabled`, `repetition_nesting` - how deep inside a look-ahead, and
  inside a repetition, the match currently is.

`PegexpState` is the pattern position and the Source position together, and
`PegexpDefaultMatch` is the pair of States a Match runs between.
