#if !defined(STRVAL_H)
#define STRVAL_H
/*
 * Unicode Strings
 * - By-value semantics with mutation
 * - Thread-safe content sharing and garbage collection using atomic reference counting
 * - Substring support using "slices" (substrings using shared content)
 * - Unicode support using UTF-8
 * - Character indexing, not byte offsets
 * - Efficient forward and backward scanning using bookmarks to assist
 *
 * Not yet:
 * - Unicode normalization (de/composition), see https://en.wikipedia.org/wiki/Unicode_equivalence
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<stdlib.h>
#include	<stdint.h>
#include	<functional>
#include	<type_traits>

#include	<error.h>
#include	<array.h>
#include	<refcount.h>
#include	<char_encoding.h>

#define	StrValIndexBits	16
typedef typename std::conditional<(StrValIndexBits <= 16), uint16_t, uint32_t>::type StrValIndex;

#define	STRERR_SET		1	// Message set number for StrVal
#define	STRERR_TRAIL_TEXT	1	// There are non-blank characters after the number
#define	STRERR_NO_DIGITS	2	// Number string contains only blank characters
#define	STRERR_NUMBER_OVERFLOW	3	// The number doesn't fit in the requested type
#define	STRERR_NOT_NUMBER	4	// The first non-blank character was non-numeric
#define	STRERR_ILLEGAL_RADIX	5	// Use of an unsupported radix

template<typename Index = StrValIndex> class StrValI;
template<typename Index = StrValIndex> struct StrBookmark
{
	StrBookmark() : char_num(0), byte_num(0) {}	// 0, 0 is always a valid Bookmark
	StrBookmark(Index c, Index b) : char_num(c), byte_num(b) {}
	Index		char_num;
	Index		byte_num;
};
template<typename Index = StrValIndex> class StrBodyI;

typedef	StrValI<>	StrVal;
typedef	StrBodyI<>	StrBody;

template<typename Index> class StrBodyI
: public ArrayBody<char, Index>
{
	using Val = StrValI<Index>;
	using Bookmark = StrBookmark<Index>;
	using Body = ArrayBody<char, Index>;
	using Body::num_elements;
	using Body::start;
	using Body::num_alloc;
	using Body::ref_count;

public:
	static	StrBodyI nullBody;

	~StrBodyI()	{}
	StrBodyI()	: num_chars(0) {}
	StrBodyI(const char* data, bool copy, Index length, Index allocate = 0)
			: Body(data, copy, (length == 0 ? strlen(data) : length)+1, allocate), num_chars(0)
			{
				if (copy)
					start[length] = '\0';
			}

	Index		numChars()
			{
				if (num_chars == 0 && num_elements > 0)
					countChars();
				return num_chars;
			}

			// Return a pointer to the start of the nth character
	char*		nthChar(Index char_num, Bookmark& mark)
			{
				UTF8*		up;		// starting pointer for forward search
				int		start_char;	// starting char number for forward search
				UTF8*		ep;		// starting pointer for backward search
				int		end_char;	// starting char number for backward search

				if (char_num < 0 || char_num > (end_char = numChars())) // numChars() counts the string if necessary
					return (UTF8*)0;

				if (num_chars == num_elements-1) // ASCII data only, use direct index!
					return start+char_num;

				up = start;		// Set initial starting point for forward search
				start_char = 0;
				ep = start+num_elements-1;	// and for backward search (end_char is set above)

				// If we have a bookmark, move either the forward or backward search starting point to it.
				if (mark.char_num > 0)
				{
					if (char_num >= mark.char_num)
					{		// forget about starting from the start
						up += mark.byte_num;
						start_char = mark.char_num;
					}
					else
					{		// Don't search past here, maybe back from here
						ep = start+mark.byte_num;
						end_char = mark.char_num;
					}
				}

				/*
				 * Decide whether to search forwards from up/start_char or backwards from ep/end_char.
				 */
				if (char_num-start_char < end_char-char_num)
				{		// Forwards search is shorter
					end_char = char_num-start_char; // How far forward should we search?
					while (start_char < char_num && up < ep)
					{
						up += UTF8Len(up);
						start_char++;
					}
				}
				else
				{		// Search back from end_char to char_num
					while (end_char > char_num && ep && ep > up)
					{
						ep = (UTF8*)UTF8Backup(ep, start);
						end_char--;
					}
					up = ep;
				}

				// Save the bookmark if it looks likely to be helpful.
				if (up && char_num > 3 && char_num < num_chars-3)
				{
					mark.byte_num = up-start;
					mark.char_num = char_num;
				}
				return up;
			}
	UTF8*		startChar() const { return static_cast<UTF8*>(start); }
	UTF8*		endChar() const { return startChar()+num_elements-1; }
	bool		isNulTerminated() const				// If we allocated memory, it's always terminated
			{ return num_alloc > 0 || start[num_elements] == '\0'; }
	void		insert(Index pos, const char* addend, Index len)
			{
				Body::insert(pos, addend, len);
				num_chars = 0;
			}
	void		transform(const std::function<Val(const UTF8*& cp, const UTF8* ep)> xform, int after = -1);
	void		toLower()
			{
				UTF8		one_char[7];
				StrBodyI	temp_body;	// Any StrVal that references this must have a shorter lifetime. transform() guarantees this.
				transform(
					[&](const UTF8*& cp, const UTF8* ep) -> Val
					{
						UCS4	ch = UTF8Get(cp);	// Get UCS4 character
						ch = UCS4ToLower(ch);		// Transform it
						UTF8*	op = one_char;		// Pack it into our local buffer
						UTF8Put(op, ch);
						*op = '\0';
						// Assign this to the body in our closure
						temp_body = StrBodyI(one_char, false, op-one_char, 1);
						return Val(&temp_body);
					}
				);
			}
	void		toUpper()
			{
				UTF8		one_char[7];
				StrBodyI	temp_body;	// Any StrVal that references this must have a shorter lifetime. transform() guarantees this.
				transform(
					[&](const UTF8*& cp, const UTF8* ep) -> Val
					{
						UCS4	ch = UTF8Get(cp);	// Get UCS4 character
						ch = UCS4ToUpper(ch);		// Transform it
						UTF8*	op = one_char;		// Pack it into our local buffer
						UTF8Put(op, ch);
						*op = '\0';

						// Assign this to the body in our closure
						temp_body = StrBodyI(one_char, false, op-one_char+1);
						return Val(&temp_body);
					}
				);
			}

protected:
	Index		num_chars;	// zero if not yet counted
	StrBodyI& operator=(const StrBodyI& s1)	 // Assignment operator; ONLY for no-copy bodies
			{
				assert(s1.num_alloc == 0);	// Must not do this if we would make two references to allocated data
				start = s1.start;
				ArrayBody<Index>::AddRef();	// Ensure we don't get deleted
				num_chars = s1.num_chars;
				num_elements = s1.num_elements;
				num_alloc = 0;
				return *this;
			}
	void		countChars()
			{
				if (num_chars > 0 || num_elements == 0)
					return;				// Already counted
				const UTF8*	cp = start;		// Progress pointer when reading data
				UTF8*		ep = start+num_elements-1;	// Marker for end of data
				while (cp < ep)
				{
					UCS4		ch = UTF8Get(cp);
					if (ch == UCS4_NONE		// Illegal encoding not handled by UTF8_ILLEGAL
					 || cp > ep)			// Overlaps the end of data
						break;			// An error occurred before the end of the data; truncate it.
					// Count illegal UTF-8 characters here
					num_chars++;
				}
			}
};

template<typename Index> class StrBodyI<Index> StrBodyI<Index>::nullBody("", false, 0, 0);

// Expose a slice onto the underlying StrBodyI UTF8 storage as an array of bytes
template<typename Index>
class CharBufI
: public Array<char, Index, StrBodyI<Index>>
{
};

// A StrVal defined by number of bits in the index:
template<unsigned int IndexBits = StrValIndexBits>
class StrValB
: public StrValI<typename std::conditional<(IndexBits <= 16), uint16_t, uint32_t>::type>
{
};

template<typename Index>
class StrValI
{
	using Bookmark = StrBookmark<Index>;
	using Body = StrBodyI<Index>;
public:
	typedef enum {
		CompareRaw,		// No processing, just the characters
		CompareCI,		// Case independent
		// REVISIT: Language-sensitive collation must consider 2-1 and 1-2 digraphs for each locale
		// Then there's the issue of Unicode normalization (de/composition), which should use transform()
		CompareNatural		// Natural comparison, with numeric strings by value
	} CompareStyle;

	static const StrValI	null;

	~StrValI() {}			// Destructor
	StrValI()			// Empty string
			: body(&Body::nullBody)
			, offset(0)
			, num_chars(0)
			{}
	StrValI(const StrValI& s1)	// Normal copy constructor
			: body(s1.body), offset(s1.offset), num_chars(s1.num_chars) {}
	StrValI& operator=(const StrValI& s1) // Assignment operator
			{
				body = s1.body;
				offset = s1.offset;
				num_chars = s1.num_chars;
				return *this;
			}
	StrValI(const char* data)	// construct by copying NUL-terminated data
			: body(data == 0 || data[0] == '\0' ? &Body::nullBody : new Body(data, true, strlen(data)+1))
			, offset(0)
			, num_chars(body->numChars())
			{
			}
	StrValI(const char* data, Index length, size_t allocate = 0) // construct from length-terminated char data
			: body(0)
			, offset(0)
			, num_chars(0)
			, mark()
			{
				if (allocate <= length)
					allocate = 0;
				body = new Body(data, true, length+1, allocate);
				num_chars = body->numChars();
			}
	StrValI(UCS4 character)		// construct from single-character string
			: body(0), offset(0), num_chars(0)
			{
				UTF8	one_char[7];
				UTF8*	op = one_char;		// Pack it into our local buffer
				UTF8Put(op, character);
				*op = '\0';
				body = new Body(one_char, true, op-one_char+1);
				num_chars = 1;
			}
	StrValI(Body* s1)		// New reference to same string body; used for static strings
			: body(s1), offset(0), num_chars(s1->numChars()) { }
	StrValI(CharBufI<Index> cb)
			: body(cb.body), offset(cb.offset), num_chars(0)
			{
				// If the Slice doesn't start with legal UTF8, we can't represent the result correctly without Unsharing
				// because we cannot rely on counting characters
				const UTF8*	cp = body->start+cb.offset;
				UCS4		ch;
				if (UCS4IsIllegal(ch = UTF8Get(cp)))
					Unshare();

				// Find the character offset and number of characters in this CharBuf slice
				cp = body->start;				// Progress pointer when reading data
				const UTF8*	dp = cp+offset;			// Start of this CharBuf's data
				UTF8*		ep = cp+cb.num_elements;	// Marker for end of data
				while (cp < dp)
				{		// Skip the offset, counting characters
					if ((cp = body->nthChar(offset, mark)) && cp >= dp)
						break;	// This might have skipped into a multi-byte char it we hadn't Unshared
					offset++;
				}
				while (cp < ep)
				{
					// Is there another whole UTF8 character before ep?
					if ((cp = (body->nthChar(offset+num_chars, mark))) < ep)
						num_chars++;
				}
			}

	operator CharBufI<Index>()
			{
				const char*	first_byte = nthChar(0);
				return CharBufI<Index>(body, first_byte, nthChar(num_chars)-first_byte);
			}

	Index		numBytes() const { return nthChar(num_chars)-nthChar(0); }
	Index		length() const { return num_chars; }	// Number of chars
	bool		isEmpty() const { return length() == 0; } // equals empty string?
	operator bool() const { return !isEmpty(); }
	inline bool	isShared() const
			{ return body->GetRefCount() > 1; }

	// Access the characters and UTF8 value:
	UCS4		operator[](int charNum) const
			{
				if (charNum == num_chars)
					return '\0';
				const UTF8*	cp = nthChar(charNum);
				if (!cp)
					return UCS4_NONE;
				return UTF8Get(cp);
			}
	const UTF8*	asUTF8()	// Null terminated. Must unshare data if it's a substring with elided suffix
			{
				const	UTF8*	ep = nthChar(num_chars);	// Pointer to end of this slice's data
				if (ep < body->endChar()			// The body has more data following that
				 || !body->isNulTerminated())			// It's a static body with no existing terminator
				{
					Unshare();
					// If we are the last reference and a substring, we might not be terminated
					*body->nthChar(num_chars, mark) = '\0';	// This uses the bookmark just set
				}
				return nthChar(0);
			}
	const UTF8*	asUTF8(Index& bytes) const	// Returns the bytes, but doesn't guarantee NUL termination
			{
				const	UTF8*	cp = nthChar(0);
				const	UTF8*	ep = nthChar(num_chars);
				bytes = ep-cp;
				return cp;
			}

	// Comparisons:
	int		compare(const StrValI&, CompareStyle = CompareRaw) const;
	inline bool	operator==(const StrValI& comparand) const {
				return length() == comparand.length() && compare(comparand) == 0;
			}
	inline bool	operator!=(const StrValI& comparand) const { return !(*this == comparand); }
	inline bool	operator<(const StrValI& comparand) const { return compare(comparand) < 0; }
	inline bool	operator<=(const StrValI& comparand) const { return compare(comparand) <= 0; }
	inline bool	operator>=(const StrValI& comparand) const { return compare(comparand) >= 0; }
	inline bool	operator>(const StrValI& comparand) const { return compare(comparand) > 0; }
	bool		equalCI(const StrValI& s) const		// Case independent equality
			{ return compare(s, CompareCI) == 0; }

	// Extract substrings:
	StrValI		substr(Index at, int len = -1) const
			{
				assert(len >= -1);

				// Quick check for a null substring:
				if (at < 0 || at >= num_chars || len == 0)
					return StrValI();

				// Clamp substring length:
				if (len == -1)
					len = num_chars-at;
				else if (len > num_chars-at)
					len = num_chars-at;

				return StrValI(body, offset+at, len);
			}
	StrValI		head(Index chars) const
			{ return substr(0, chars); }
	StrValI		tail(Index chars) const
			{ return substr(length()-chars, chars); }
	StrValI		shorter(Index chars) const	// all chars up to tail
			{ return substr(0, length()-chars); }
	void		remove(Index at, int len = -1);	// Delete a substring from the middle

	// Search for a character:
	int		find(UCS4 ch, int after = -1) const
			{
				Index		n = after+1;		// First Index we'll look at
				const UTF8*	up;
				while ((up = nthChar(n)) != 0)
				{
					if (ch == UTF8Get(up))
						return n;		// Found at n
					n++;
				}

				return -1;				// Not found
			}
	int		rfind(UCS4 ch, int before = -1) const
			{
				Index		n = (before == -1 ? num_chars : before)-1;		// First Index we'll look at
				const UTF8*	bp;
				while ((bp = nthChar(n)) != 0)
				{
					if (ch == UTF8Get(bp))
						return n;		// Found at n
					n--;
				}

				return -1;				// Not found
			}

	// Search for substrings:
	int		find(const StrValI& s1, int after = -1) const
			{
				Index		n = after+1;		// First Index we'll look at
				Index		last_start = num_chars-s1.length();	// Last possible start position
				const UTF8*	s1start = s1.nthChar(0);
				const UTF8*	up;
				while (n <= last_start && (up = nthChar(n)) != 0)
				{
					if (memcmp(up, s1start, s1.numBytes()) == 0)
						return n;
					n++;
				}
				return -1;
			}
	int		rfind(const StrValI& s1, int before = -1) const
			{
				Index		n = before == -1 ? num_chars-s1.length() : before-1;	// First Index we'll look at
				if (n > num_chars-s1.length())
					n = num_chars-s1.length();

				const UTF8*	s1start = s1.nthChar(0);
				const UTF8*	bp;
				while ((bp = nthChar(n)) != 0)
				{
					if (memcmp(bp, s1start, s1.numBytes()) == 0)
						return n;
					n--;
				}
				return -1;
			}

	// Search for characters in set:
	int		findAny(const StrValI& s1, int after = -1) const
			{
				Index		n = after+1;		// First Index we'll look at
				const UTF8*	s1start = s1.nthChar(0);
				const UTF8*	ep = s1.nthChar(s1.length());	// Byte after the last char in s1
				const UTF8*	up;
				while ((up = nthChar(n)) != 0)
				{
					UCS4	ch = UTF8Get(up);
					for (const UTF8* op = s1start; op < ep; n++)
						if (ch == UTF8Get(op))
							return n;	// Found at n
					n++;
				}

				return -1;				// Not found
			}
	int		rfindAny(const StrValI& s1, int before = -1) const
			{
				Index		n = before == -1 ? num_chars-1 : before-1;	// First Index we'll look at
				const UTF8*	s1start = s1.nthChar(0);
				const UTF8*	ep = s1.nthChar(s1.length());	// Byte after the last char in s1
				const UTF8*	bp;
				while ((bp = nthChar(n)) != 0)
				{
					UCS4	ch = UTF8Get(bp);
					for (const UTF8* op = s1start; op < ep; n++)
						if (ch == UTF8Get(op))
							return n;
					n--;
				}
				return -1;
			}

	// Search for characters not in set:
	int		findNot(const StrValI& s1, int after = -1) const
			{
				Index		n = after+1;		// First Index we'll look at
				const UTF8*	s1start = s1.nthChar(0);
				const UTF8*	ep = s1.nthChar(s1.length());	// Byte after the last char in s1
				const UTF8*	up;
				while ((up = nthChar(n)) != 0)
				{
					UCS4	ch = UTF8Get(up);
					for (const UTF8* op = s1start; op < ep; n++)
						if (ch == UTF8Get(op))
							goto next;	// Found at n
					return n;
				next:
					n++;
				}

				return -1;				// Not found
			}
	int		rfindNot(const StrValI& s1, int before = -1) const
			{
				Index		n = (before == -1 ? num_chars : before)-1;	// First Index we'll look at
				const UTF8*	s1start = s1.nthChar(0);
				const UTF8*	ep = s1.nthChar(s1.length());	// Byte after the last char in s1
				const UTF8*	bp;
				while ((bp = nthChar(n)) != 0)
				{
					UCS4	ch = UTF8Get(bp);
					for (const UTF8* op = s1start; op < ep; n++)
						if (ch == UTF8Get(op))
							goto next;	// Found at n
					return n;
				next:
					n--;
				}
				return -1;
			}

	// Add, producing a new StrValI:
	StrValI		operator+(const char* addend) const
			{ return *this + StrValI(addend); }
	StrValI		operator+(const StrValI& addend) const
			{
				// Handle the rare but important case of extending a slice with a contiguous slice of the same body
				if (static_cast<Body*>(body) == static_cast<Body*>(addend.body)	// From the same body
				 && offset+num_chars == addend.offset)	// And this ends where the addend starts
					return StrValI(body, offset, num_chars+addend.num_chars);

				const UTF8*	cp = nthChar(0);
				Index		len = numBytes();
				StrValI		str(cp, len, len+addend.numBytes());

				str += addend;
				return str;
			}
	StrValI		operator+(UCS4 addend) const
			{
				// Convert addend using a stack-local buffer to save allocation here.
				UTF8	buf[7];				// Enough for 6-byte content plus a NUL
				UTF8*	cp = buf;
				UTF8Put(cp, addend);
				*cp = '\0';
				Body	body(buf, false, cp-buf, 1);

				return operator+(StrValI(&body));
			}

	// Add, StrValI is modified:
	StrValI&	operator+=(const StrValI& addend)
			{
				if (num_chars == 0 && !addend.noCopy())
					return *this = addend;		// Just assign, we were empty anyhow

				append(addend);
				return *this;
			}
	StrValI&	operator+=(UCS4 addend)
			{
				// Convert addend using a stack-local buffer to save allocation here.
				UTF8	buf[7];				// Enough for 6-byte content plus a NUL
				UTF8*	cp = buf;
				UTF8Put(cp, addend);
				*cp = '\0';
				Body	body(buf, false, cp-buf, 1);

				operator+=(StrValI(&body));
				return *this;
			}

	void		insert(Index pos, const StrValI& addend)
			{
				// Handle the rare but important case of extending a slice with a contiguous slice of the same body
				if (pos == num_chars			// Appending at the end
				 && static_cast<Body*>(body) == static_cast<Body*>(addend.body)	// From the same body
				 && offset+pos == addend.offset)	// And addend starts where we end
				{
					num_chars += addend.length();
					return;
				}

				Unshare();
				Index		addend_length;
				const UTF8*	ap = addend.asUTF8(addend_length);
				body->insert(pos, ap, addend_length);
				num_chars += addend.length();
			}
	void		append(const StrValI& addend)
			{ insert(num_chars, addend); }

	StrValI		asLower() const { StrValI lower(*this); lower.toLower(); return lower; }
	StrValI		asUpper() const { StrValI upper(*this); upper.toUpper(); return upper; }
	void		toLower()
			{
				Unshare();	// REVISIT: Unshare only when first change must be made
				body->toLower();
				num_chars = body->numChars();
			}
	void		toUpper()
			{
				Unshare();	// REVISIT: Unshare only when first change must be made
				body->toUpper();
				num_chars = body->numChars();
			}
	void		transform(const std::function<StrValI(const UTF8*& cp, const UTF8* ep)> xform, int after = -1);

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
				ErrNum*	err_return = 0, // error return
				int	radix = 0,	// base for conversion
				Index*	scanned = 0	// characters scanned
			) const;

protected:
	StrValI(Body* s1, Index offs, Index len)	// offs/len not bounds-checked!
			: body(s1), offset(offs), num_chars(len) { }
	const char*	nthChar(Index char_num) const	// Return a pointer to the start of the nth character
			{
				if (char_num < 0 || char_num > num_chars)
					return 0;
				Bookmark	unsaved = mark;
				return body->nthChar(offset+char_num, unsaved);
			}
	const char*	nthChar(Index char_num)	// Return a pointer to the start of the nth character
			{
				if (char_num < 0 || char_num > num_chars)
					return 0;
				return body->nthChar(offset+char_num, mark);
			}
	bool		noCopy() const
			{ return body->noCopy(); }

private:
	Ref<Body>	body;		// The storage structure for the character data
	Index		offset;		// What char number we start at
	Index		num_chars;	// How many chars we include in this slice
	Bookmark	mark;

	void		Unshare()
			{
				if (body->GetRefCount() <= 1)
					return;

				// Copy only this slice of the body's data, and reset our offset to zero
				Bookmark	savemark(mark);			// copy the bookmark
				const UTF8*	cp = nthChar(0);		// start of this substring
				const UTF8*	ep = nthChar(num_chars);	// end of this substring
				Index		prefix_bytes = cp - body->startChar(); // How many leading bytes of the body we are eliding

				body = new Body(cp, true, ep-cp+1);
				mark.char_num = savemark.char_num - offset;	// Restore the bookmark
				mark.byte_num = savemark.byte_num - prefix_bytes;
				offset = 0;
			}

	static int HexAlpha(UCS4 ch)
			{
				return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
					? (int)((ch & ~('a'-'A')) - 'A' + 10)
					: -1;
			}

	static int Digit(UCS4 ch, int radix)
			{
				int	d;

				if ((d = UCS4Digit(ch)) < 0)		// Not decimal digit
				{
					if (radix > 10
					 && (d = HexAlpha(ch)) < 0)	// Not ASCII a-z, A-Z either
						return -1;		// ditch it
				}

				if (d < radix || (d == 1 && radix == 1))
					return d;
				else
					return -1;
			}

	};

template<typename Index>
const class StrValI<Index>	StrValI<Index>::null;

template<typename Index>
int StrValI<Index>::compare(const StrValI& comparand, CompareStyle style) const
{
	int	cmp;
	switch (style)
	{
	case CompareRaw:
		cmp = memcmp(nthChar(0), comparand.nthChar(0), numBytes());
		if (cmp == 0)
			cmp = numBytes() - comparand.numBytes();
		return cmp;

	case CompareCI:
		assert(!"REVISIT: Case-independent comparison is not implemented");

	case CompareNatural:
		assert(!"REVISIT: Natural comparison is not implemented");

	default:
		return 0;
	}
}

template<typename Index>
void StrBodyI<Index>::transform(const std::function<Val(const UTF8*& cp, const UTF8* ep)> xform, int after)
{
	assert(ref_count <= 1);
	UTF8*		old_start = start;
	size_t		old_num_elements = num_elements;

	// Allocate new data, preserving the old
	start = 0;
	num_chars = 0;
	num_elements = 0;
	num_alloc = 0;
	ArrayBody<char, Index>::resize(old_num_elements+6);		// Start with same allocation plus one character space

	const UTF8*	up = old_start;		// Input pointer
	const UTF8*	ep = old_start+old_num_elements;	// Termination guard
	Index		processed_chars = 0;	// Total input chars transformed
	bool		stopped = false;	// Are we done yet?
	UTF8*		op = start;		// Output pointer
	while (up < ep)
	{
		const UTF8*	next = up;
		if (processed_chars+1 > after+1	// Not yet reached the point to start transforming
		 && !stopped)			// We have stopped transforming
		{
			StrVal		replacement = xform(next, ep);
			Index		replaced_bytes = next-up;	// How many bytes were consumed?
			Index		replaced_chars = replacement.length();	// Replaced by how many chars?

			// Advance 'up' over the replaced characters
			while (up < next)
			{
				up += UTF8Len(up);
				processed_chars++;
			}

			stopped |= (replaced_bytes == 0);

			Index		replacement_bytes;
			const UTF8*	rp = replacement.asUTF8(replacement_bytes);
			ArrayBody<char, Index>::insert(num_chars, rp, replacement_bytes);
			num_chars += replacement.length();
			op = start+num_elements;
		}
		else
		{		// Just copy one character and move on
			UCS4	ch = UTF8Get(up);	// Get UCS4 character
			processed_chars++;
			if (num_alloc < (op-start+6+1))
			{
				ArrayBody<char, Index>::resize((op-start)+6+1);	// Room for any char and NUL
				op = start+num_elements;	// Reset our output pointer in case start has changed
			}
			UTF8Put(op, ch);
			num_elements = op-start;
			num_chars++;
		}
	}
	*op = '\0';
	delete [] old_start;
}

template<typename Index>
void StrValI<Index>::transform(const std::function<StrValI(const UTF8*& cp, const UTF8* ep)> xform, int after)
{
	Unshare();
	body->transform(xform, after);
	num_chars = body->numChars();
	mark = Bookmark();
}

template<typename Index>
int32_t StrValI<Index>::asInt32(
	ErrNum*	err_return,	// error return
	int	radix,		// base for conversion
	Index*	scanned		// characters scanned
) const
{
	Index		len = length();		// length of string
	Index		i = 0;			// position of next character
	UCS4		ch = 0;			// current character
	int		d;			// current digit value
	bool		negative = false;	// Was a '-' sign seen?
	unsigned long	l = 0;			// Number being converted
	unsigned long	last;
	unsigned long	max;

	if (err_return)
		*err_return = 0;

	// Check legal radix
	if (radix < 0 || radix > 36)
	{
		if (err_return)
			*err_return = ErrNum(STRERR_SET, STRERR_ILLEGAL_RADIX);
		if (scanned)
			*scanned = 0;
		return 0;
	}

	// Skip leading white-space
	while (i < len && UCS4IsWhite(ch = (*this)[i]))
		i++;
	if (i == len)
		goto no_digits;

	// Check for sign character
	if (ch == '+' || ch == '-')
	{
		i++;
		negative = ch == '-';
		while (i < len && UCS4IsWhite(ch = (*this)[i]))
			i++;
		if (i == len)
			goto no_digits;
	}

	if (radix == 0)		// Auto-detect radix (octal, decimal, binary)
	{
		if (UCS4Digit(ch) == 0 && i+1 < len)
		{
			// ch is the digit zero, look ahead
			switch ((*this)[i+1])
			{
			case 'b': case 'B':
				if (radix == 0 || radix == 2)
				{
					radix = 2;
					ch = (*this)[i += 2];
					if (i == len)
						goto no_digits;
				}
				break;
			case 'x': case 'X':
				if (radix == 0 || radix == 16)
				{
					radix = 16;
					ch = (*this)[i += 2];
					if (i == len)
						goto no_digits;
				}
				break;
			default:
				if (radix == 0)
					radix = 8;
				break;
			}
		}
		else
			radix = 10;
	}

	// Check there's at least one digit:
	if ((d = Digit(ch, radix)) < 0)
		goto not_number;

	max = (ULONG_MAX-1)/radix + 1;
	// Convert digits
	do {
		i++;			// We're definitely using this char
		last = l;
		if (l > max		// Detect *unsigned* long overflow
		 || (l = l*radix + d) < last)
		{
			// Overflowed unsigned long!
			if (err_return)
				*err_return = ErrNum(STRERR_SET, STRERR_NUMBER_OVERFLOW);
			if (scanned)
				*scanned = i;
			return 0;
		}
	} while (i < len && (d = Digit((*this)[i], radix)) >= 0);

	if (err_return)
		*err_return = 0;

	// Check for trailing non-white characters
	while (i < len && UCS4IsWhite((*this)[i]))
		i++;
	if (i != len && err_return)
		*err_return = ErrNum(STRERR_SET, STRERR_TRAIL_TEXT);

	// Return number of digits scanned
	if (scanned)
		*scanned = i;

	if (l > (unsigned long)LONG_MAX+(negative ? 1 : 0))
	{
		if (err_return)
			*err_return = ErrNum(STRERR_SET, STRERR_NUMBER_OVERFLOW);
		// Try anyway, they might have wanted unsigned!
	}

	/*
	 * Casting unsigned long down to long doesn't clear the high bit
	 * on a twos-complement architecture:
	 */
	return negative ? -(long)l : (long)l;

no_digits:
	if (err_return)
		*err_return = ErrNum(STRERR_SET, STRERR_NO_DIGITS);
	if (scanned)
		*scanned = i;
	return 0;

not_number:
	if (err_return)
		*err_return = ErrNum(STRERR_SET, STRERR_NOT_NUMBER);
	if (scanned)
		*scanned = i;
	return 0;
}

#endif
