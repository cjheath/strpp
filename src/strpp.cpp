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

#include	<stdio.h>

StrBody	StrVal::nullBody;

// Empty string
StrVal::StrVal()
: body(&nullBody)
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
: body(data && data[0] != '\0' ? new StrBody(data) : &nullBody)
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
: body(new StrBody((char*)0, 0, UTF8Len(character)))
{
	int	length = UTF8Len(character);
	UTF8*	cp = body->data();
	UTF8Put(cp, character);
	body->setLength(1, length);
	UTF8Put(cp, 0);
	num_chars = 1;
}

StrVal
StrVal::Static(const UTF8* data)	// A reference to a static C string, which will never change or be deleted
{
	return StrVal(StrBody::Static(data));
}

// Get our own copy of StrBody that we can safely mutate
void
StrVal::Unshare()
{
	if (body->GetRefCount() > 1)
	{
		// Copy only this slice of the body's data, and reset our offset to zero
		Bookmark	savemark(mark);
		const UTF8*	cp = nthChar(0);
		const UTF8*	ep = nthChar(num_chars);
		CharBytes	prefix_bytes = cp - body->data(); // How many leading bytes we are eliding

		body = new StrBody(cp, ep-cp);
		mark.char_num = savemark.char_num - offset;	// Restore the bookmark
		mark.byte_num = savemark.byte_num - prefix_bytes;
		offset = 0;
	}
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
		Unshare();
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
	// printf("Comparing %s with %s\n", nthChar(0), comparand.nthChar(0));
	switch (style)
	{
	case CompareRaw:
		return memcmp(nthChar(0), comparand.nthChar(0), body->numBytes());

	case CompareCI:
		assert(!"REVISIT: Not implemented");

	case CompareNatural:
		assert(!"REVISIT: Not implemented");

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
		return StrVal(&nullBody);

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
	UTF8	buf[7];				// Enough for 6-byte content plus a null
	UTF8*	cp = buf;
	UTF8Put(cp, addend);
	*cp = '\0';

	return operator+(StrVal::Static(buf));	// The StrVal constructed here will be destroyed without de-allocation
}

// Add, StrVal is modified:
StrVal&
StrVal::operator+=(const StrVal& addend)
{
	append(addend);
	return *this;
}

StrVal&
StrVal::operator+=(UCS4 addend)
{
	// Convert addend using a stack-local buffer to save allocation here.
	UTF8	buf[7];				// Enough for 6-byte content plus a null
	UTF8*	cp = buf;
	UTF8Put(cp, addend);
	*cp = '\0';

	operator+=(StrVal::Static(buf));	// The StrVal constructed here will be destroyed without de-allocation
	return *this;
}

void
StrVal::insert(CharNum pos, const StrVal& addend)
{
	Unshare();

	body->insert(pos, addend);
}

void
StrBody::insert(CharNum pos, const StrVal& addend)
{
	CharBytes	addend_bytes;
	const UTF8*	addend_data = addend.asUTF8(addend_bytes);
	resize(num_bytes + addend_bytes);

	StrVal::Bookmark nullmark;
	UTF8*		insert_position = nthChar(pos, nullmark);
	CharBytes	unmoved = insert_position-start;

	// Move data up, including the trailing NUL
	memmove(insert_position+addend_bytes, insert_position, num_bytes-unmoved+1);
	memcpy(insert_position, addend_data, addend_bytes);
	num_bytes += addend_bytes;
	num_chars += addend.length();
}

#if 0
StrVal
StrVal::substitute(
	StrVal		from_str,
	StrVal		to_str,
	bool		do_all		= true,
	int		after		= -1,
	bool		ignore_case	= false
) const
{
}

StrVal
StrVal::asLower() const
{
}

StrVal
StrVal::asUpper() const
{
}

void
StrVal::toLower()
{
}

void
StrVal::toUpper()
{
}

int32_t
StrVal::asInt32(
//	ErrorT* err_return = 0, // error return
	int	radix = 0,	// base for conversion
	CharNum* scanned = 0	// characters scanned
) const
{
}

int64_t
StrVal::asInt64(
//	ErrorT* err_return = 0, // error return
	int	radix = 0,	// base for conversion
	CharNum* scanned = 0	// characters scanned
) const
{
}

// // Like printf, but type-safe
// static StrVal	StrVal::format(StrVal fmt, const ArgListC)
// {
// }
//
#endif

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

// Return a pointer to the start of the nth character
UTF8*
StrBody::nthChar(CharNum char_num, StrVal::Bookmark& mark)
{
	UTF8*		up;		// starting pointer for forward search
	int		start_char;	// starting char number for forward search
	UTF8*		ep;		// starting pointer for backward search
	int		end_char;	// starting char number for backward search

	if (char_num < 0 || char_num > (end_char = numChars()))	// numChars() counts the string if necessary
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
		end_char = char_num-start_char;	// How far forward should we search?
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

StrBody::~StrBody()
{
	if (start && num_alloc > 0)
		delete[] start;
}

// null string constructor. Give it a self-reference so we don't ever try to delete it.
StrBody::StrBody()
: start((UTF8*)"")
, num_chars(0)
, num_bytes(0)
, num_alloc(0)
{
	AddRef();
}

StrBody::StrBody(const UTF8* data)
: start(0)
, num_chars(0)
, num_bytes(0)
, num_alloc(0)
{
	size_t	length = strlen((const char*)data);
	resize(length+1);	// Include space for a trailing NUL
	memcpy(start, data, length);
	start[length] = '\0';
	num_bytes = length;
}

StrBody::StrBody(const UTF8* data, CharBytes length, size_t allocate)
: start(0)
, num_chars(0)
, num_bytes(0)
, num_alloc(0)
{
	if (allocate < length)
		allocate = length;
	resize(allocate+1);	// Include space for a trailing NUL
	if (length)
		memcpy(start, data, length);
	start[length] = '\0';
	num_bytes = length;
}

StrBody*
StrBody::Static(const UTF8* data)
{
	StrBody*	str = new StrBody();	// A null string, with an extra reference
	str->start = (UTF8*)data;	// Cast away const. We won't ever change or delete this data
	str->num_bytes = strlen(data);
	return str;
}

// Return the next UCS4 character or UCS4_NONE
UCS4
StrBody::charAt(CharBytes off)
{
	if (off >= num_bytes)
		return UCS4_NONE;
	const UTF8*	cp = start+off;
#if defined(USE_SYS8)
	return (UCS4)*cp;
#else
	int	bytes = UTF8Len(cp);
	if (bytes == 0 || off+bytes > num_bytes)	// Bad encoding or overlaps end
		return UCS4_NONE;
	return UTF8Get(cp);
#endif
}

// Return the next UCS4 character or UCS4_NONE, advancing the offset
UCS4
StrBody::charNext(CharBytes& off)
{
	if (off >= num_bytes)
		return UCS4_NONE;
	const UTF8*	cp = start+off;
#if defined(USE_SYS8)
	return (UCS4)*cp++;
#else
	int	bytes = UTF8Len(cp);
	if (bytes == 0 || off+bytes > num_bytes)	// Bad encoding or overlaps end
		return UCS4_NONE;
	UCS4	ch = UTF8Get(cp);
	off += bytes;
	return ch;
#endif
}

// Return the preceeding UCS4 character or UCS4_NONE, backing up the offset
UCS4
StrBody::charB4(CharBytes& off)
{
	if (off > num_bytes || off == 0)
		return UCS4_NONE;
#if defined(USE_SYS8)
	return (UCS4)start[--off];
#else
	const UTF8*	cp = start+off;
	for (int bytes = 1; bytes <= 4 && cp > start; bytes++)
		if (UTF8Is1st(*--cp))			// Look backwards for a legal UTF-8 1st char
		{
			UCS4	ch = UTF8Get(cp);	// Advances cp again
			if (cp != start+off)		// Must be by the right amount!
				return UCS4_NONE;	// Otherwise it is illegal encoding
			off -= bytes;
			return ch;
		}
	return UCS4_NONE;
#endif
}

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

void
StrBody::countChars()
{
	if (num_chars > 0 || num_bytes == 0)
		return;
#if defined(USE_SYS8)
	num_chars = num_bytes;
#else
	const UTF8*	cp = start;		// Progress pointer when reading data
	UTF8*		op = start;		// Output for compacted data
	UTF8*		ep = start+num_bytes;	// Marker for end of data
	while (cp < ep)
	{
		const UTF8*	char_start = cp; // Save the start of this character, for rewriting
		UCS4		ch = UTF8Get(cp);

		// We don't hold by Postel’s Law here. We really should throw an exception, so I'm being kind. N'yah!
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
	{			// We were rewriting, so now have fewer bytes
		num_bytes = op-start;
		*op = '\0';
	}
#endif
}
