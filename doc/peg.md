## PEG parsing

`#include	<peg.h>`

<pre>template&lt;typename Source&gt; class PegDefaultMatch;
template&lt;typename Match&gt; class PegContextNoCapture;
template&lt;typename Source, typename Match, typename Context&gt; class Peg;
</pre>

The Peg parser templates make use of Pegexp to provide a powerful PEG
parsing engine for arbitrary grammars. No memoization is performed, so a
badly constructed grammar can cause long runtimes. Peg uses a new Context
for each nested call to a Pegexp. The default Context detects left recursion
and reports failure.

The grammar is expressed using a compact in-memory table. No executable code
needs to be generated. No heap memory allocation is required during
execution (and minimal stack), unless you use a Context that saves text
captures, builds Abstract Syntax Trees, or records failure tokens and
locations.

The preferred way to use this is to compile a grammar expressed in the
BNFlike language [Px](https://github.com/cjheath/px), which (will) emit C++
data definitions for the parser engine to interpret.

Like the Pegexp template, Peg\<\> processes data from a Source, which may be
a stream.

You should define a Context which implements whatever result capture you
require. Context should have a nested Context::Rule type, which can be
refined where necessary.

The [Peg parser](https://github.com/cjheath/strpp/blob/main/test/peg_test.cpp)
for [Px](https://github.com/cjheath/px) shows how to generate an AST from
captures.

### Public methods

`Peg` and the no-capture Context it comes with are defined in
[peg.h](https://github.com/cjheath/strpp/blob/main/include/peg.h):

- `Peg(Rule* rules, int num_rule)` - a parser over an array of rules, sorted
  by name in place so that a binary search finds them.
- `parse(Source& source)` - run the rule named `TOP` over the source, and
  answer the Match.
- `lookup(const char* name)` - the rule of that name. A shortened name ended
  by `>` or by the end of the string is allowed.
- `recurse(Rule* sub_rule, State& state, Context* context)` - match a sub-rule
  at the current position, or fail when it would be left recursion.
- `PegRuleNoCapture(const char* name, PegexpT pegexp)` - one named rule,
  holding the Pegexp it matches.
- `PegPegexp` - the Pegexp subclass that gives `<rule>` its meaning, through
  `match_extended` and `skip_extended`.

A Context must provide these, and `PegContextNoCapture` is the default that
implements them all as no-ops:

- `capture(name, name_len, match, in_repetition)` - called for a labelled
  atom, and answers the capture count afterwards.
- `capture_count()`, `rollback_capture(count)` - number the captures, and give
  the recent ones back when a path fails.
- `record_failure(op, op_end, location)` - called for an atom that did not
  match, with the place it was tried.
- `match_result(from, to)`, `match_failure(at)` - how the Context declares its
  answers.
- `capture_disabled`, `repetition_nesting` - how deep inside a look-ahead, and
  inside a repetition, the match currently is.

The Context that builds an AST is in
[peg_ast.h](https://github.com/cjheath/strpp/blob/main/include/peg_ast.h):

- `PegContext::capture(...)` - records the captured text into the AST, growing
  an array where a label repeats, or where the capture was inside a
  repetition.
- `PegContext::record_failure(...)` - keeps the atoms tried at the furthest
  point reached, which is what a parse error is reported from.
- `PegMatch::var` - the AST the parse produced.
- `PegMatch::is_failure()` - whether the parse failed.
- `PegMatch::furthermost_success`, `PegMatch::failures` - where the parse got
  to, and what it wanted there. Both are filled in only by the outermost
  parse.
- `PegCaptureRule(name, pegexp, captures)` - a rule that collects the labels
  in a zero-terminated list of names.
- `PegMemorySource(const char* cp)` - the in-memory Source these use, whose
  `peek()` answers the raw pointer at the current position.
