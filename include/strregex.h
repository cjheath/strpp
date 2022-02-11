#if !defined(STRREGEX_H)
#define STRREGEX_H
/*
 * Unicode Strings
 * Regular expression configuration, compiler, matcher and results
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<strpp.h>
#include	<vector>

/*
 * Configurable features. Normal things are default, but you can either
 * disable, or detect and reject specific features in the compiler.
 * This allows you to configure a custom regular expression language.
 */
enum class RxFeature
{
	NoFeature	= 0x0000000,
	// Kinds of characters or classes:
	CEscapes	= 0x0000001,	// \0 \b \e \f \n \t \r
	Shorthand	= 0x0000002,	// \s, \d, \h (whitespace, decimal, hexadecimal)
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

// Regular expression instruction tokens and opcodes
enum class RxOp: char
{
	// These opcodes are lexical tokens only
	RxoNull = 0,		// Null termination on the NFA
	RxoLiteral = 1,		// A string of specific characters
	RxoNonCapturingGroup = 2,	// (...)
	RxoNamedCapture = 3,	// (?<name>...)
	RxoAlternate = 4,	// |
	RxoEndGroup = 5,	// End of a group
	RxoRepetition = 6,	// {n, m}

	// The following tokens are emitted in the NFA. Use ASCII to make it a bit readable
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
	// These opcodes are NFA Instructions, not lexical tokens
	RxoChar = 'C',		// A single literal character
	RxoJump = 'J',		// Continue from a different offset in the NFA
	RxoSplit = 'A',		// Continue with two threads at different offsets in the NFA
	RxoZero = 'Z',		// Zero a counter and continue from specified offset in the NFA
	RxoCount = 'R',		// Increment a counter. Compare the value with min and max parameters,
				// and continue one or two threads at the specified offsets
	RxoCaptureStart = '(',	// Save start
	RxoCaptureEnd = ')',	// Save end
};

#define	RxMaxNesting		12	// Maximum nesting depth for groups

struct RxRepetitionRange
{
	uint16_t	min;
	uint16_t	max;	// Zero means no maximum
};

class	RxToken;	// Passes information between the lexer and the parser

/*
 * RxCompiler compiles a regular expression for the matcher to execute.
 * The same RxCompiler can be used multiple times.
 */
class RxCompiler
{
public:
	~RxCompiler();
	RxCompiler(StrVal re, RxFeature features = RxFeature::AllFeatures, RxFeature reject_features = RxFeature::NoFeature);

	// Lexical scanner and compiler for a regular expression. Returns false if error_message gets set.
	// The compiler allocates memory for the nfa, which you should delete using "delete[] nfa".
	bool		scanRegex(const std::function<bool(const RxToken& instr)> func);
	bool		compile(char*& nfa);

	const char*	errorMessage() const { return error_message; }
	int		errorOffset() const { return error_offset; }

	void		dump(const char* nfa) const;	// Dump NFA to stdout
	void		dumpHex(const char* nfa) const;	// Dump binary code to stdout
	static bool	dumpInstruction(const char* nfa, const char*& np);	// Disassemble symbolic code to stdout

protected:
	StrVal		re;
	RxFeature	features_enabled;	// Features that are not enabled are normally ignored
	RxFeature	features_rejected;	// but these features cause an error if used
	const char*	error_message;		// An error from compiling
	int		error_offset;
	CharBytes	nfa_size;		// Number of bytes allocated. It is likely that not all are used

	bool		supported(RxFeature);	// Set error message and return false on rejected feature use
	bool		enabled(RxFeature) const; // Return true if the specified feature is enabled
};

typedef	CharNum	RxStationID;	// A station in the NFA (as a byte offset from the start)
struct		RxDecoded;	// A decoded NFA instruction
class		RxResult;	// The result of a regex match

/*
 * RxProgram wraps a compiled Regular Expression and decodes the header, and starts matchers.
 * Does not get modified after construction, so can run matches in multiple threads simultaneously.
 */
class RxProgram
{
public:
	~RxProgram();
	RxProgram(const char* nfa, bool take_ownership = false);

	// REVISIT: Add option flags? Like no-capture, case insensitive, etc?
	const RxResult	matchAfter(StrVal target, CharNum offset = 0) const;
	const RxResult	matchAt(StrVal target, CharNum offset = 0) const;

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

class	RxCaptures;	// RefCounted shared capture and counter values
class	RxMatch;

/*
 * The result from regex matching: captures, function-call results.
 * During matching, it also contains the counter stack (count&offset) for each nested repetition level.
 */
class RxResult
{
public:
	struct	Counter {
		CharNum		count;		// The number of repetitions seen so far
		CharNum		offset;		// The text offset at the start of the last repetition
	};
	~RxResult();
	RxResult();				// Construct a non-Result (failure)
	RxResult(short counter_max, short capture_max);
	RxResult(const RxResult& s1);
	RxResult& operator=(const RxResult& s1);
	void		clear();

	bool		succeeded() const;
	operator	bool() { return succeeded(); }

	CharNum		offset() const { return cap0; }
	CharNum		length() const { return succeeded() ? cap1-cap0 : 0; }

	// Capture and counter access outside the 0..index range is ignored.
	// This makes it possible to match a Regex without capturing all results.
	// Even numbers are the capture start, odd numbers are the end
	int		captureMax() const;
	CharNum		capture(int index) const;

	// Mutation API, used during matching
	RxResult&	captureSet(int index, CharNum val);
	int		counterNum() const;	// How many counters are in use?
	bool		hasCounter() const { return counterNum() > 0; }
	bool		countersSame(RxResult& other) const;
	int		counterGet(int = 0) const;		// get the nth top counter
	void		counterPushZero(CharNum offset);	// Push a zero counter at this offset
	CharNum		counterIncr(CharNum offset);		// Increment and return top counter of stack
	void		counterPop();		// Discard the top counter of the stack
	const Counter	counterTop();		// Top counter value, if any

	// Something to handle function-call results: mumble, mumble...
	// std::vector<RxResult> subroutineMatches;	// Ordered by position of the subroutine call in the regexp

        int		resultNumber() const;	// Only used for diagnostics

private:
	CharNum		cap0, cap1;		// Start and end of match
	Ref<RxCaptures> captures;
	void		Unshare();
};

#endif	// STRREGEX_H
