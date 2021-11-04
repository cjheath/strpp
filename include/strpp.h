#if !defined(STRPP_H)
#define STRPP_H
/*
 * String Value library.
 * - By-value semantics with mutation
 * - Thread-safe content sharing and garbage collection using atomic reference counting
 * - Substring support using "slices" (substrings using shared content)
 * - Unicode support using UTF-8
 * - Character indexing, not byte offsets
 * - Efficient forward and backward scanning using bookmarks to assist
 */
#include	<stdlib.h>
#include	<stdint.h>
#include	<functional>

#include	<refcount.h>
#include	<char_encoding.h>

// These typedefs can be changed to uint16_t, which reduces storage and maximum string length to 65534
typedef	uint32_t	CharNum;	// Character position (offset) within a string
typedef	uint32_t	CharBytes;	// Byte position (offset) within a string

class	StrBody;

#define	STRERR_TRAIL_TEXT	1	// There are non-blank characters after the number
#define	STRERR_NO_DIGITS	2	// Number string contains only blank characters
#define	STRERR_NUMBER_OVERFLOW	3	// The number doesn't fit in the requested type
#define	STRERR_NOT_NUMBER	4	// The first non-blank character was non-numeric
#define	STRERR_ILLEGAL_RADIX	5	// Use of an unsupported radic

class	StrVal
{
public:
	typedef enum {
		CompareRaw,		// No processing, just the characters
		CompareCI,		// Case independent
		CompareNatural		// Natural comparison, with numeric strings by value
	} CompareStyle;

	~StrVal() {}			// Destructor
	StrVal();			// Empty string
	StrVal(const StrVal& s1);	// Normal copy constructor
	StrVal& operator=(const StrVal& s1); // Assignment operator
	StrVal(const UTF8* data);	// construct by copying nul-terminated UTF8 data
	StrVal(const UTF8* data, CharBytes length, size_t allocate = 0); // construct from length-terminated UTF8 data
	StrVal(UCS4 character);		// construct from single-character string
	StrVal(StrBody* body);		// New reference to same string body

	CharBytes	numBytes() const { return nthChar(num_chars)-nthChar(0); }
	CharNum		length() const { return num_chars; }	// Number of chars
	bool		isEmpty() const { return length() == 0; } // equals empty string?

	// Access characters and UTF8 value:
	UCS4		operator[](int charNum) const;
	const UTF8*	asUTF8();	// Null terminated. Must unshare data if it's a substring with elided suffix
	const UTF8*	asUTF8(CharBytes& bytes) const;	// Returns bytes, but doesn't guarantee nul termination

	// Comparisons:
	int		compare(const StrVal&, CompareStyle = CompareRaw) const;
	inline bool	operator==(const StrVal& comparand) const {
				return length() == comparand.length() && compare(comparand) == 0;
			}
	inline bool	operator!=(const StrVal& comparand) const { return !(*this == comparand); }
	bool		equalCI(const StrVal& s) const		// Case independent equality
			{ return compare(s, CompareCI) == 0; }

	// Extract substrings:
	StrVal		substr(CharNum at, int len = -1) const;
	StrVal		head(CharNum chars) const
			{ return substr(0, chars); }
	StrVal		tail(CharNum chars) const
			{ return substr(length()-chars, chars); }
	StrVal		shorter(CharNum chars) const	// all chars up to tail
			{ return substr(0, length()-chars); }
	void		remove(CharNum at, int len = -1);	// Delete a substring from the middle

	// Search for a character:
	int		find(UCS4, int after = -1) const;
	int		rfind(UCS4, int before = -1) const;

	// Search for substrings:
	int		find(const StrVal&, int after = -1) const;
	int		rfind(const StrVal&, int before = -1) const;

	// Search for characters in set:
	int		findAny(const StrVal&, int after = -1) const;
	int		rfindAny(const StrVal&, int before = -1) const;

	// Search for characters not in set:
	int		findNot(const StrVal&, int after = -1) const;
	int		rfindNot(const StrVal&, int before = -1) const;

	// Add, producing a new StrVal:
	StrVal		operator+(const StrVal& addend) const;
	StrVal		operator+(UCS4 addend) const;

	// Add, StrVal is modified:
	StrVal&		operator+=(const StrVal& addend);
	StrVal&		operator+=(UCS4 addend);

	void		insert(CharNum pos, const StrVal& addend);
	void		append(const StrVal& addend)
			{ insert(num_chars, addend); }

	StrVal		asLower() const { StrVal lower(*this); lower.toLower(); return lower; }
	StrVal		asUpper() const { StrVal upper(*this); upper.toUpper(); return upper; }
	void		toLower();
	void		toUpper();

	/*
	 * Convert a string to an integer, using radix (0 means use C rules)
         *
         * Leading and trailing spaces are scanned and ignored. Other than
         * that, non-numeric characters or digits after trailing spaces are
         * flagged as an error. 
         *                      
         * The radix value may be 0 or in the range 2-36. Radices beyond 10
         * use the ASCII alphabet for digits above 9, upper or lower case.
         * Radix 2 allows 0b... or 0B..., and radix 16 allows 0x... or
         * 0X..., and radix 0 recognises both forms and uses the appropriate
         * radix. Radix 0 also treats a number with a leading zero as octal.
	 *
	 * @retval 0 no problems
         * @retval STRERR_TRAIL_TEXT There are non-blank characters after the number
         * @retval STRERR_NO_DIGITS Number string contains only blank characters
         * @retval STRERR_NUMBER_OVERFLOW The number doesn't fit in the requested type
         * @retval STRERR_NOT_NUMBER The first non-blank character was non-numeric
 	 */
	int32_t		asInt32(
				int*	err_return = 0, // error return
				int	radix = 0,	// base for conversion
				CharNum* scanned = 0	// characters scanned
			) const;

	struct Bookmark
	{
		Bookmark() : char_num(0), byte_num(0) {}	// 0, 0 is always a valid Bookmark
		Bookmark(CharNum c, CharBytes b) : char_num(c), byte_num(b) {}
		CharNum		char_num;
		CharBytes	byte_num;
	};

protected:
	StrVal(StrBody* body, CharNum offs, CharNum len);	// offs/len not bounds-checked!
	const UTF8*	nthChar(CharNum off) const;	// Return a pointer to the start of the nth character
	const UTF8*	nthChar(CharNum off);	// Return a pointer to the start of the nth character

private:
	Ref<StrBody>	body;		// The storage structure for the character data
	// Support for substrings (slices):
	CharNum		offset;		// Character number of the first character of this substring
	CharNum		num_chars;	// How many characters in this substring
	// Assistance for searching in UTF-8 strings, a bookmark likely to to be near our next access:
	Bookmark	mark;		// Always the byte_num of a given char_num in body

	void		Unshare();	// Get our own copy of StrBody that we can safely mutate
};

class	StrBody
:	public RefCounted
{
public:
	static	StrBody nullBody;

	~StrBody();
	StrBody();				// Null string constructor
	StrBody(const UTF8* data);
	StrBody(const UTF8* data, CharBytes length, size_t allocate);

	/*
	 * This method returns a StrVal for fixed data that will not change until the StrBody is destroyed.
	 * The lifetime of the returned StrVal and all copies must end before the StrBody's does.
	 */
	StrVal		staticStr(const UTF8* static_data, CharNum _c, CharBytes _b);

	UCS4		charAt(CharBytes);	// Return the next UCS4 character or UCS4_NONE
	UCS4		charNext(CharBytes&);	// Return the next UCS4 character or UCS4_NONE, advancing the offset
	UCS4		charB4(CharBytes&);	// Return the preceeding UCS4 character or UCS4_NONE, backing up the offset

	void		resize(size_t);
	inline CharNum	numChars()
	{
		if (num_chars == 0 && num_bytes > 0)
			countChars();
		return num_chars;
	}
	CharBytes	numBytes() const { return num_bytes; }
	void		setLength(CharNum chars, CharBytes len) { num_chars = chars; num_bytes = len; }
	UTF8*		data() const { return start; }
	UTF8*		nthChar(CharNum off, StrVal::Bookmark& bm);	// ptr to start of the nth character
	const UTF8*	endChar() { return start+num_bytes; }

	// Mutating methods. Must only be called when refcount <= 1 (i.e., unshared)
	void		remove(CharNum at, int len = -1);	// Delete a substring from the middle
	void		insert(CharNum pos, const StrVal& addend);
	void		insert(CharNum pos, const StrBody& addend);
	void		toLower();
	void		toUpper();

	/*
	 * At every character position after the given point, the passed transform function
	 * can extract any number of chars (limited by ep) and return a replacement StrVal for those chars.
	 * To leave the remainder untransformed, return without advancing cp
	 * (but a returned StrVal will still be inserted)
	 */
	void		transform(const std::function<StrVal(const UTF8*& cp, const UTF8* ep)> xform, int after = -1);

protected:
	UTF8*		start;
	CharNum		num_chars;	// Number of characters, if they have been counted yet
	CharBytes	num_bytes;	// How many bytes of data
	CharBytes	num_alloc;	// How many bytes are allocated

	void		countChars();

	StrBody(StrBody&) { }		// Never copy a body
};
#endif
