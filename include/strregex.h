#if !defined(STRREGEX_H)
#define STRREGEX_H
/*
 * Unicode String regular expressions
 */
#include	<strpp.h>

/*
 * RxCompiled compiles a regular expressions for the machine to execute.
 */
class RxCompiled
{
protected:
	enum class RxOp: unsigned char;

public:
	enum RxFeature {
		NoFeature	= 0x0000000,
		// Kinds of characters or classes:
		// AnyChar	= 0x0000000,	// . means any char, default
		CEscapes	= 0x0000001,	// \0 \b \e \f \n \t \r
		Shorthand	= 0x0000002,	// \s, later \d \w
		OctalChar	= 0x0000004,	// \177 (1-3 digits)
		HexChar		= 0x0000008,	// \xNN	(1-2 hex digits)
		UnicodeChar	= 0x0000010,	// \uNNNNN (1-5 hex digits)
		PropertyChars	= 0x0000020,	// \p{Property_Name}, see https://unicode.org/reports/tr18/
//		CharClass	= 0x0000040,	// [...], [^...]
//		PosixClasses	= 0x0000080,	// [:digit:], [=e=], etc, in classes
		// Kinds of multiplicity:
		ZeroOrMore	= 0x0000100,	// *, zero or more
		OneOrMore	= 0x0000200,	// +, one or more
		ZeroOrOneQuest	= 0x0000400,	// ?, zero or one
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
//		Digraphs	= 0x10000000,	// e.g. ll in Spanish, ss in German, ch in Czech
		ExtendedRE	= 0x20000000,	// Whitespace allowed, but may be matched by \s
	};
	~RxCompiled();
	RxCompiled(StrVal re, RxFeature reject_features = NoFeature, RxFeature ignore_features = NoFeature);

	// Lexical scanner for a regular expression. Returns false if error_message gets set.
	bool		scan_rx(bool (*func)(RxOp op, StrVal param));

	const char*	ErrorMessage() const { return error_message; }

protected:
	StrVal		re;
	RxFeature	features_enabled;	// Features that are not enabled are normally ignored
	RxFeature	features_rejected;	// but these features cause an error if used
	const char*	error_message;

	// Regular expression opcodes
	enum class RxOp: unsigned char {
		RxoStart,		// Place to start the DFA
		RxoChar,		// A specific char
		RxoCharProperty,	// A char with a named Unicode property
		RxoBOL,			// Beginning of Line
		RxoEOL,			// End of Line
		RxoCharClass,		// Character class
		RxoNegCharClass,	// Negated Character class
		RxoAny,			// Any single char
		RxoZeroOrOne,		// ?
		RxoZeroOrMore,		// *
		RxoOneOrMore,		// +
		RxoCountedRepetition,	// {n, m}
		RxoNonCapturingGroup,	// (...)
		RxoNamedCapture,	// (?<name>...)
		RxoNegLookahead,	// (?!...)
		RxoAlternate,		// |
		RxoSubroutine,		// Subroutine call to a named group
		RxoEndGroup,		// End of a group
		RxoEnd			// Termination condition
	};

	struct CharClass {
		// REVISIT: Implement character classes
	};
	class RxInstruction {
	public:
		RxOp		op;
		union {
			StrVal*		str;
			struct {
				uint16_t	min;
				uint16_t	max;
			} repetition;
			CharClass*	cclass;
		};

		RxInstruction(RxOp);
		RxInstruction(RxOp, StrVal);
		RxInstruction(RxOp, int min, int max);
		RxInstruction(RxOp, CharClass*);
		~RxInstruction() {
			// Clean up according to type.
			if (op == RxOp::RxoNamedCapture || op == RxOp::RxoSubroutine)
				delete str;
			else if (op == RxOp::RxoCharClass || op == RxOp::RxoNegCharClass)
				delete[] cclass;
		}
	};

	bool		supported(RxFeature);	// Set error message and return false on rejected feature use
	bool		enabled(RxFeature);	// Return true if the specified feature is enabled
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
