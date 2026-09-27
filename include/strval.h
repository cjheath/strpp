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
 * You can cheaply pass a StrVal by copying (and should not pass by reference except for "out" parameters).
 * The body of the string is shared but will be copied to isolate it from any other StrVals before mutation.
 *
 * Not yet:
 * - Unicode normalization (de/composition), see https://en.wikipedia.org/wiki/Unicode_equivalence
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<cstdlib>
#include	<cstdint>
#include	<cstring>
#include	<functional>
#include	<limits>
#include	<type_traits>

#include	<error.h>
#include	<str_err.h>			// The error numbers this header returns
#include	<array.h>
#include	<refcount.h>
#include	<char_encoding.h>
#include	<strassert.h>			// A resolution a width cannot hold stops

/*
 * The index type for string sizes, settable by the build as ArrayIndexBits is
 * (see the INDEXBITS option in the Makefile). The string limit follows from it:
 * a string cannot hold as many characters as its index can count, because one
 * count is spoken for as the marker for a body of raw binary data.
 */
#if	!defined(StrValIndexBits)
#define	StrValIndexBits	32
#endif
typedef typename std::conditional<(StrValIndexBits <= 8), uint8_t,
	typename std::conditional<(StrValIndexBits <= 16), uint16_t,
	typename std::conditional<(StrValIndexBits <= 32), uint32_t, uint64_t>::type>::type>::type StrValIndex;
static_assert(StrValIndexBits >= 8 && StrValIndexBits <= sizeof(void*)*8,
	"StrValIndexBits must be from 8 to the width of a pointer");

const	StrValIndex	StrValIndexRawBinaryMarker = ((StrValIndex)-1);	// Marker num_chars for non-UTF8 data
const	StrValIndex	StrValIndexMaxChars = ((StrValIndex)-2);	// Most characters a string may hold
typedef enum {
	StrUTF8,		// Characters are decoded as UTF-8 (the default)
	StrRawBinary,		// One byte is one character, in the locale's 8-bit encoding
} StrDataType;

template<typename Index = StrValIndex> class StrRefI;
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
typedef	StrRefI<>	StrRef;
typedef	StrBodyI<>	StrBody;
class	Variant;
typedef	Array<Variant>	VariantArray;

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
	StrBodyI(const char* data, ArrayOwnership ownership, Index length = 0, Index allocate = 0, StrDataType encoding = StrUTF8)
			: num_chars(0)
			{
				// REVISIT: Need a Panic() function when a string passes the allowed maximum size
				// assert(num_elements < StrValIndexRawBinaryMarker);
				if (length == 0)
					length = strlen(data);		// The caller offered no length, so the NUL is the length

				if (ownership == ArrayBorrow)
				{					// Borrowed data, which the caller keeps
					start = (char*)data;		// Cast const away; we will not alter it
					num_elements = length+1;	// Counting the NUL the caller wrote
					Body::AddRef();			// Cannot be deleted or resized
				}
				else if (ownership == ArrayTakeOver)
				{					// Borrowed no longer: the caller's new[]'d,
									// NUL-terminated buffer becomes this Body's own
									// allocation, freed (delete[]) exactly as if we
									// had allocated it ourselves - no copy is made.
					start = (char*)data;		// Cast const away; it's ours now
					num_elements = length+1;	// Counting the NUL the caller wrote
					num_alloc = length+1;		// Marks it as ours to resize or free
				}
				else
				{					// ArrayCopy
					if (allocate < length+1)
						allocate = length+1;
					Body::resize(allocate);
					memcpy(start, data, length);	// Only what was offered is ever read
					start[length] = '\0';		// The NUL is ours to add
					num_elements = length+1;
				}

				if (encoding == StrRawBinary)
					num_chars = StrValIndexRawBinaryMarker;	// one byte = one char, don't count them
			}

	inline bool	isShared() const			// It's not just this StrVal using this Body
			{ return ref_count > 1; }
	bool		isNulTerminated() const			// If we allocated memory, it's always terminated
			{ return num_alloc > 0 || start[num_elements-1] == '\0'; }
	bool		isRawBinary() const
			{ return num_chars == StrValIndexRawBinaryMarker; }

	Index		numChars()
			{
				if (isRawBinary())
					return num_elements-1;		// The Array always assumes a NUL is present
				if (num_chars == 0 && num_elements > 0)
					countChars();
				return num_chars;
			}

			// Return a pointer to the start of the nth character
	char*		nthChar(Index char_num, Bookmark& mark)
			{
				if (char_num < 0)	// Check char_num is in range.
				{
			bad_offset:	assert(char_num >= 0 && char_num <= numChars());
					return (char*)0;
				}

				if (isRawBinary()
				 || char_num == 0		// Fast path for first char
				 || num_chars == num_elements-1) // ASCII data only, use direct index!
					return start+char_num;

				int		end_char = numChars();	// count the string if necessary
				if (char_num > end_char)	// Check char_num is in range.
					goto bad_offset;

				char*		up;		// starting pointer for forward search
				int		start_char;	// starting char number for forward search
				char*		ep;		// starting pointer for backward search
				up = start;			// Set initial starting point for forward search
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
						ep = (char*)UTF8Backup(ep, start);
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
	char*		endChar() const { return start+num_elements-1; }
private:		// Prevent accidental use of Array insert by outsiders
	void		insert(Index pos, const char* addend, Index len) { Body::insert(pos, addend, len); }
public:	void		insertBytes(Index pos, const char* addend, Index len)
			{
				Body::insert(pos, addend, len);
				if (!isRawBinary())
					num_chars = 0;	// Force a re-count
			}
	void		transform(const std::function<Val(const char*& cp, const char* ep)> xform, int after = -1);
	void		toLower()
			{
				char		one_char[7];
				bool		nonASCII = false;
				StrBodyI	temp_body;	// No StrVal reference may have a longer lifetime
				transform(
					[&](const char*& cp, const char* ep) -> Val
					{
						// REVISIT: Handle StrRawBinary data
						UCS4	ch = getChar(cp);
						ch = UCS4ToLower(ch);		// Transform it
						nonASCII |= !UCS4IsASCII(ch);
						char*	op = one_char;		// Pack it into our local buffer
						putChar(op, ch);		// As itself on a raw-binary body, else as UTF-8
						*op = '\0';
						// Assign this to the body in our closure
						temp_body = StrBodyI(one_char, ArrayBorrow, op-one_char);
						return Val(&temp_body);
					}
				);
				if (!isRawBinary() && nonASCII)	// Need to convert to UTF8 mode
					num_chars = 0;	// Count the UTF8 bytes we wrote
			}
	void		toUpper()
			{
				bool		nonASCII = false;
				char		one_char[7];
				StrBodyI	temp_body;	// No StrVal reference may have a longer lifetime
				transform(
					[&](const char*& cp, const char* ep) -> Val
					{
						UCS4	ch = getChar(cp);
						ch = UCS4ToUpper(ch);		// Transform it
						nonASCII |= !UCS4IsASCII(ch);
						char*	op = one_char;		// Pack it into our local buffer
						putChar(op, ch);		// As itself on a raw-binary body, else as UTF-8
						*op = '\0';

						// Assign this to the body in our closure
						temp_body = StrBodyI(one_char, ArrayBorrow, op-one_char);
						return Val(&temp_body);
					}
				);
				if (!isRawBinary() && nonASCII)	// Need to convert to UTF8 mode
					num_chars = 0;	// Count the UTF8 bytes we wrote
			}
	void		toJSON();

	StrBodyI& operator=(const StrBodyI& s1)	 // Assignment operator; ONLY for no-copy bodies
			{
				assert(s1.num_alloc == 0);	// Must not do this if we would make two references to allocated data
				start = s1.start;
				this->AddRef();			// Ensure we don't get deleted
				num_chars = s1.num_chars;
				num_elements = s1.num_elements;
				num_alloc = 0;
				return *this;
			}

protected:
	// The most characters a body can hold: one count is spoken for as the
	// marker that says the data is raw binary. This is the body's own index,
	// which for a string body is the string index - not ArrayIndex.
	static const Index	maxChars = (Index)-2;

	Index		num_chars;	// zero if not yet counted, StrValIndexRawBinaryMarker if locale-8bit
	void		countChars()
			{
				if (isRawBinary())
					return;

				const char*	cp = start;		// Progress pointer when reading data
				char*		ep = start+num_elements-1;	// Marker for end of data
				size_t		count = 0;		// Counted wide, so a count that cannot
									// be held is seen rather than wrapped
				while (cp < ep)
				{
					UCS4		ch = UTF8Get(cp);
					if (ch == UCS4_NONE		// Illegal encoding not handled by UTF8_ILLEGAL
					 || cp > ep)			// Overlaps the end of data
						break;			// An error occurred before the end of the data; truncate it.
					// Count illegal UTF-8 characters here
					count++;
				}
				StrppAssert(count <= (size_t)maxChars);
				num_chars = (Index)count;
			}

	UCS4		getChar(const char*& cp) const	// Return next character, next advancing cp
			{
				if (isRawBinary())
					return (UCS4)(unsigned char)*cp++;
				return UTF8Get(cp);
			}

	void		putChar(char*& cp, UCS4 ch) const // Store a character, advancing cp
			{
				if (isRawBinary())
				{
					*cp++ = ch;	// REVISIT: Panic on oversized char
					return;		// A raw byte is stored as itself, and only once
				}
				UTF8Put(cp, ch);
			}
};

template<typename Index> class StrBodyI<Index> StrBodyI<Index>::nullBody("", ArrayBorrow, 0, 0);

// A StrVal defined by number of bits in the index:
template<unsigned int IndexBits = StrValIndexBits>
class StrValB
: public StrValI<typename std::conditional<(IndexBits <= 16), uint16_t, uint32_t>::type>
{
};

/*
 * A StrRefI encapsulated a counted reference to a StrBody but contains no data access nor mutation.
 * It is used merely to pass around strings (e.g. in a Variant) without also carrying an unnecessary Bookmark
 */
template<typename Index>
class StrRefI
{
	using Body = StrBodyI<Index>;
	using Bookmark = StrBookmark<Index>;
public:
	~StrRefI() {}			// Destructor
	StrRefI()			// Empty string
			: body(&Body::nullBody)
			, offset(0)
			, num_chars(0)
			{}
	StrRefI(const StrRefI& s1)	// Normal copy constructor
			: body(s1.body), offset(s1.offset), num_chars(s1.num_chars)
			{
			}

	StrRefI(const char* data)	// The common case: copy NUL-terminated UTF-8 data
			: body(data == 0 || data[0] == '\0' ? &Body::nullBody : new Body(data, ArrayCopy))
			, offset(0)
			, num_chars(body->numChars())
			{
			}
	// Construct with specified ownership and/or encoding
	StrRefI(const char* data, ArrayOwnership ownership, StrDataType encoding = StrUTF8)
			: body(data == 0 || (ownership != ArrayTakeOver && data[0] == '\0') ? &Body::nullBody : new Body(data, ownership, 0, 0, encoding))
			, offset(0)
			, num_chars(body->numChars())
			{
			}
	// `allocate` is how many elements the body is to hold, including the terminating NUL
	StrRefI(const char* data, Index length, size_t allocate = 0, ArrayOwnership ownership = ArrayCopy, StrDataType encoding = StrUTF8) // construct from length-terminated char data
			: body(0)
			, offset(0)
			, num_chars(0)
			{
				if (allocate <= length)
					allocate = 0;
				// Even an empty ArrayTakeOver buffer is a real allocation we must
				// own and free, so it must never take the nullBody shortcut.
				if (ownership != ArrayTakeOver && length == 0 && (allocate == 0 || data == 0))
					body = &Body::nullBody;	// Don't use strlen!
				else
					body = new Body(data, ownership, length, allocate, encoding);	// Room to grow is a body of its own
				num_chars = body->numChars();
			}
	StrRefI(UCS4 character)		// construct from single-character string
			: body(0), offset(0), num_chars(0)
			{
				// REVISIT: Handle StrRawBinary data
				char	one_char[7];
				char*	op = one_char;		// Pack it into our local buffer
				UTF8Put(op, character);
				*op = '\0';
				body = new Body(one_char, ArrayCopy, op-one_char);
				num_chars = 1;
			}
	StrRefI(Body* s1)		// New reference to same string body; used for static strings
			: body(s1), offset(0), num_chars(s1->numChars()) { }

	StrRefI& operator=(const StrRefI& s1) // Assignment operator
			{
				body = s1.body;
				offset = s1.offset;
				num_chars = s1.num_chars;
				return *this;
			}

	Index		length() const { return num_chars; }	// Number of chars
	bool		isEmpty() const { return length() == 0; } // equals empty string?
	explicit operator bool() const { return !isEmpty(); }
	bool		isStatic() const { return body->isStatic(); }	// Not owned by this StrRefI's body

	// Must a copy Unshare? a StrRefI::null is static but may be shared. This happens often!
	bool		copyNeedsUnshare() const
			{ return body->isStatic() && static_cast<const Body*>(body) != &Body::nullBody; }

	Index		numBytes() const	// Number of bytes of (UTF-8 or raw binary) data
			{
				const char*	ep = nthChar(length());
				assert(ep);
				return ep-nthChar(0);
			}

	// Comparisons: raw byte-wise only. StrValI adds a compare() of its own, which
	// compares characters when the two encodings differ, and compareNatural().
	int		compare(const StrRefI& comparand) const
			{
				/*
				 * Bytes of two different encodings cannot be compared with each
				 * other: a raw-binary byte is the code point of its own value,
				 * so the same text is one byte on one side and several on the
				 * other. Compare the characters instead, which is what a raw
				 * byte already is. The common case, both sides alike, is still
				 * a byte compare.
				 */
				if (body->isRawBinary() != comparand.body->isRawBinary())
				{
					const char*	cp1 = nthChar(0);
					const char*	ep1 = cp1+numBytes();
					const char*	cp2 = comparand.nthChar(0);
					const char*	ep2 = cp2+comparand.numBytes();
					while (cp1 < ep1 && cp2 < ep2)
					{
						// The same reading the bodies themselves do: a raw byte as itself, else UTF-8
						UCS4	ch1 = body->isRawBinary() ? (UCS4)(unsigned char)*cp1++ : UTF8Get(cp1);
						UCS4	ch2 = comparand.body->isRawBinary() ? (UCS4)(unsigned char)*cp2++ : UTF8Get(cp2);
						if (ch1 != ch2)
							return ch1 < ch2 ? -1 : 1;
					}
					if (cp1 < ep1)
						return 1;		// This one has characters left over
					if (cp2 < ep2)
						return -1;
					return 0;
				}

				// Only compare the overlapping prefix - comparing numBytes() of
				// *this* against a shorter comparand would read past the end of
				// its buffer.
				Index	shorter = numBytes() < comparand.numBytes() ? numBytes() : comparand.numBytes();
				int	cmp = memcmp(nthChar(0), comparand.nthChar(0), shorter);
				if (cmp == 0)
					cmp = numBytes() - comparand.numBytes();
				return cmp;
			}
	inline bool	operator==(const StrRefI& comparand) const
			{ return length() == comparand.length() && compare(comparand) == 0; }
	inline bool	operator!=(const StrRefI& comparand) const { return !(*this == comparand); }
	inline bool	operator<(const StrRefI& comparand) const { return compare(comparand) < 0; }
	inline bool	operator<=(const StrRefI& comparand) const { return compare(comparand) <= 0; }
	inline bool	operator>=(const StrRefI& comparand) const { return compare(comparand) >= 0; }
	inline bool	operator>(const StrRefI& comparand) const { return compare(comparand) > 0; }

protected:
	StrRefI(Body* s1, Index offs, Index len)	// offs/len not bounds-checked!
			: body(s1), offset(offs), num_chars(len) {}

	const char*	nthChar(Index char_num) const	// Return a pointer to the start of the nth character
			{
				if (char_num < 0 || char_num > length())
					return 0;
				Bookmark	unsaved;	// No cache: a StrRefI carries no Bookmark, by design (see class comment)
				return body->nthChar(offset+char_num, unsaved);
			}

	Ref<Body>	body;		// The storage structure for the character data
	Index		offset;		// What char number we start at
	Index		num_chars;	// How many chars we include in this slice
};

template<typename Index>
class StrValI
: public StrRefI<Index>
{
	using Bookmark = StrBookmark<Index>;
	using Base = StrRefI<Index>;
	using Body = StrBodyI<Index>;
protected:
	using Base::body;
	using Base::num_chars;
	using Base::offset;
public:
	using Base::length;
	using Base::numBytes;

protected:
	// A number's digits, and the loop that produces them
	template<typename U> static char*	reprDigits(U u, int base, const char* digits, char* end);
	template<typename N> static StrVal	reprInt(N n, char repr);
	template<typename N> static StrVal	reprUInt(N u, char repr);

public:
	static const StrValI	null;

	~StrValI() {}			// Destructor
	StrValI() : Base() {}		// Empty string
	StrValI(const StrValI& s1)	// Normal copy constructor
			: Base(s1)
			{
				if (s1.copyNeedsUnshare())	// Must not copy a reference to a non-allocated body
					Unshare();
			}
	StrValI(const StrRefI<Index>& s1)	// Copy from StrRef
			: Base(s1)
			{
				if (s1.copyNeedsUnshare())	// Must not copy a reference to a non-allocated body
					Unshare();
			}

	StrValI(const char* data)	// The common case: copy NUL-terminated UTF-8 data
			: Base(data)
			{
			}
	// Construct with specified ownership and/or encoding
	StrValI(const char* data, ArrayOwnership ownership, StrDataType encoding = StrUTF8)
			: Base(data, ownership, encoding)
			{
			}
	StrValI(const char* data, Index length, size_t allocate = 0, ArrayOwnership ownership = ArrayCopy, StrDataType encoding = StrUTF8) // construct from length-terminated char data
			: Base(data, length, allocate, ownership, encoding)
			, mark()
			{
			}
	StrValI(UCS4 character)		// construct from single-character string
			: Base(character)
			{}
	StrValI(Body* s1) : Base(s1) {}	// New reference to same string body; used for static strings

	StrValI& operator=(const StrValI& s1) // Assignment operator
			{
				body = s1.body;
				offset = s1.offset;
				num_chars = s1.num_chars;
				mark = s1.mark;
				if (s1.copyNeedsUnshare())	// Must not copy a reference to a non-allocated body
					Unshare();
				return *this;
			}
	// As above, but from a StrRef, which carries no Bookmark of its own to
	// copy - and skips constructing a temporary StrValI to assign from, which
	// operator=(const StrValI&) alone would otherwise need via the converting
	// constructor above.
	StrValI& operator=(const StrRefI<Index>& s1)
			{
				Base::operator=(s1);		// StrRefI's own operator=: body, offset, num_chars
				mark = Bookmark();
				if (s1.copyNeedsUnshare())	// Must not copy a reference to a non-allocated body
					Unshare();
				return *this;
			}
	// Without this, "str = literal" is ambiguous: StrValI and StrRefI each
	// have their own non-explicit converting constructor from a const char*,
	// so the two operator='s above are equally good (one user-defined
	// conversion each) for that argument. A const char* binds to this one
	// with no conversion at all, which wins outright and settles it.
	StrValI& operator=(const char* cp)
			{ return *this = StrValI(cp); }

	// numBytes() is inherited from StrRefI

	// Access the characters and character value:
	UCS4		operator[](int charNum) const
			{
				if (charNum == length())
					return '\0';
				const char*	cp = nthChar(charNum);
				if (!cp)
					return UCS4_NONE;
				return body->isRawBinary() ? (UCS4)(unsigned char)*cp : UTF8Get(cp);
			}
	const char*	asUTF8()	// Null terminated. Must unshare data if it's a substring with elided suffix
			{
				if (offset+length() < body->numChars() // Substring ends before body does
				 || !body->isNulTerminated())		// Body wasn't terminated anyhow
				 	copyBody();
				return nthChar(0);
			}
	const char*	asUTF8(Index& bytes) const	// Returns the bytes, but doesn't guarantee NUL termination
			{
				const	char*	cp = nthChar(0);
				const	char*	ep = nthChar(length());
				bytes = ep-cp;
				return cp;
			}

	// Comparisons, raw characters, and natural (comparing digit strings numerically).
	// We don't attempt language-sensitive collation (1-2 & 2-1 mappings) or normalization
	int		compare(const StrValI&) const;
	int		compareNatural(const StrValI&) const;

	inline bool	operator==(const StrValI& comparand) const {
				return length() == comparand.length() && compare(comparand) == 0;
			}
	inline bool	operator!=(const StrValI& comparand) const { return !(*this == comparand); }
	inline bool	operator<(const StrValI& comparand) const { return compare(comparand) < 0; }
	inline bool	operator<=(const StrValI& comparand) const { return compare(comparand) <= 0; }
	inline bool	operator>=(const StrValI& comparand) const { return compare(comparand) >= 0; }
	inline bool	operator>(const StrValI& comparand) const { return compare(comparand) > 0; }

	// Minimum requirements for using a StrVal with std::map:
	static bool	compare(const StrValI& c1, const StrValI& c2);
	static bool	equiv(const StrValI& c1, const StrValI& c2);

	// Extract substrings:
	// A request overlapping a boundary is clamped. Step right outside,that's an error.
	StrValI		substr(Index at, int len = -1) const
			{
				// Quick check for a null substring:
				if (len < -1 || len == 0 || at >= length())
				{
					if (at > length())
						array_index_error(at, length(), "slice");
					return null;
				}

				// Clamp substring length:
				if (len < 0 || (Index)len > length()-at)	// -1 means "to the end"
					len = (int)(length()-at);

				return StrValI(body, offset+at, len);
			}
	StrValI		head(Index chars) const
			{ return substr(0, chars); }
	StrValI		tail(Index chars) const
			{ return chars >= length() ? *this : substr(length()-chars, chars); }
	StrValI		shorter(Index chars) const	// all chars up to tail
			{
				if (chars > length())	// Nothing left
					return StrValI();
				return substr(0, length()-chars);
			}
//	StrValI&	remove(Index at, int len = -1);	// Delete a substring from the middle

	// Search for a character:
	int		find(UCS4 ch, int after = -1) const
			{
				Index		n = after+1;		// First Index we'll look at
				const char*	up;
				while ((up = nthChar(n)) != 0)
				{
					if (ch == getChar(up))
						return n;		// Found at n
					n++;
				}

				return -1;				// Not found
			}
	int		rfind(UCS4 ch, int before = -1) const
			{
				Index		n = (before == -1 ? length() : before)-1; // First Index we'll look at
				const char*	bp;
				while ((bp = nthChar(n)) != 0)
				{
					if (ch == getChar(bp))
						return n;		// Found at n
					n--;
				}

				return -1;				// Not found
			}

	// Search for substrings:
	int		find(const StrValI& s1, int after = -1) const
			{
				// Bytes of different encodings cannot be matched; the text forms can
				if (body->isRawBinary() != s1.body->isRawBinary())
					return asText().find(s1.asText(), after);

				Index		n = after+1;		// First Index we'll look at
				Index		last_start = length()-s1.length();	// Last possible start position
				const char*	s1start = s1.nthChar(0);
				const char*	up;
				while (n <= last_start && (up = nthChar(n)) != 0)
				{
					// REVISIT: Only works if the StrDataType matches
					if (memcmp(up, s1start, s1.numBytes()) == 0)
						return n;
					n++;
				}
				return -1;
			}
	int		rfind(const StrValI& s1, int before = -1) const
			{
				// Bytes of different encodings cannot be matched; the text forms can
				if (body->isRawBinary() != s1.body->isRawBinary())
					return asText().rfind(s1.asText(), before);

				Index		n = before == -1 ? length()-s1.length() : before-1;	// First Index we'll look at
				if (n > length()-s1.length())
					n = length()-s1.length();

				const char*	s1start = s1.nthChar(0);
				const char*	bp;
				while ((bp = nthChar(n)) != 0)
				{
					// REVISIT: Only works if the StrDataType matches
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
				const char*	up;
				while ((up = nthChar(n)) != 0)
				{
					UCS4	ch = getChar(up);
					for (Index i = 0; i < s1.length(); i++)
						if (ch == s1[i])
							return n;	// Found at n
					n++;
				}

				return -1;				// Not found
			}
	int		rfindAny(const StrValI& s1, int before = -1) const
			{
				Index		n = before == -1 ? length()-1 : before-1;	// First Index we'll look at
				const char*	bp;
				while ((bp = nthChar(n)) != 0)
				{
					UCS4	ch = getChar(bp);
					for (Index i = 0; i < s1.length(); i++)
						if (ch == s1[i])
							return n;
					n--;
				}
				return -1;
			}

	// Search for characters not in set:
	int		findNot(const StrValI& s1, int after = -1) const
			{
				Index		n = after+1;		// First Index we'll look at
				const char*	up;
				while ((up = nthChar(n)) != 0)
				{
					UCS4	ch = getChar(up);
					for (Index i = 0; i < s1.length(); i++)
						if (ch == s1[i])
							goto next;	// Found at n
					return n;
				next:
					n++;
				}

				return -1;				// Not found
			}
	int		rfindNot(const StrValI& s1, int before = -1) const
			{
				Index		n = (before == -1 ? length() : before)-1;	// First Index we'll look at
				const char*	bp;
				while ((bp = nthChar(n)) != 0)
				{
					UCS4	ch = getChar(bp);
					for (Index i = 0; i < s1.length(); i++)
						if (ch == s1[i])
							goto next;	// Found at n
					return n;
				next:
					n--;
				}
				return -1;
			}

	// Add, producing a new StrValI:
	StrValI		operator+(const char* addend) const
			{	// Borrowed while the result is built rather than copied first:
				// what is added is read once, into the new string
				StrBody	body(addend, ArrayBorrow);
				return *this + StrValI(&body);
			}
	StrValI		operator+(const StrValI& addend) const
			{
				// Handle the rare but important case of extending a slice with a contiguous slice of the same body
				if (static_cast<Body*>(body) == static_cast<Body*>(addend.body)	// From the same body
				 && offset+length() == addend.offset)	// And this ends where the addend starts
					return StrValI(body, offset, length()+addend.num_chars);

				const char*	cp = nthChar(0);
				Index		len = numBytes();
				// Copy with the encoding this string has: appending converts if the two differ
				StrValI		str(cp, len, len+addend.numBytes()+1, ArrayCopy,
						body->isRawBinary() ? StrRawBinary : StrUTF8);	// +1 counts the terminator

				str += addend;
				return str;
			}
	StrValI		operator+(UCS4 addend) const
			{
				// REVISIT: Handle StrRawBinary data more efficiently (no double-conversion)
				// Convert addend using a stack-local buffer to save allocation here.
				char	buf[7];				// Enough for 6-byte content plus a NUL
				char*	cp = buf;
				UTF8Put(cp, addend);
				*cp = '\0';
				Body	body(buf, ArrayBorrow, cp-buf, 1);

				return operator+(StrValI(&body));
			}

	// Add, StrValI is modified:
	StrValI&	operator+=(const StrValI& addend)
			{
				if (length() == 0 && !addend.isStatic() && !body->ownsData())
					return *this = addend;		// Just assign, we were empty with no room anyhow

				append(addend);
				return *this;
			}
	StrValI&	operator+=(UCS4 addend)
			{
				// REVISIT: Handle StrRawBinary data more efficiently (no double-conversion)
				// Convert addend using a stack-local buffer to save allocation here.
				char	buf[7];				// Enough for 6-byte content plus a NUL
				char*	cp = buf;
				UTF8Put(cp, addend);
				*cp = '\0';
				Body	body(buf, ArrayBorrow, cp-buf, 1);

				operator+=(StrValI(&body));
				return *this;
			}
	StrValI		operator*(int repeats)
			{
				const char*	first_byte = nthChar(0);
				const char*	end_byte = nthChar(length());	// Pre-allocate enough memory
				StrValI	res("", 0, (end_byte-first_byte)*repeats+1);

				for (int i = 0; i < repeats; i++)
					res += *this;
				return res;
			}

	StrValI&	insert(Index pos, const StrValI& addend)
			{
				// Handle the rare but important case of extending a slice with a contiguous slice of the same body
				if (pos == length()			// Appending at the end
				 && static_cast<Body*>(body) == static_cast<Body*>(addend.body)	// From the same body
				 && offset+pos == addend.offset)	// And addend starts where we end
				{
					StrppAssert((size_t)num_chars + addend.length() <= StrValIndexMaxChars);
					num_chars += addend.length();
					return *this;
				}

				/*
				 * One body holds one encoding, and a raw-binary byte means the
				 * code point of its own value, so text is the form that can
				 * hold both: whichever side is raw is converted to it.
				 */
				if (body->isRawBinary() != addend.body->isRawBinary())
				{
					if (body->isRawBinary())
						*this = asText();	// This receiver becomes text
					else
					{
						StrValI	text = addend.asText();
						return insert(pos, text);	// Insert in text form instead
					}
				}

				/*
				 * The position is a character index, and nthChar() is how it
				 * becomes a byte offset: an index it cannot find answers null,
				 * and subtracting a null pointer makes a wild offset, not a
				 * failed insert. So refuse first, and change nothing.
				 */
				if (pos > length())
				{
					array_index_error(pos, length(), "insert");	// Reports
					return *this;
				}

				Unshare();

				Index		addend_length;		// Get length in bytes
				const char*	ap = addend.asUTF8(addend_length);
				body->insertBytes(nthChar(pos)-nthChar(0), ap, addend_length);
				// REVISIT: update or nullify the bookmark if after insertion point
				num_chars += addend.length();
				return *this;
			}
	StrValI&	append(const StrValI& addend)
			{ return insert(length(), addend); }
	StrValI&	prepend(const StrValI& addend)
			{ return insert(0, addend); }

	StrValI		asLower() const { StrValI lower(*this); lower.toLower(); return lower; }
	StrValI		asUpper() const { StrValI upper(*this); upper.toUpper(); return upper; }
	StrValI&	toLower()
			{
				Unshare();	// REVISIT: Unshare only when first change must be made
				body->toLower();
				num_chars = body->numChars();
				return *this;
			}
	StrValI&	toUpper()
			{
				Unshare();	// REVISIT: Unshare only when first change must be made
				body->toUpper();
				num_chars = body->numChars();
				return *this;
			}
	StrValI&	transform(const std::function<StrValI(const char*& cp, const char* ep)> xform, int after = -1);
	StrValI		asJSON() const { StrValI json(*this); json.toJSON(); return json; }
	StrValI&	toJSON()
			{
				Unshare();
				body->toJSON();
				num_chars = body->numChars();
				return *this;
			}

	/*
	 * An integer as text, in the representation `repr` names: one of
	 * b, o, d, x or X, or 0, which is decimal. No base carries a prefix -
	 * a text that wants 0x writes it itself, so that a translated text
	 * keeps the prefix where its own language wants it.
	 *
	 * Decimal renders the sign; the most negative value of a type has no
	 * positive counterpart, so its digits are built from its unsigned
	 * form. Every other base renders the bit pattern of the value at the
	 * width of its own type, which is what a base is for: an int of -1 is
	 * ffffffff, and no sign is involved.
	 *
	 * There is one function per type rather than one taking the widest,
	 * because the width is what a non-decimal base renders. A long is not
	 * the same width on every target, so the function that matches the
	 * type in hand is the one that writes the width that type has.
	 *
	 * These are the inverse of asInt32 below, and are what strval_render
	 * in strformat.h renders an integer parameter with - see
	 * doc/strval.md, "Integers as text".
	 */
	static StrVal	fromInt32(int32_t n, char repr = 0);
	static StrVal	fromUInt32(uint32_t n, char repr = 0);
	static StrVal	fromLong(long n, char repr = 0);
	static StrVal	fromULong(unsigned long n, char repr = 0);
	static StrVal	fromInt64(int64_t n, char repr = 0);
	static StrVal	fromUInt64(uint64_t n, char repr = 0);

	/*
	 * Convert a string to a number, of any width, at the caller's resolution.
	 *
	 * asInteger reads a whole number of type T. asFixedPoint reads a number
	 * with a radix point as a count of `places` places after it: "1.5" read
	 * in radix 10 with eight places is 150000000, and "1.8" read in radix 16
	 * with one is 24. A caller whose values are counted in hundredths, or in
	 * 10^-8 seconds, names that resolution and gets an exact integer rather
	 * than a rounding.
	 *
	 * Leading and trailing spaces are scanned and ignored; other non-numeric
	 * characters are flagged as an error. The radix may be 0 or 2-36: beyond
	 * 10 the ASCII alphabet is used for digits above 9, upper or lower case.
	 * Radix 2 allows 0b..., radix 16 allows 0x..., and radix 0 recognises
	 * both and treats a number with a leading zero as octal, as C does - so a
	 * caller reading decimal text passes 10, and nothing is read as octal. A
	 * radix point is a radix point in any radix.
	 *
	 * Digits the value cannot hold do not truncate and are not rounded: the
	 * number stops at the last digit that fits its type and its resolution,
	 * and the digits after it are reported as trailing text. A caller whose
	 * resolution is coarser than the text rolls that report back and keeps
	 * what fitted, which is what rx/rxcompile.cpp has always done with it; a
	 * caller for whom a third decimal place is a mistake leaves the report
	 * standing. Either way, a value is answered.
	 *
	 * These are templates so that no width is compiled unless a caller names
	 * it: nothing instantiates either of them for any T, and asInt32 below is
	 * asInteger<int32_t> and nothing more.
	 *
	 * @retval 0 no problems
	 * @retval STRERR_TRAIL_TEXT There are non-blank characters after the number
	 * @retval STRERR_NO_DIGITS Number string contains only blank characters
	 * @retval STRERR_NOT_NUMBER The first non-blank character was non-numeric
	 * @retval STRERR_ILLEGAL_RADIX The radix is not one a number can be read in
	 */
	template<typename T>
	T		asInteger(
				ErrNum*	err_return = 0, // error return
				int	radix = 0,	// base for conversion
				Index*	scanned = 0	// characters scanned
			) const;

	// The same, read as a count of `places` places after the radix point.
	template<typename T>
	T		asFixedPoint(
				int	places,		// Digits after the radix point
				ErrNum*	err_return = 0,
				int	radix = 0,
				Index*	scanned = 0
			) const;

	// The int32 case of asInteger, for a caller that knows its width. This
	// library uses it itself, so it is compiled whether or not a program
	// ever calls it.
	int32_t		asInt32(
				ErrNum*	err_return = 0, // error return
				int	radix = 0,	// base for conversion
				Index*	scanned = 0	// characters scanned
			) const
			{ return asInteger<int32_t>(err_return, radix, scanned); }

	// Expand a text by interpolating the positional parameters of `args`:
	// {1} is the first, {2} the second. An array or a map among them is
	// expanded, to at most RENDER_MAX_DEPTH levels. See include/strformat.h,
	// and doc/strval.md, "Substituting parameters into a text".
	static StrVal	format(StrVal f, VariantArray args);

protected:
	StrValI(Body* s1, Index offs, Index len)	// offs/len not bounds-checked!
			: Base(s1, offs, len) { }
	const char*	nthChar(Index char_num) const	// Return a pointer to the start of the nth character
			{
				if (char_num < 0 || char_num > length())
					return 0;
				Bookmark	unsaved = mark;
				return body->nthChar(offset+char_num, unsaved);
			}
	const char*	nthChar(Index char_num)	// Return a pointer to the start of the nth character
			{
				if (char_num < 0 || char_num > length())
					return 0;
				return body->nthChar(offset+char_num, mark);
			}
	// isStatic() is inherited from StrRefI

private:
	Bookmark	mark;

	/*
	 * Where a number is in a text, and what is wrong with it if it is not one.
	 * It knows no width and no scale: it finds the sign, the run of digits
	 * before the radix point and the run after it, and returns an ErrNum when
	 * there is no number to be found. What the digits are worth - in a type of
	 * some width, at some resolution - is the reader's business, and so is
	 * everything the value cannot hold.
	 */
	typedef struct NumberScan
	{
		ErrNum	why;		// 0, or why no number was read
		Index	at;		// Where the trouble is, for a report
		int	radix;		// What a radix of 0 was resolved to
		bool	negative;	// A leading minus was read
		Index	digits;		// First digit of the whole part
		Index	digits_end;	// One past the last of them
		Index	fraction;	// First digit after the radix point, or digits_end
		Index	fraction_end;	// One past the last of them
		Index	end;		// One past the number, its 0x prefix included
	} NumberScan;

	static NumberScan	scanNumber(const StrValI& text, int radix);

	/*
	 * The digits a scan found, read into T at the caller's resolution. The
	 * value grows only while it fits the type: a digit that would not is where
	 * the number stops, and everything from there on is trailing text. So
	 * nothing wraps and nothing is rounded, and a caller whose resolution is
	 * coarser than the text rolls the report back and keeps what fitted.
	 */
	template<typename T>
	T		readNumber(const NumberScan& scan, int places, ErrNum* err_return, Index* scanned) const;

	/*
	 * A failure that a reader cannot report from this header: Error() needs a
	 * VariantArray, which needs variant.h, which needs this file. Defined in
	 * src/strval.cpp, as asInt32 itself used to be. `at` says where the
	 * trouble is and `stop` where the number stopped being read; a message
	 * that names neither ignores them.
	 */
	static ErrNum	reportNumber(ErrNum why, const char* type_name, const StrValI& text,
					int radix, Index at, Index stop);

	UCS4		getChar(const char*& cp) const
			{
				if (body->isRawBinary())
					return (UCS4)(unsigned char)*cp++;
				return UTF8Get(cp);
			}
	/*
	 * These characters, encoded as UTF-8: a raw-binary byte is the code point
	 * of its own value, which is what makes this total. A slice of this body
	 * is returned when this is already text, so a caller need not care which
	 * it was. Used where two strings of different encodings must become one.
	 */
	StrValI		asText() const
			{
				if (!body->isRawBinary())
					return *this;

				Index		len = numBytes();
				char*		text = new char[len*2+1];	// A raw byte is at most two UTF-8 bytes
				char*		op = text;
				const char*	cp = nthChar(0);
				for (Index i = 0; i < len; i++)
					UTF8Put(op, (UCS4)(unsigned char)cp[i]);
				*op = '\0';
				return StrValI(text, op-text, 0, ArrayTakeOver);
			}

	void		copyBody()
			{
				// Copy only this slice of the body's data, and reset our offset to zero
				Bookmark	savemark(mark);			// copy the bookmark
				const char*	cp = nthChar(0);		// start of this substring
				const char*	ep = nthChar(length());		// end of this substring
				Index		prefix_bytes = cp - body->nthChar(0, mark); // How many leading bytes of the body we are eliding

				body = new Body(cp, ArrayCopy, ep-cp, 0, body->isRawBinary() ? StrRawBinary : StrUTF8);
				mark.char_num = savemark.char_num - offset;	// Restore the bookmark
				mark.byte_num = savemark.byte_num - prefix_bytes;
				offset = 0;
			}

	void		Unshare()
			{
				// A substring on Unallocated memory which is the last remaining ref
				// cannot be terminated correctly, so must be copied even if unshared
				bool	must_copy_static = body->isStatic() && offset+length() < body->numChars();
				if (must_copy_static || body->isShared())
					copyBody();
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

extern template int StrValI<StrValIndex>::compareNatural(const StrValI<StrValIndex>&) const;

template<typename Index>
const class StrValI<Index>	StrValI<Index>::null;

template<typename Index>
bool StrValI<Index>::compare(const StrValI& c1, const StrValI& c2)
{
	return c1.compare(c2) > 0;
}

template<typename Index>
bool StrValI<Index>::equiv(const StrValI& c1, const StrValI& c2)
{
	return c1.compare(c2) == 0;
}

/*
 * The raw comparison: the characters, with nothing done to them. Two strings
 * of different encodings cannot be compared byte-wise - a raw-binary byte is
 * the code point of its own value, so the same text is one byte on one side
 * and several on the other - and are compared as characters instead, which
 * StrRefI::compare does.
 */
template<typename Index>
int StrValI<Index>::compare(const StrValI& comparand) const
{
	if (body->isRawBinary() != comparand.body->isRawBinary())
		return StrRefI<Index>::compare(comparand);	// Different encodings: by character

	// Only compare the overlapping prefix - comparing numBytes() of
	// *this* against a shorter comparand read past the end of its
	// buffer (a real heap-buffer-overflow, caught by ASan while
	// testing StrVal-keyed CowMap/RbTree usage: "nonexistent" (11
	// bytes) compared against a 1-byte key read 10 bytes past it).
	Index	shorter = numBytes() < comparand.numBytes() ? numBytes() : comparand.numBytes();
	int	cmp = memcmp(nthChar(0), comparand.nthChar(0), shorter);
	if (cmp == 0)
		cmp = numBytes() - comparand.numBytes();
	return cmp;
}

/*
 * The natural comparison: text order, except that a run of decimal digits is
 * compared as the number it spells, so "a10" sorts after "a9". What a digit is
 * worth is UCS4Digit's business, so this is any script's digits, not just the
 * ASCII ones - and a number may therefore be written more than one way.
 *
 * A run of digits that starts with a zero is not a number: "007" is text, and
 * is ordered as text.
 *
 * Two numbers of different length are ordered by length (neither leads with a
 * zero to pad it); of the same length, by their digits' values; and if they are
 * the same number, by the *sets* of digits they are written with, which is what
 * tells one script's numerals from another's. Only when they are written with
 * the same digits as well is there no difference at all, and the comparison
 * carries on along the string - which is what makes "a1b" sort before "a1c".
 * Both differences are noted as the two runs are walked, so the tie-break costs
 * no second pass.
 *
 * One consequence, which a caller must know before using this as a comparator:
 * text mixing digits from more than one script is not totally ordered by it, and
 * cannot be while a digit's value and its position in the character set
 * disagree. "9" < "a" < the Arabic-Indic five, but 9 > 5, so the three are not
 * in a line. Text whose digits are all from one script - including plain ASCII -
 * is ordered strictly and consistently, which was checked exhaustively over
 * small alphabets. Nothing in this library compares with it: compare() is what
 * the containers use, and it never reaches this.
 *
 * The characters are read through the class's own accessors. This method is
 * const, so the bookmark the string keeps for its own repeated accesses is not
 * this walk's to move; a local pair does the same job for a walk that only goes
 * forwards.
 */
template<typename Index>
int StrValI<Index>::compareNatural(const StrValI& comparand) const
{
	if (body->isRawBinary() != comparand.body->isRawBinary())
		return asText().compareNatural(comparand.asText());	// One encoding or the other

	Index		len1 = length();
	Index		len2 = comparand.length();
	Bookmark	mark1, mark2;
	const char*	cp1 = body->nthChar(offset, mark1);
	const char*	cp2 = comparand.body->nthChar(comparand.offset, mark2);
	const char*	ep1 = body->nthChar(offset+len1, mark1);
	const char*	ep2 = comparand.body->nthChar(comparand.offset+len2, mark2);
	while (cp1 < ep1 && cp2 < ep2)
	{
		UCS4		ch1 = getChar(cp1);	// The character, and past it
		UCS4		ch2 = comparand.getChar(cp2);
		NumericScript	set1 = ScriptNone;	// The script each was written in
		NumericScript	set2 = ScriptNone;
		int		d1 = UCS4Digit(ch1, set1);
		int		d2 = UCS4Digit(ch2, set2);

		if (d1 > 0 && d2 > 0)			// Two numbers, neither of them starting with a zero
		{
			/*
			 * Walk both numbers in step, counting their digits and noting the
			 * first place they differ - by a digit's value, or by the set of
			 * digits it is written with. Both are wanted only if the numbers
			 * turn out to be equal, but finding them here costs no second pass.
			 */
			const char*	p1 = cp1;	// Past each run's first digit
			const char*	p2 = cp2;
			Index		n1 = 1;		// Digits counted, with the first
			Index		n2 = 1;
			int		value_diff = d1 != d2 ? d1-d2 : 0;
			int		set_diff = set1 != set2 ? (int)set1-(int)set2 : 0;
			bool		go1 = true;
			bool		go2 = true;
			while (go1 || go2)
			{
				int		v1 = 0, v2 = 0;
				NumericScript	s1 = ScriptNone, s2 = ScriptNone;
				bool		read1 = false;
				bool		read2 = false;

				if (go1)
				{
					const char*	next = p1;
					if (p1 < ep1 && (v1 = UCS4Digit(getChar(next), s1)) >= 0)
					{
						p1 = next;	// The run goes on
						n1++;
						read1 = true;
					}
					else
						go1 = false;	// The run ends here
				}
				if (go2)
				{
					const char*	next = p2;
					if (p2 < ep2 && (v2 = UCS4Digit(comparand.getChar(next), s2)) >= 0)
					{
						p2 = next;
						n2++;
						read2 = true;
					}
					else
						go2 = false;
				}

				if (read1 && read2)	// These two digits are the same position
				{
					if (value_diff == 0 && v1 != v2)
						value_diff = v1-v2;
					if (set_diff == 0 && s1 != s2)
						set_diff = s1-s2;
				}
			}

			if (n1 != n2)			// Neither leads with a zero, so the longer run is the bigger number
				return n1 < n2 ? -1 : 1;
			if (value_diff != 0)		// As many digits: their values decide
				return value_diff < 0 ? -1 : 1;
			if (set_diff != 0)		// The same number, written with different digits: the sets decide
				return set_diff < 0 ? -1 : 1;

			cp1 = p1;			// The same number, written the same way: carry on
			cp2 = p2;
			continue;
		}

		/*
		 * Not both of them numbers - one is not a digit at all, or starts with
		 * a zero, so it is text - and then the characters themselves decide.
		 * As characters, not as UTF-8 bytes: for one encoding that is the same
		 * order, and for a raw-binary byte it is the code point it stands for.
		 */
		if (ch1 != ch2)
			return ch1 < ch2 ? -1 : 1;
	}
	if (cp1 < ep1)
		return 1;				// This one has characters left over
	if (cp2 < ep2)
		return -1;
	return 0;
}

// Allow ("str" + StrVal):
template<typename Index = StrValIndex> StrValI<Index> operator+(const char* cp, const StrVal s)
{
	return StrValI<Index>(cp) + s;
}

// Names of the integer widths for use in messages.
template<typename T> inline const char* strval_integer_type_name()	{ return "integer"; }
template<> inline const char* strval_integer_type_name<int8_t>()	{ return "int8_t"; }
template<> inline const char* strval_integer_type_name<int16_t>()	{ return "int16_t"; }
template<> inline const char* strval_integer_type_name<int32_t>()	{ return "int32_t"; }
template<> inline const char* strval_integer_type_name<int64_t>()	{ return "int64_t"; }
template<> inline const char* strval_integer_type_name<uint8_t>()	{ return "uint8_t"; }
template<> inline const char* strval_integer_type_name<uint16_t>()	{ return "uint16_t"; }
template<> inline const char* strval_integer_type_name<uint32_t>()	{ return "uint32_t"; }
template<> inline const char* strval_integer_type_name<uint64_t>()	{ return "uint64_t"; }

/*
 * The number recogniser: where a number is in a text, and what is wrong with it
 * if it is not one. Deliberately width-free - it says where the digits are, and
 * not what they are worth - so that one scan serves every reader, and only the
 * readers have to know a type.
 */
template<typename Index>
typename StrValI<Index>::NumberScan
StrValI<Index>::scanNumber(const StrValI<Index>& text, int radix)
{
	NumberScan	scan;
	Index		len = text.length();
	Index		i = 0;
	UCS4		ch = 0;

	scan.why = 0;
	scan.at = 0;
	scan.radix = radix;
	scan.negative = false;
	scan.digits = 0;
	scan.digits_end = 0;
	scan.fraction = 0;
	scan.fraction_end = 0;

	if (radix < 0 || radix > 36)
	{
		scan.why = STRERR_ILLEGAL_RADIX;
		return scan;			// Nothing was read, so there is no offset to give
	}

	while (i < len && UCS4IsWhite(ch = text[i]))
		i++;
	if (i == len)
	{
		scan.why = STRERR_NO_DIGITS;
		scan.at = i;
		return scan;
	}

	if (ch == '+' || ch == '-')
	{
		scan.negative = ch == '-';
		i++;
		while (i < len && UCS4IsWhite(ch = text[i]))
			i++;
		if (i == len)
		{
			scan.why = STRERR_NO_DIGITS;
			scan.at = i;
			return scan;
		}
	}

	/*
	 * Detect the radix, as C does: a leading zero is octal, and 0b or 0x name
	 * the radix themselves. A prefix is stripped when the radix was not given,
	 * and also when a radix of 2 or 16 was given, which C allows as well - so
	 * that "0xff" read in radix 16 is 255, and not 0 followed by "xff". For
	 * any other radix a prefix is not one, and b or x is a digit if the radix
	 * has such a digit.
	 */
	int	given = radix;			// What the caller asked for; 0 means detect it

	if (given == 0)
		radix = (UCS4Digit(ch) == 0 && i+1 < len) ? 8 : 10;

	if (UCS4Digit(ch) == 0 && i+1 < len
	 && ((text[i+1] == 'b' || text[i+1] == 'B') ? (given == 0 || given == 2)
	  :  (text[i+1] == 'x' || text[i+1] == 'X') ? (given == 0 || given == 16)
	  :  false))
	{
		radix = (text[i+1] == 'b' || text[i+1] == 'B') ? 2 : 16;
		ch = text[i += 2];
		if (i == len)
		{
			scan.why = STRERR_NO_DIGITS;
			scan.at = i;
			return scan;
		}
	}
	scan.radix = radix;

	if (Digit(ch, radix) < 0 && ch != '.' && ch != ',')
	{
		scan.why = STRERR_NOT_NUMBER;
		scan.at = i;
		return scan;
	}

	scan.digits = i;
	while (i < len && Digit(text[i], radix) >= 0)
		i++;
	scan.digits_end = i;

	// The radix point and the digits after it, if the text writes any. A text
	// with a point and no digits before it is a fraction alone, which a reader
	// that wants a fraction can read and one that wants a whole number cannot.
	scan.fraction = i;
	if (i < len && (text[i] == '.' || text[i] == ','))
	{
		i++;
		scan.fraction = i;
		while (i < len && Digit(text[i], radix) >= 0)
			i++;
	}
	scan.fraction_end = i;

	return scan;
}

/*
 * The digits a scan found, read into T at the caller's resolution. The value
 * grows only while it fits the type, so nothing wraps and nothing is rounded:
 * a digit that would not fit is where the number stops, and everything from
 * there on is trailing text, which is reported and then left to the caller.
 *
 * The caller decides what that report means, and it differs by domain: a place
 * beyond the resolution of a time is a rounding, so the time layer rolls the
 * report back and keeps what fitted; a place beyond cents is a mistake, so a
 * caller reading money leaves it standing. Nothing is decided here.
 */
template<typename Index>
template<typename T>
T
StrValI<Index>::readNumber(const NumberScan& scan, int places, ErrNum* err_return, Index* scanned) const
{
	uint64_t	limit;			// The largest magnitude this width holds
	uint64_t	scale = 1;		// What one place after the point is worth
	uint64_t	value = 0;
	Index		len = length();
	Index		i;
	Index		stop;
	Index		f;
	bool		overflowed = false;	// Too many digits for the type
	bool		trailed = false;	// Precision lost, or characters not consumed
	ErrNum		why = 0;

	if (err_return)
		*err_return = 0;

	if (scan.why)			// No number was recognised: say why, and return 0
	{
		reportNumber(scan.why, strval_integer_type_name<T>(), *this, scan.radix, scan.at, scan.at);
		if (err_return)
			*err_return = scan.why;
		if (scanned)
			*scanned = scan.at;
		return (T)0;
	}

	limit = (uint64_t)std::numeric_limits<T>::max();
	if (scan.negative && std::numeric_limits<T>::is_signed)
		limit++;		// The most negative value has no positive counterpart

	{
		/*
		 * A resolution the type cannot hold at all is the caller's fault and
		 * not the text's: no number, however small, could be read at it. It
		 * is caught rather than returned, a wrong number being what it would be.
		 */
		uint64_t	scale = 1;

		for (int place = 0; place < places; place++)
		{
			StrppAssert(scale <= limit/(uint64_t)scan.radix);
			scale *= scan.radix;
		}
	}

	/*
	 * A text with a point and no digits either side of it is not a number at
	 * all. One with a point and no digits *before* it is a fraction, which a
	 * reader that wants a fraction can read and one that wants a whole number
	 * cannot - so that is this reader's complaint, and the fraction reader's
	 * is nothing.
	 */
	if (scan.digits_end == scan.digits
	 && (places == 0 || scan.fraction_end == scan.fraction))
	{
		reportNumber(STRERR_NOT_NUMBER, strval_integer_type_name<T>(), *this, scan.radix, scan.at, scan.at);
		if (err_return)
			*err_return = STRERR_NOT_NUMBER;
		if (scanned)
			*scanned = scan.at;
		return (T)0;
	}

	/*
	 * A number with a minus sign cannot be read into a type with no sign at
	 * all: it is not that the value is too large for the width, it is that the
	 * sign has no meaning there. Refused rather than wrapped as C's strtoul
	 * wraps it, a bit pattern being asked for by reading it in a base.
	 */
	if (scan.negative && !std::numeric_limits<T>::is_signed)
	{
		reportNumber(STRERR_NEGATIVE_UNSIGNED, strval_integer_type_name<T>(), *this,
				scan.radix, scan.digits, scan.digits);
		if (err_return)
			*err_return = STRERR_NEGATIVE_UNSIGNED;
		if (scanned)
			*scanned = 0;		// Nothing of the number was read
		return (T)0;
	}

	// The whole part, a digit at a time, while it fits
	stop = scan.digits;
	for (i = scan.digits; i < scan.digits_end; i++)
	{
		unsigned	d = (unsigned)Digit((*this)[i], scan.radix);

		if (value > (limit - d)/scan.radix)
		{
			overflowed = true;	// Too many digits for the type: a different number
			break;
		}
		value = value*scan.radix + d;
		stop = i+1;
	}

	// The places after the point, to the resolution asked for: a place the text
	// did not write is a zero, and places it wrote beyond that are trailing text
	if (!overflowed)
	{
		f = scan.fraction;
		for (int place = 0; place < places; place++)
		{
			unsigned	d = 0;

			if (f < scan.fraction_end)
				d = (unsigned)Digit((*this)[f], scan.radix);
			if (value > (limit - d)/scan.radix)
			{
				overflowed = true;	// The value, at this resolution, is too large
				break;
			}
			value = value*scan.radix + d;
			if (f < scan.fraction_end && ++f > stop)
				stop = f;	// The number ends after this digit
		}
		if (!overflowed && f < scan.fraction_end)
			trailed = true;		// More places were written than were asked for
	}

	// Anything after where the number stopped is trailing text, blank or not
	i = stop;
	while (i < len && UCS4IsWhite((*this)[i]))
		i++;
	if (!overflowed && !trailed && i != len)
		trailed = true;

	/*
	 * Two failures, and they are not the same complaint. Digits the type cannot
	 * hold are an overflow: the number is not this number, and a caller should
	 * not carry on with it. Places beyond the resolution, or characters after
	 * the number, are trailing text: precision was lost or something was not
	 * consumed, which a caller whose resolution is coarser than the text may
	 * well decide to live with. Either way a value is answered.
	 */
	if (overflowed)
	{
		why = STRERR_NUMBER_OVERFLOW;
		reportNumber(why, strval_integer_type_name<T>(), *this, scan.radix, scan.at, stop);
	}
	else if (trailed)
	{
		why = STRERR_TRAIL_TEXT;
		reportNumber(why, strval_integer_type_name<T>(), *this, scan.radix, scan.at, stop);
	}
	if (err_return)
		*err_return = why;
	if (scanned)
		*scanned = i;

	return scan.negative ? (T)(0 - value) : (T)value;
}

template<typename Index>
template<typename T>
T
StrValI<Index>::asInteger(ErrNum* err_return, int radix, Index* scanned) const
{
	return readNumber<T>(scanNumber(*this, radix), 0, err_return, scanned);
}

template<typename Index>
template<typename T>
T
StrValI<Index>::asFixedPoint(int places, ErrNum* err_return, int radix, Index* scanned) const
{
	StrppAssert(places >= 0);
	return readNumber<T>(scanNumber(*this, radix), places, err_return, scanned);
}

template<typename Index>
void StrBodyI<Index>::transform(const std::function<Val(const char*& cp, const char* ep)> xform, int after)
{
	assert(ref_count <= 1);
	char*		old_start = start;
	size_t		old_num_elements = num_elements;
	bool		raw = isRawBinary();		// What this body was, and still will be

	// Allocate new data, preserving the old
	start = 0;
	num_chars = raw ? StrValIndexRawBinaryMarker : 0;	// Held throughout: reading and writing a raw body is per byte
	Index		counted = 0;			// Characters written, when this body is text
	num_elements = 0;
	num_alloc = 0;
	ArrayBody<char, Index>::resize(old_num_elements+6);		// Start with same allocation plus one character space

	const char*	up = old_start;		// Input pointer
	const char*	ep = old_start+old_num_elements-1;	// Termination guard, points to the NUL
	Index		processed_chars = 0;	// Total input chars transformed
	bool		stopped = false;	// Are we done yet?
	char*		op = start;		// Output pointer
	while (up < ep)
	{
		const char*	next = up;
		if (processed_chars+1 > after+1	// Not yet reached the point to start transforming
		 && !stopped)			// We have stopped transforming
		{
			assert(next < ep);
			StrVal		replacement = xform(next, ep);
			Index		replaced_bytes = next-up;	// How many bytes were consumed?
			Index		replaced_chars = replacement.length();	// Replaced by how many chars?

			// Advance 'up' over the replaced characters
			while (up < next)
			{
				up += raw ? 1 : UTF8Len(up);	// One byte to a character, or as UTF-8
				processed_chars++;
			}

			stopped |= (replaced_bytes == 0);

			Index		replacement_bytes;
			const char*	rp = replacement.asUTF8(replacement_bytes);
			ArrayBody<char, Index>::insert(num_elements, rp, replacement_bytes);
			counted += replacement.length();
			op = start+num_elements;
		}
		else
		{		// Just copy one character and move on
			UCS4	ch = getChar(up);	// Get UCS4 character
			processed_chars++;
			if (num_alloc < (op-start+6+1))
			{
				ArrayBody<char, Index>::resize((op-start)+6+1);	// Room for any char and NUL
				op = start+num_elements;	// Reset our output pointer in case start has changed
			}
			putChar(op, ch);
			num_elements = op-start;
			counted++;
		}
	}
	// Append the \0 to the array:
	ArrayBody<char, Index>::insert(num_elements, "", 1);
	num_chars = raw ? StrValIndexRawBinaryMarker : counted;	// Still one byte to a character
	delete [] old_start;
}

template<typename Index>
StrValI<Index>& StrValI<Index>::transform(const std::function<StrValI(const char*& cp, const char* ep)> xform, int after)
{
	Unshare();
	body->transform(xform, after);
	num_chars = body->numChars();
	mark = Bookmark();
	return *this;
}

// asInt32 is defined in src/strval.cpp, and not here: it reports a failure
// through the message set, and reporting needs a Variant, which needs this
// header. See the comment at the head of that file.

/*
 * Represent the string as JSON, using UTF16 surrogates if necessary.
 * Does not include the enclosing double-quote characters that are part of the JSON spec.
 */
template<typename Index>
void
StrBodyI<Index>::toJSON()
{
	char		one_char[14];			// \u{12345678} or \u1234\u4321
	StrBodyI	temp_body;
	static const char hex[] = "0123456789ABCDEF";

	transform(
		[&](const char*& cp, const char* ep) -> Val
		{
			UCS4	ch = getChar(cp);
			char*	op = one_char;		// Pack it into our local buffer
			switch (ch)
			{
			case '\0':	// Null Byte
				*op++ = '\\'; *op++ = '0'; break;
			case '\"':	// Double quote
				*op++ = '\\'; *op++ = '\"'; break;
			case '\\':	// Backslash character
				*op++ = '\\'; *op++ = '\\'; break;
			case '/':	// Forward slash
				*op++ = '\\'; *op++ = '/'; break;
			case '\b':	// Backspace
				*op++ = '\\'; *op++ = 'b'; break;
			case '\f':	// Form Feed
				*op++ = '\\'; *op++ = 'f'; break;
			case '\n':	// New Line
				*op++ = '\\'; *op++ = 'n'; break;
			case '\r':	// Carriage Return
				*op++ = '\\'; *op++ = 'r'; break;
			case '\t':	// Tab
				*op++ = '\\'; *op++ = 't'; break;

			default:
				// JSON allows direct representation of any legal code point that's not
				// a control-char, \, ' or a surrogate, but we don't have to do that.
				// Here we leave valid UTF-16 characters inline, represented as UTF-8

				// REVISIT: Handle StrRawBinary data
				// if (ch >= ' ' && ch < 128)			// ASCII but not ctl
				// if (ch >= ' ' && ch < 256)			// ISO8859-1 but not ctl
				if (ch >= ' '
				 && (ch <= 0xFFFF && !UTF16IsSurrogate(ch))	// Not ctl, emoji or surrogate 
				  || (ch <= 0xFF && isRawBinary()))		// Just 8-bit
				{
					putChar(op, ch);
					break;
				}

				// Use \u1234 format, as a surrogate pair if needed, per JSON spec
				auto	u4 = [](unsigned short u, char*& op){
						*op++ = '\\';
						*op++ = 'u';
						*op++ = hex[(u>>12)&0xF];
						*op++ = hex[(u>>8)&0xF];
						*op++ = hex[(u>>4)&0xF];
						*op++ = hex[u&0xF];
					};
				if (ch <= 0xFFFF)
				{		// Character fits in one \u escape, do that
					u4(ch, op);
					break;
				}

				// We need two surrogates for Emoji's etc, which is what the
				// UTF-16 rules are: a value that is not a code point is
				// substituted there rather than made into a bogus pair
				assert(ch <= 0x10FFFF);		// The last code point there is
				UTF16	units[2];
				UTF16*	up = units;
				UTF16Put(up, ch);
				for (const UTF16* unit = units; unit < up; unit++)
					u4(*unit, op);
				break;
			}
			*op = '\0';

			// Assign this to the body in our closure
			temp_body = StrBodyI(one_char, ArrayBorrow, op-one_char);
			return Val(&temp_body);
		}
	);
}

// Generate the digits of an unsigned value, pushed backwards to the end of a buffer.
template<typename Index> template<typename U> char*
StrValI<Index>::reprDigits(U u, int base, const char* digits, char* end)
{
	char*	cp = end;

	do
	{
		*--cp = digits[u % base];
		u /= base;
	}
	while (u != 0);

	return cp;
}

/*
 * An integer as text in the named representation: decimal unless it is b, o, x
 * or X, in which case it is that base. No base carries a prefix: a text that
 * wants 0x writes it itself, which leaves it where a translator can put it.
 *
 * Decimal renders the sign, since a number written for a person is signed; the
 * digits of the most negative value, which has no positive counterpart, are
 * built from its unsigned form. The other bases render the bit pattern of the
 * value at the width of its own type, which is what a non-decimal base is for:
 * an int of -1 is eight F's, not a minus sign and one F.
 *
 * The digits are placed here rather than by printf, since a library that
 * formats its own text must not need it.
 */
template<typename Index> template<typename N> StrVal
StrValI<Index>::reprInt(N n, char repr)
{
	typedef typename std::make_unsigned<N>::type	U;
	int		base = 10;
	const char*	digits = "0123456789abcdef";

	switch (repr)
	{
	case 'b':	base = 2; break;
	case 'o':	base = 8; break;
	case 'x':	base = 16; break;
	case 'X':	base = 16; digits = "0123456789ABCDEF"; break;
	}

	bool	negative = n < 0 && base == 10;
	U	u = base == 10 && n < 0 ? 0-(U)n : (U)n;

	char	buf[72];			// Sixty-four bits of binary, a sign, and room to spare
	char*	end = buf + sizeof(buf);
	char*	cp = reprDigits(u, base, digits, end);	// In the width of the value's own type

	if (negative)
		*--cp = '-';

	return StrVal(cp, (StrValIndex)(end-cp));
}

// An unsigned integer type as text, in the requested representation
template<typename Index> template<typename N> StrVal
StrValI<Index>::reprUInt(N u, char repr)
{
	int		base = 10;
	const char*	digits = "0123456789abcdef";

	switch (repr)
	{
	case 'b':	base = 2; break;
	case 'o':	base = 8; break;
	case 'x':	base = 16; break;
	case 'X':	base = 16; digits = "0123456789ABCDEF"; break;
	}

	char	buf[72];			// Sixty-four bits of binary, and room to spare
	char*	end = buf + sizeof(buf);
	char*	cp = reprDigits(u, base, digits, end);	// In the width of the value's own type

	return StrVal(cp, (StrValIndex)(end-cp));
}

class	StringArray
: public Array<StrRef>
{
	using	Base = Array<StrRef>;
public:
	StringArray() {}
	StringArray(const Base& a1) : Base(a1) {}
	StringArray(const StrVal* data, Index size, Index allocate = 0)
	: Array(data, size, allocate)
	{
	}

	// Construct from an Array of const char*
	StringArray(const Array<const char*> strings)
	: Array((const StrVal*)0, 0, strings.length())
	{
		strings.each([&](const char* s) { append(s); });
	}

	StrVal		join(StrVal joiner) const
	{
		if (length() == 0)
			return "";

		StrVal		e0 = elem(0);
		StrValIndex	result_length = e0.length();
		for (int i = 1; i < length(); i++)
			result_length += joiner.length() + elem(i).length();

		// Preallocate the required amount of storage:
		StrVal	joined(e0.asUTF8(), e0.numBytes(), result_length+1);
		for (int i = 1; i < length(); i++)
			joined += joiner + StrVal(elem(i));
		return joined;
	}

	StrVal		operator[](int elem_num) const
			{ return Base::operator[](elem_num); }
	StrVal		elem(int elem_num) const					// Returns a copy
			{ return Base::operator[](elem_num); }
	StrVal		last()
			{ return Base::last(); }
	Element		pull()
			{ return Base::pull(); }
	StrVal		delete_at(Index at)
			{ return Base::delete_at(at); }
protected:	// These cannot be implemented using a StrVal return type. You can still use them if you handle StrRef
	// Element&	elem_mut(int elem_num)		// Return a mutable element
	// const Element&	elem_ref(int elem_num) const
	// const Element*	asElements() const
	// const Element&	set(int elem_num, const Element& e)
	// Element&	last_mut()
	// Element		shift()				// remove an element from the start

protected:
	StringArray(Body* body, Index offs, Index len)	// offs/len not bounds-checked!
			: Base(body, offs, len) {}
};

template<typename Index> inline StrVal
StrValI<Index>::fromInt32(int32_t n, char repr)			{ return reprInt(n, repr); }
template<typename Index> inline StrVal
StrValI<Index>::fromUInt32(uint32_t n, char repr)		{ return reprUInt(n, repr); }
template<typename Index> inline StrVal
StrValI<Index>::fromLong(long n, char repr)			{ return reprInt(n, repr); }
template<typename Index> inline StrVal
StrValI<Index>::fromULong(unsigned long n, char repr)		{ return reprUInt(n, repr); }
template<typename Index> inline StrVal
StrValI<Index>::fromInt64(int64_t n, char repr)			{ return reprInt(n, repr); }
template<typename Index> inline StrVal
StrValI<Index>::fromUInt64(uint64_t n, char repr)		{ return reprUInt(n, repr); }

#include <unistd.h>
inline void p(StrVal s) { char const*cp = s.asUTF8(); write(1, "\"", 1); write(1, cp, strlen(cp)); write(1, "\"\n", 2); }

#endif
