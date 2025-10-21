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
