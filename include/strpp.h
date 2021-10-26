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
#include	<refcount.h>
#include	<utf8.h>

// These typedefs can be changed to uint16_t, which reduces storage and maximum string length to 65534
typedef	uint32_t	CharNum;	// Character position (offset) within a string
typedef	uint32_t	CharBytes;	// Byte position (offset) within a string

class	StrBody;

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

	StrVal(const UTF8* data);	// construct by copying null-terminated UTF8 data
	StrVal(const UTF8* data, CharBytes length, size_t allocate = 0); // construct from length-terminated UTF8 data
	StrVal(UCS4 character);		// construct from single-character string

	// Return a reference to a static C string, which must persist and will never be changed or deleted:
	static	StrVal	Static(const UTF8* data);	// Avoids using the allocator for a string constant

	CharBytes	numBytes() const { return nthChar(num_chars)-nthChar(0); }
	CharNum		length() const { return num_chars; }	// Number of chars
	bool		isEmpty() const { return length() == 0; } // equals empty string?

	// Access characters and UTF8 value:
	UCS4		operator[](int charNum) const;
	const UTF8*	asUTF8();	// Null terminated. Must unshare data if it's a substring with elided suffix
	const UTF8*	asUTF8(CharBytes& bytes) const;	// Returns bytes, but doesn't guarantee null termination

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

#if 0
	int32_t		asInt32(
			//	ErrorT* err_return = 0, // error return
				int	radix = 0,	// base for conversion
				CharNum* scanned = 0	// characters scanned
			) const;

	int64_t		asInt64(
			//	ErrorT* err_return = 0, // error return
				int	radix = 0,	// base for conversion
				CharNum* scanned = 0	// characters scanned
			) const;

	StrVal		asLower() const;
	StrVal		asUpper() const;
	void		toLower();
	void		toUpper();

	StrVal		substitute(
				StrVal		from_str,
				StrVal		to_str,
				bool		do_all		= true,
				int		after		= -1,
				bool		ignore_case	= false
			) const;

	// static StrVal	format(StrVal fmt, const ArgListC);	// Like printf, but type-safe
#endif

	struct Bookmark
	{
		Bookmark() : char_num(0), byte_num(0) {}	// 0, 0 is always a valid Bookmark
		Bookmark(CharNum c, CharBytes b) : char_num(c), byte_num(b) {}
		CharNum		char_num;
		CharBytes	byte_num;
	};

protected:
	StrVal(StrBody* s1);		// New reference to same string body
	StrVal(StrBody* body, CharNum offs, CharNum len);	// Not bounds-checked!
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
			StrVal(StrBody&);
	static	StrBody	nullBody;
};

class	StrBody
:	public RefCounted
{
public:
	~StrBody();
	StrBody();				// Null string constructor
	StrBody(const UTF8* data);
	StrBody(const UTF8* data, CharBytes length, size_t allocate = 0);
	static StrBody*	Static(const UTF8* data); // reference a static C string, which will never change or be deleted

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

protected:
	UTF8*		start;
	CharNum		num_chars;	// Number of characters, if they have been counted yet
	CharBytes	num_bytes;	// How many bytes of data
	CharBytes	num_alloc;	// How many bytes are allocated

	void		countChars();

	StrBody(StrBody&) { }		// Never copy a body
};
