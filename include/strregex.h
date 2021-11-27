#if !defined(STRREGEX_H)
#define STRREGEX_H
/*
 * Unicode String regular expressions
 */
#include	<strpp.h>

enum RxFeature
{
	NoFeature	= 0x0000000,
	// Kinds of characters or classes:
	// AnyChar	= 0x0000000,	// . means any char, default
	CEscapes	= 0x0000001,	// \0 \b \e \f \n \t \r
	Shorthand	= 0x0000002,	// \s, later \d \w
	OctalChar	= 0x0000004,	// \177 (1-3 digits)
	HexChar		= 0x0000008,	// \xNN	(1-2 hex digits)
	UnicodeChar	= 0x0000010,	// \uNNNNN (1-5 hex digits)
	PropertyChars	= 0x0000020,	// \p{Property_Name}, see https://unicode.org/reports/tr18/
	CharClasses	= 0x0000040,	// [...], [^...]
//	PosixClasses	= 0x0000080,	// [:digit:], [=e=], etc, in classes
	// Kinds of multiplicity:
	ZeroOrOneQuest	= 0x0000100,	// ?, zero or one
	ZeroOrMore	= 0x0000200,	// *, zero or more
	OneOrMore	= 0x0000400,	// +, one or more
	CountRepetition	= 0x0000800,	// {n, m}
	// Groups of regular expressions:
	Alternates	= 0x0001000,	// re1|re2
	Group		= 0x0002000,	// (re)
	Capture		= 0x0004000,	// (?<group_name>re)
	NonCapture	= 0x0008000,	// (?:re)
	NegLookahead	= 0x0010000,	// (?!re)
	Subroutine	= 0x0020000,	// (?&group_name)
	// Line start/end
	BOL		= 0x0040000,	// ^ beginning of line
	EOL		= 0x0080000,	// $ end of line
	AllFeatures	= 0x00FFFFF,
	// Options relevant to regexp interpretation
	AnyIsQuest	= 0x01000000,	// ? means any char (so cannot be ZeroOrOneQuest)
	ZeroOrMoreAny	= 0x02000000,	// * means zero or more any, aka .*
	AnyIncludesNL	= 0x04000000,	// Any character includes newline
	CaseInsensitive	= 0x08000000,	// Perform case-insensitive match
//	Digraphs	= 0x10000000,	// e.g. ll in Spanish, ss in German, ch in Czech
	ExtendedRE	= 0x20000000,	// Whitespace allowed, but may be matched by \s
};

class RxCompiled;
class RxMatcher
{
protected:
	~RxMatcher();
	RxMatcher(RxCompiled& rx);
public:
private:
};

// Regular expression instructions
enum class RxOp: unsigned char
{
	RxoNull = 0,		// Null termination on the NFA
	RxoStart,		// Place to start the DFA
	RxoLiteral,		// A string of specific characters
	RxoCharProperty,	// A character with a named Unicode property
	RxoBOL,			// Beginning of Line
	RxoEOL,			// End of Line
	RxoCharClass,		// Character class
	RxoNegCharClass,	// Negated Character class
	RxoAny,			// Any single char
	RxoRepetition,		// {n, m}
	RxoNonCapturingGroup,	// (...)
	RxoNamedCapture,	// (?<name>...)
	RxoNegLookahead,	// (?!...)
	RxoAlternate,		// |
	RxoSubroutine,		// Subroutine call to a named group
	RxoEndGroup,		// End of a group
	RxoEnd			// Termination condition
};

struct RxRepetitionRange
{
	RxRepetitionRange() : min(0), max(0) {}
	RxRepetitionRange(int n, int x) : min(n), max(x) {}
	uint16_t	min;
	uint16_t	max;
};

class RxInstruction
{
public:
	RxOp		op;		// The instruction code
	RxRepetitionRange repetition;	// How many repetitions?
	StrVal		str;		// RxoNamedCapture, RxoSubroutine, RxoCharClass, RxoNegCharClass

	RxInstruction(RxOp _op) : op(_op) { }
	RxInstruction(RxOp _op, StrVal _str) : op(_op), str(_str) { }
	RxInstruction(RxOp _op, int min, int max) : op(_op), repetition(min, max) { }
};

/*
 * RxCompiled compiles a regular expressions for the machine to execute.
 */
class RxCompiled
{
public:
	~RxCompiled();
	RxCompiled(StrVal re, RxFeature features = AllFeatures, RxFeature reject_features = NoFeature);

	// Lexical scanner and compiler for a regular expression. Returns false if error_message gets set.
	bool		scan_rx(const std::function<bool(const RxInstruction& instr)> func);
	bool		compile();

	void		dump();			// Dump binary code to stdout

	const char*	ErrorMessage() const { return error_message; }

protected:
	StrVal		re;
	RxFeature	features_enabled;	// Features that are not enabled are normally ignored
	RxFeature	features_rejected;	// but these features cause an error if used
	const char*	error_message;
	UTF8*		nfa;
	CharBytes	nfa_size;

	bool		supported(RxFeature);	// Set error message and return false on rejected feature use
	bool		enabled(RxFeature) const; // Return true if the specified feature is enabled
};

/*
 * RxEngine is the machine that evaluates a Regular Expression
 * against a string, and contains its result.
 */
class RxEngine
{
public:
	~RxEngine();
	RxEngine(const RxCompiled* machine, StrVal target);

private:
};

#endif	// STRREGEX_H
