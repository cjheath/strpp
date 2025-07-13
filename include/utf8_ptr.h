#if !defined(UTF8_PTR_H)
#define UTF8_PTR_H
/*
 * Wrap a const UTF8* to make it behave like a readonly character pointer, but working in whole UTF8 characters.
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<char_encoding.h>

// A text pointer class which understands UTF-8 and will not advance beyond a Nul character
// Incrementing and decrementing the pointer works in UTF-8 characters
class	NulGuardedUTF8Ptr
{
protected:
	const UTF8*	data;
public:
	NulGuardedUTF8Ptr() : data(0) {}			// Null constructor
	NulGuardedUTF8Ptr(const UTF8* s) : data(s) {}	// Normal constructor
	NulGuardedUTF8Ptr(NulGuardedUTF8Ptr& c) : data(c.data) {}	// Copy constructor
	NulGuardedUTF8Ptr(const NulGuardedUTF8Ptr& c) : data(c.data) {}	// Copy constructor
	~NulGuardedUTF8Ptr() {};
	NulGuardedUTF8Ptr& operator=(NulGuardedUTF8Ptr s)	// Assignment
			{ data = s.data; return *this; }

	bool		at_eof()
			{ return this->operator*() == '\0'; }

	UCS4		operator*()		// Dereference to char under the pointer
			{ const UTF8* s = data; return UTF8Get(s); }
	operator const char*() const
			{ return static_cast<const char*>(data); }	// Access the UTF8 bytes

	static int	len(UCS4 ch)		// Length in bytes of this UCS4 character
			{ return UTF8Len(ch); }
	int		len()			// length in bytes of character under the pointer
			{ return UTF8Len(data); }
	static bool	is1st(const UTF8* s)	// Is this looking at the start of a UTF8 character?
			{ return UTF8Is1st(*s); }
	bool		is1st()			// Are we looking at the start of a UTF8 character?
			{ return UTF8Is1st(*data); }

	// Add and subtract integers:
	NulGuardedUTF8Ptr&	operator+=(int i)
			{	const UTF8* s = data;
				while (i > 0 && *s != '\0') { UTF8Get(s); i--;}	// Advance
				while (i < 0) { s = UTF8Backup(s); i++;}	// Or backup
				data = (const UTF8*)s; return *this;
			}
	NulGuardedUTF8Ptr	operator+(int i){ NulGuardedUTF8Ptr t(*this); t += i; return t; }
	NulGuardedUTF8Ptr	operator-=(int i){ return *this += -i; }
	NulGuardedUTF8Ptr	operator-(int i){ NulGuardedUTF8Ptr t(*this); t += -i; return t; }

	// incr/decr functions:
	NulGuardedUTF8Ptr	postincr()	{ NulGuardedUTF8Ptr save(*this); ++*this; return save; }
	NulGuardedUTF8Ptr&	preincr()	{ const UTF8* s = data; !at_eof() && UTF8Get(s); data = (const UTF8*)s; return *this; }
	NulGuardedUTF8Ptr	postdecr()	{ NulGuardedUTF8Ptr save(*this); --*this; return save; }
	NulGuardedUTF8Ptr&	predecr()	{ data = (const UTF8*)UTF8Backup(data); return *this; }

	// incr/decr operators:
	NulGuardedUTF8Ptr	operator++(int)	{ return postincr(); }
	NulGuardedUTF8Ptr&	operator++()	{ return preincr(); }
	NulGuardedUTF8Ptr	operator--(int)	{ return postdecr(); }
	NulGuardedUTF8Ptr&	operator--()	{ return predecr(); }
	long		operator-(NulGuardedUTF8Ptr s)	{ return data-s.data; }
	long		operator-(const char* cp)	{ return data-cp; }
};

// Similar, but will never back up before the origin either.
class	GuardedUTF8Ptr
: public NulGuardedUTF8Ptr
{
protected:
	const UTF8*	origin;
public:
	GuardedUTF8Ptr() : NulGuardedUTF8Ptr(0), origin(0) {}			// Null constructor
	GuardedUTF8Ptr(const UTF8* s) : NulGuardedUTF8Ptr(s), origin(s) {}	// Normal constructor
	GuardedUTF8Ptr(GuardedUTF8Ptr& c) : NulGuardedUTF8Ptr(c.data), origin(c.origin) {}	// Copy constructor
	GuardedUTF8Ptr(const GuardedUTF8Ptr& c) : NulGuardedUTF8Ptr(c.data), origin(c.origin) {}	// Copy constructor

	GuardedUTF8Ptr&	operator+=(int i)
			{	const UTF8* s = data;
				while (i > 0 && *s != '\0') { UTF8Get(s); i--;}		// Advance
				while (i < 0 && s > origin) { s = UTF8Backup(s); i++;}	// Or backup
				data = (const UTF8*)s; return *this;
			}
	GuardedUTF8Ptr&	predecr()	{ !at_bot() && (data = (const UTF8*)UTF8Backup(data)); return *this; }
	bool		at_bot()
			{ return data == origin; }
};

#endif	// UTF8_PTR_H
