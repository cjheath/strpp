#if !defined(STRREGEX_H)
#define STRREGEX_H
/*
 * Unicode String regular expressions
 */
#include	<strpp.h>
#include	<vector>

/*
 * Configurable features. These change the behaviour of the compiler, not the matcher.
 */
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

// Regular expression instructions
enum class RxOp: unsigned char
{
	RxoNull = 0,		// Null termination on the NFA
	RxoLiteral = 1,		// A string of specific characters
	RxoNonCapturingGroup = 2,	// (...)
	RxoNamedCapture = 3,	// (?<name>...)
	RxoAlternate = 4,	// |
	RxoEndGroup = 5,	// End of a group
	RxoRepetition = 6,	// {n, m}

	// The following opcodes are emitted in the NFA. Use ASCII to make it a bit readable
	RxoStart = 'S',		// Place to start the DFA
	RxoCharProperty = 'P',	// A character with a named Unicode property
	RxoBOL = '^',		// Beginning of Line
	RxoEOL = '$',		// End of Line
	RxoCharClass = 'L',	// Character class
	RxoNegCharClass = 'N',	// Negated Character class
	RxoAny = '.',		// Any single char
	RxoNegLookahead = '!',	// (?!...)
	RxoSubroutineCall = 'U', // Subroutine call to a named group
	RxoAccept = '#',	// Termination condition
	// NFA Instructions, not from the lexical scan
	RxoChar = 'C',		// A single literal character
	RxoJump = 'J',		// Continue from a different offset in the NFA
	RxoSplit = 'A',		// Continue with two threads at different offsets in the NFA
	RxoZero = 'Z',		// Zero a counter and continue from specified offset in the NFA
	RxoCount = 'R',		// Increment a counter. Compare the value with min and max parameters,
				// and continue one or two threads at the specified offsets
	RxoCaptureStart = '(',	// Save start
	RxoCaptureEnd = ')',	// Save end
};

#define	RxMaxLiteral		100	// Long literals in the NFA will be split after this many bytes
#define	RxMaxNesting		16	// Maximum nesting depth for groups

struct RxRepetitionRange
{
	uint16_t	min;
	uint16_t	max;	// Zero means no maximum
};

class RxStatement
{
public:
	RxOp		op;		// The instruction code
	RxRepetitionRange repetition;	// How many repetitions?
	StrVal		str;		// RxoNamedCapture, RxoSubroutineCall, RxoCharClass, RxoNegCharClass

	RxStatement(RxOp _op) : op(_op) { }
	RxStatement(RxOp _op, StrVal _str) : op(_op), str(_str) { }
	RxStatement(RxOp _op, int min, int max) : op(_op) { repetition.min = min; repetition.max = max; }
};

/*
 * RxCompiler compiles a regular expression for the matcher to execute.
 */
class RxCompiler
{
public:
	~RxCompiler();
	RxCompiler(StrVal re, RxFeature features = AllFeatures, RxFeature reject_features = NoFeature);

	// Lexical scanner and compiler for a regular expression. Returns false if error_message gets set.
	bool		scan_rx(const std::function<bool(const RxStatement& instr)> func);
	bool		compile(char*& nfa);

	void		dump(const char* nfa) const;	// Dump NFA to stdout
	void		hexdump(const char* nfa) const;	// Dump binary code to stdout
	static bool	instr_dump(const char* nfa, const char*& np);	// Disassemble symbolic code to stdout

	const char*	ErrorMessage() const { return error_message; }

protected:
	StrVal		re;
	RxFeature	features_enabled;	// Features that are not enabled are normally ignored
	RxFeature	features_rejected;	// but these features cause an error if used
	const char*	error_message;
	CharBytes	nfa_size;

	bool		supported(RxFeature);	// Set error message and return false on rejected feature use
	bool		enabled(RxFeature) const; // Return true if the specified feature is enabled
};

typedef	CharNum	RxStationID;	// A station in the NFA (as a byte offset from the start)
struct		RxDecoded;	// A decoded NFA instruction
class		RxResult;	// The result of a regex match

/*
 * RxProgram contains a compiled Regular Expression. Contains the decoded NFA header.
 * Does not get modified after construction, so can run matches in multiple threads simultaneously.
 */
class RxProgram
{
public:
	~RxProgram();
	RxProgram(const char* nfa, bool take_ownership = false);		// REVISIT: Add option flags? Like no-capture, etc?

	const RxResult	match_after(StrVal target, CharNum offset = 0) const;
	const RxResult	match_at(StrVal target, CharNum offset = 0) const;

	RxStationID	startStation() const { return start_station; }
	RxStationID	searchStation() const { return search_station; }
	int		maxStation() const { return max_station; }
	int		maxCounter() const { return max_counter; }
	int		maxCapture() const { return max_capture; }
	void		decode(RxStationID, RxDecoded&) const;

private:
	// The NFA, and whether we should delete the data:
	bool		nfa_owned;	// This program will delete the nfa
	const char*	nfa;

	RxStationID	start_station;	// Start point for just the regex
	RxStationID	search_station;	// Start point for the regex preceeded by .*
	int		max_station;	// Maximum number of concurrent threads, from NFA header
	short		max_counter;	// Number of counters required, from NFA header
	short		max_capture;	// Number of capture expressions in this NFA

	// Names of the capture groups or subroutines, extracted from the NFA header
	std::vector<StrVal>	names;
};

class RxResultBody;	// RefCounted shared result values

/*
 * The result from regex matching:
 * Counter stack, captures, function-call results.
 */
class RxResult
{
public:
	~RxResult();
	RxResult();
	RxResult(const RxProgram& program);
	RxResult(const RxResult& s1);
	RxResult& operator=(const RxResult& s1);
	void		clear();

	bool		succeeded() { return (RxResultBody*)body != 0; }
	operator	bool() { return succeeded(); }
	CharNum		offset() const { return capture(0); }
	CharNum		length() const { return capture(1); }

	// Capture and counter access outside the 0..index range is ignored.
	// This makes it possible to match a Regex without capturing all results.
	// Even numbers are the capture start, odd numbers are the end
	CharNum		capture(int index) const;

	// Mutation API, used during matching
	RxResult&	capture_set(int index, CharNum val);
	void		counter_push_zero();	// Push a zero counter
	CharNum		counter_incr();		// Increment and return top counter of stack
	void		counter_pop();		// Discard the top counter of the stack

	// Something to handle function-call results:
	// mumble, mumble...
	// std::vector<RxResult> subroutineMatches;	// Ordered by position of the subroutine call in the regexp

private:
	Ref<RxResultBody> body;
	void		Unshare();
};

#endif	// STRREGEX_H
