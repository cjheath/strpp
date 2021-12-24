/*
 * String Value library.
 * - By-value semantics with mutation
 * - Thread-safe content sharing and garbage collection using atomic reference counting
 * - Substring support using "slices" (substrings using shared content)
 * - Unicode support using UTF-8
 * - Character indexing, not byte offsets
 * - Efficient forward and backward scanning using bookmarks to assist
 *
 * A shared string is never mutated. Mutation is allowed when only one reference exists.
 * Mutating methods clone (unshare) a shared string before making changes.
 */
#include	<strpp.h>

#include	<string.h>
#include	<limits.h>
#include	<stdio.h>

StrBody		StrBody::nullBody("", false, 0, 0);
const StrVal	StrVal::null;

// Empty string
StrVal::StrVal()
: body(&StrBody::nullBody)
, offset(0)
, num_chars(0)
{
}

// Normal copy constructor
StrVal::StrVal(const StrVal& s1)
: body(s1.body)
, offset(s1.offset)
, num_chars(s1.num_chars)
{
}

// A new reference to the same StrBody
StrVal::StrVal(StrBody* s1)
: body(s1)
, offset(0)
, num_chars(s1->numChars())
{
}

StrVal::StrVal(StrBody* s1, CharNum offs, CharNum len)
: body(s1)
, offset(offs)
, num_chars(len)
{
}

// Assignment operator
StrVal& StrVal::operator=(const StrVal& s1)
{
	body = s1.body;
	offset = s1.offset;
	num_chars = s1.num_chars;
	return *this;
}

// Null-terminated UTF8 data
// Don't allocate a new StrBody for an empty string
StrVal::StrVal(const UTF8* data)
: body(data == 0 || data[0] == '\0' ? &StrBody::nullBody : new StrBody(data, true, strlen((const char*)data)))
, offset(0)
, num_chars(body->numChars())
{
}

// Length-terminated UTF8 data
StrVal::StrVal(const UTF8* data, CharBytes length, size_t allocate)
: body(0)
, offset(0)
, num_chars(0)
, mark()
{
	if (allocate < length)
		allocate = length;
	body = new StrBody(data, length, allocate);
	num_chars = body->numChars();
}

// Single-character string
StrVal::StrVal(UCS4 character)
: body(0)
, offset(0)
, num_chars(0)
{
	UTF8	one_char[7];
	UTF8*	op = one_char;		// Pack it into our local buffer
	UTF8Put(op, character);
	*op = '\0';
	body = new StrBody(one_char, true, op-one_char, 1);
	num_chars = 1;
}

// Get our own copy of StrBody that we can safely mutate
void
StrVal::Unshare()
{
	if (body->GetRefCount() <= 1)
		return;

	// Copy only this slice of the body's data, and reset our offset to zero
	Bookmark	savemark(mark);			// copy the bookmark
	const UTF8*	cp = nthChar(0);		// start of this substring
	const UTF8*	ep = nthChar(num_chars);	// end of this substring
	CharBytes	prefix_bytes = cp - body->startChar(); // How many leading bytes of the body we are eliding

	body = new StrBody(cp, true, ep-cp, 0);
	mark.char_num = savemark.char_num - offset;	// Restore the bookmark
	mark.byte_num = savemark.byte_num - prefix_bytes;
	offset = 0;
}

// Access characters:
UCS4
StrVal::operator[](int charNum) const
{
	const UTF8*	cp = nthChar(charNum);
	return cp ? UTF8Get(cp) : UCS4_NONE;
}

const UTF8*
StrVal::asUTF8()	// Must unshare data if it's a substring with elided suffix
{
	const	UTF8*	ep = nthChar(num_chars);
	if (ep < body->endChar())
	{
		Unshare();
		// If we are the last reference and a substring, we might not be terminated
		*body->nthChar(num_chars, mark) = '\0';
	}
	return nthChar(0);
}

const UTF8*
StrVal::asUTF8(CharBytes& bytes) const
{
	const	UTF8*	cp = nthChar(0);
	const	UTF8*	ep = nthChar(num_chars);
	bytes = ep-cp;
	return cp;
}

// Compare strings using different styles
int
StrVal::compare(const StrVal& comparand, CompareStyle style) const
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


// Delete a substring from the middle:
void
StrVal::remove(CharNum at, int len)
{
	if (at >= num_chars || len == 0)
		return; // Nothing to do
	Unshare();
	body->remove(at, len);
}

StrVal
StrVal::substr(CharNum at, int len) const
{
	assert(len >= -1);

	// Quick check for a null substring:
	if (at < 0 || at >= num_chars || len == 0)
		return StrVal();

	// Clamp substring length:
	if (len == -1)
		len = num_chars-at;
	else if (len > num_chars-at)
		len = num_chars-at;

	return StrVal(body, offset+at, len);
}

int
StrVal::find(UCS4 ch, int after) const
{
	CharNum		n = after+1;		// First CharNum we'll look at
	const UTF8*	up;
	while ((up = nthChar(n)) != 0)
	{
		if (ch == UTF8Get(up))
			return n;		// Found at n
		n++;
	}

	return -1;				// Not found
}

int
StrVal::rfind(UCS4 ch, int before) const
{
	CharNum		n = before == -1 ? num_chars-1 : before-1;		// First CharNum we'll look at
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
int
StrVal::find(const StrVal& s1, int after) const
{
	CharNum		n = after+1;		// First CharNum we'll look at
	CharNum		last_start = num_chars-s1.length();	// Last possible start position
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

int
StrVal::rfind(const StrVal& s1, int before) const
{
	CharNum		n = before == -1 ? num_chars-s1.length() : before-1;	// First CharNum we'll look at
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
int
StrVal::findAny(const StrVal& s1, int after) const
{
	CharNum		n = after+1;		// First CharNum we'll look at
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

int
StrVal::rfindAny(const StrVal& s1, int before) const
{
	CharNum		n = before == -1 ? num_chars-1 : before-1;	// First CharNum we'll look at
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
int
StrVal::findNot(const StrVal& s1, int after) const
{
	CharNum		n = after+1;		// First CharNum we'll look at
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

int
StrVal::rfindNot(const StrVal& s1, int before) const
{
	CharNum		n = before == -1 ? num_chars-1 : before-1;	// First CharNum we'll look at
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

// Add, producing a new StrVal:
StrVal
StrVal::operator+(const StrVal& addend) const
{
	// Handle the rare but important case of extending a slice with a contiguous slice of the same body
	if ((StrBody*)body == (StrBody*)addend.body	// From the same body
	 && offset+num_chars == addend.offset)	// And this ends where the addend starts
		return StrVal(body, offset, num_chars+addend.num_chars);

	const UTF8*	cp = nthChar(0);
	CharBytes	len = numBytes();
	StrVal		str(cp, len, len+addend.numBytes());

	str += addend;
	return str;
}

StrVal
StrVal::operator+(UCS4 addend) const
{
	// Convert addend using a stack-local buffer to save allocation here.
	UTF8	buf[7];				// Enough for 6-byte content plus a NUL
	UTF8*	cp = buf;
	UTF8Put(cp, addend);
	*cp = '\0';
	StrBody	body(buf, false, cp-buf, 1);

	return operator+(StrVal(&body));
}

// Add, StrVal is modified:
StrVal&
StrVal::operator+=(const StrVal& addend)
{
	if (num_chars == 0 && !addend.noCopy())
		return *this = addend;		// Just assign, we were empty anyhow

	append(addend);
	return *this;
}

StrVal&
StrVal::operator+=(UCS4 addend)
{
	// Convert addend using a stack-local buffer to save allocation here.
	UTF8	buf[7];				// Enough for 6-byte content plus a NUL
	UTF8*	cp = buf;
	UTF8Put(cp, addend);
	*cp = '\0';
	StrBody	body(buf, false, cp-buf, 1);

	operator+=(StrVal(&body));
	return *this;
}

void
StrVal::insert(CharNum pos, const StrVal& addend)
{
	// Handle the rare but important case of extending a slice with a contiguous slice of the same body
	if (pos == num_chars			// Appending at the end
	 && (StrBody*)body == (StrBody*)addend.body	// From the same body
	 && offset+pos == addend.offset)	// And addend starts where we end
	{
		num_chars += addend.length();
		return;
	}

	Unshare();
	body->insert(pos, addend);
	num_chars += addend.length();
}

void
StrVal::toLower()
{
	Unshare();
	body->toLower();
	num_chars = body->numChars();
}

void
StrVal::toUpper()
{
	Unshare();
	body->toUpper();
	num_chars = body->numChars();
}

static inline int
HexAlpha(UCS4 ch)
{
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z')
		? (int)((ch & ~('a'-'A')) - 'A' + 10)
		: -1;
}

static inline int
Digit(UCS4 ch, int radix)
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

int32_t
StrVal::asInt32(
	int*	err_return,	// error return
	int	radix,		// base for conversion
	CharNum* scanned	// characters scanned
) const
{
	size_t		len = length();		// length of string
	size_t		i = 0;			// position of next character
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
			*err_return = STRERR_ILLEGAL_RADIX;
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
				*err_return = STRERR_NUMBER_OVERFLOW;
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
		*err_return = STRERR_TRAIL_TEXT;

	// Return number of digits scanned
	if (scanned)
		*scanned = i;

	if (l > (unsigned long)LONG_MAX+(negative ? 1 : 0))
	{
		if (err_return)
			*err_return = STRERR_NUMBER_OVERFLOW;
		// Try anyway, they might have wanted unsigned!
	}

	/*
	 * Casting unsigned long down to long doesn't clear the high bit
	 * on a twos-complement architecture:
	 */
	return negative ? -(long)l : (long)l;

no_digits:
	if (err_return)
		*err_return = STRERR_NO_DIGITS;
	if (scanned)
		*scanned = i;
	return 0;

not_number:
	if (err_return)
		*err_return = STRERR_NOT_NUMBER;
	if (scanned)
		*scanned = i;
	return 0;
}

// Return a pointer to the start of the nth character, using our bookmark to help
const UTF8*
StrVal::nthChar(CharNum char_num)
{
	if (char_num < 0 || char_num > num_chars)
		return 0;
	return body->nthChar(offset+char_num, mark);
}

// Return a pointer to the start of the nth character, without using a bookmark
const UTF8*
StrVal::nthChar(CharNum char_num) const
{
	if (char_num < 0 || char_num > num_chars)
		return 0;
	Bookmark	useless;
	return body->nthChar(offset+char_num, useless);
}

bool
StrVal::noCopy() const
{
	return body->noCopy();
}

StrBody::~StrBody()
{
	if (start && num_alloc > 0)
		delete[] start;
}

StrBody::StrBody()
: start(0)
, num_chars(0)
, num_bytes(0)
, num_alloc(0)
{
}

StrBody::StrBody(const UTF8* data, bool copy, CharBytes length, size_t allocate)
: start(0)
, num_chars(0)
, num_bytes(0)
, num_alloc(0)
{
	if (copy)
	{
		if (allocate < length)
			allocate = length;
		resize(allocate+1);	// Include space for a trailing NUL
		if (length)
			memcpy(start, data, length);
		start[length] = '\0';
	}
	else
	{
		start = (UTF8*)data;	// Cast const away; copy==false implies no change will occur
		if (length == 0)
			length = strlen(data);
		AddRef();		// Cannot be deleted or resized
	}
	num_bytes = length;
}

StrBody&
StrBody::operator=(const StrBody& s1) // Assignment operator; ONLY for no-copy bodies
{
	assert(s1.num_alloc == 0);	// Must not do this if we would make two references to allocated data
	start = s1.start;
	num_chars = s1.num_chars;
	num_bytes = s1.num_bytes;
	num_alloc = 0;
	return *this;
}

/*
 * Counting the characters in a string also checks for valid UTF-8.
 * If the string is allocated (not static) but unshared,
 * it also compacts non-minimal UTF-8 coding.
 */
void
StrBody::countChars()
{
	if (num_chars > 0 || num_bytes == 0)
		return;
	const UTF8*	cp = start;		// Progress pointer when reading data
	UTF8*		op = start;		// Output for compacted data
	UTF8*		ep = start+num_bytes;	// Marker for end of data
	while (cp < ep)
	{
		const UTF8*	char_start = cp; // Save the start of this character, for rewriting
		UCS4		ch = UTF8Get(cp);

		// We don't hold by Postel’s Law here. We really should throw an exception, but I'm feeling kind. N'yah!
		if (ch == UCS4_NONE		// Illegal encoding
		 || cp > ep)			// Overlaps the end of data
			break;			// An error occurred before the end of the data; truncate it.

		if (num_alloc > 0		// Not a static string, so we can rewrite it
		 && ref_count <= 1		// Not currently shared, so not breaking any Bookmarks
		 && (UTF8Len(ch) < cp-char_start // Optimal length is shorter than this encoding. Start compacting
		     || op < char_start))	// We were already compacting
			UTF8Put(op, ch);	// Rewrite the character in compact form
		else
			op = (UTF8*)cp;		// Keep up
		num_chars++;
	}
	if (op < cp)
	{			// We were rewriting, or there was a coding eror, so we now have fewer bytes
		num_bytes = op-start;
		if (num_alloc > 0)
			*op = '\0';
	}
}

// Resize the memory allocation to make room for a larger string (size includes the trailing NUL)
void
StrBody::resize(size_t minimum)
{
	if (minimum <= num_alloc)
		return;

	minimum = ((minimum-1)|0x7)+1;	// round up to multiple of 8
	if (num_alloc)	// Minimum growth 50% rounded up to nearest 32
		num_alloc = ((num_alloc*3/2) | 0x1F) + 1;
	if (num_alloc < minimum)
		num_alloc = minimum;		// Still not enough, get enough
	UTF8*	newdata = new UTF8[num_alloc];
	if (start)
	{
		memcpy(newdata, start, num_bytes);
		newdata[num_bytes] = '\0';
		delete[] start;
	}
	start = newdata;
}

// Return a pointer to the start of the nth character
UTF8*
StrBody::nthChar(CharNum char_num, StrVal::Bookmark& mark)
{
	UTF8*		up;		// starting pointer for forward search
	int		start_char;	// starting char number for forward search
	UTF8*		ep;		// starting pointer for backward search
	int		end_char;	// starting char number for backward search

	if (char_num < 0 || char_num > (end_char = numChars())) // numChars() counts the string if necessary
		return (UTF8*)0;

	if (num_chars == num_bytes) // ASCII data only, use direct index!
		return start+char_num;

	up = start;		// Set initial starting point for forward search
	start_char = 0;
	ep = start+num_bytes;	// and for backward search (end_char is set above)

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

void
StrBody::insert(CharNum pos, const StrVal& addend)
{
	CharBytes	addend_bytes;
	const UTF8*	addend_data = addend.asUTF8(addend_bytes);
	resize(num_bytes + addend_bytes + 1);	// Include space for a trailing NUL

	StrVal::Bookmark nullmark;
	UTF8*		insert_position = nthChar(pos, nullmark);
	CharBytes	unmoved = insert_position-start;

	// Move data up, including the trailing NUL
	memmove(insert_position+addend_bytes, insert_position, num_bytes-unmoved+1);
	memcpy(insert_position, addend_data, addend_bytes);
	num_bytes += addend_bytes;
	num_chars += addend.length();
}

// Delete a substring from the middle
void
StrBody::remove(CharNum at, int len)
{
	assert(ref_count <= 1);
	assert(len >= -1);

	StrVal::Bookmark nullmark;
	UTF8*		cp = (UTF8*)nthChar(at, nullmark);	// Cast away the const
	const UTF8*	ep = (len == -1 ? start + num_bytes : nthChar(at + len, nullmark));

	if (start+num_bytes+1 > ep)		// Move trailing data down. include the '\0'!
		memmove(cp, ep, start+num_bytes-ep+1);
	num_bytes -= ep-cp;
	num_chars -= len;	// len says how many we deleted.
}

void
StrBody::toLower()
{
	UTF8	one_char[7];
	StrBody	body;	// Any StrVal that references this must have a shorter lifetime. transform() guarantees this.
	transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4	ch = UTF8Get(cp);	// Get UCS4 character
			ch = UCS4ToLower(ch);		// Transform it
			UTF8*	op = one_char;		// Pack it into our local buffer
			UTF8Put(op, ch);
			*op = '\0';
			// Assign this to the body in our closure
			body = StrBody(one_char, false, op-one_char, 1);
			return StrVal(&body);
		}
	);
}

void
StrBody::toUpper()
{
	UTF8	one_char[7];
	StrBody	body;	// Any StrVal that references this must have a shorter lifetime. transform() guarantees this.
	transform(
		[&](const UTF8*& cp, const UTF8* ep) -> StrVal
		{
			UCS4	ch = UTF8Get(cp);	// Get UCS4 character
			ch = UCS4ToUpper(ch);		// Transform it
			UTF8*	op = one_char;		// Pack it into our local buffer
			UTF8Put(op, ch);
			*op = '\0';

			// Assign this to the body in our closure
			body = StrBody(one_char, false, op-one_char, 1);
			return StrVal(&body);
		}
	);
}

void
StrVal::transform(const std::function<StrVal(const UTF8*& cp, const UTF8* ep)> xform, int after)
{
	Unshare();
	body->transform(xform, after);
	num_chars = body->numChars();
	mark = Bookmark();
}

/*
 * At every character position after the given point, the passed transform function
 * can extract any number of chars (limited by ep) and return a replacement StrVal for those chars.
 * To leave the remainder untransformed, return without advancing cp
 * (but a returned StrVal will still be inserted)
 */
void
StrBody::transform(const std::function<StrVal(const UTF8*& cp, const UTF8* ep)> xform, int after)
{
	assert(ref_count <= 1);
	UTF8*		old_start = start;
	size_t		old_num_bytes = num_bytes;

	// Allocate new data, preserving the old
	start = 0;
	num_chars = 0;
	num_bytes = 0;
	num_alloc = 0;
	resize(old_num_bytes+6+1);		// Start with same allocation plus one character space and NUL

	const UTF8*	up = old_start;		// Input pointer
	const UTF8*	ep = old_start+old_num_bytes;	// Termination guard
	CharNum		processed_chars = 0;	// Total input chars transformed
	bool		stopped = false;	// Are we done yet?
	UTF8*		op = start;		// Output pointer
	while (up < ep)
	{
		const UTF8*	next = up;
		if (processed_chars+1 > after+1	// Not yet reached the point to start transforming
		 && !stopped)			// We have stopped transforming
		{
			StrVal		replacement = xform(next, ep);
			CharBytes	replaced_bytes = next-up;	// How many bytes were consumed?
			CharNum		replaced_chars = replacement.length();	// Replaced by how many chars?

			// Advance 'up' over the replaced characters
			while (up < next)
			{
				up += UTF8Len(up);
				processed_chars++;
			}

			stopped |= (replaced_bytes == 0);

			insert(num_chars, replacement);
			op = start+num_bytes;
		}
		else
		{		// Just copy one character and move on
			UCS4	ch = UTF8Get(up);	// Get UCS4 character
			processed_chars++;
			if (num_alloc < (op-start+6+1))
			{
				resize((op-start)+6+1);	// Room for any char and NUL
				op = start+num_bytes;	// Reset our output pointer in case start has changed
			}
			UTF8Put(op, ch);
			num_bytes = op-start;
			num_chars++;
		}
	}
	delete [] old_start;
}
