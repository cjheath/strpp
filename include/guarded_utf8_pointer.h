#if !defined(UTF8POINTER_H)
#define UTF8POINTER_H
/*
 * Wrap a const UTF8* to make it behave like a readonly character pointer, but working in whole UTF8 characters.
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<char_encoding.h>

class	GuardedUTF8Ptr
{
protected:
	const UTF8*	data;
	const UTF8*	origin;
public:
	GuardedUTF8Ptr() : data(0), origin(0) {}			// Null constructor
	GuardedUTF8Ptr(const UTF8* s) : data(s), origin(s) {}	// Normal constructor
	GuardedUTF8Ptr(GuardedUTF8Ptr& c) : data(c.data), origin(c.origin) {}	// Copy constructor
	GuardedUTF8Ptr(const GuardedUTF8Ptr& c) : data(c.data), origin(c.origin) {}	// Copy constructor
	~GuardedUTF8Ptr() {};
	GuardedUTF8Ptr&	operator=(GuardedUTF8Ptr s)	// Assignment
			{ data = s.data; origin = s.origin; return *this; }

	bool		at_eof()
			{ return this->operator*() == '\0'; }
	bool		at_bot()
			{ return data == origin; }

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
	GuardedUTF8Ptr&	operator+=(int i)
			{	const UTF8* s = data;
				while (i > 0 && !at_eof()) { UTF8Get(s); i--;}		// Advance
				while (i < 0 && !at_bot()) { s = UTF8Backup(s); i++;}	// Or backup
				data = (const UTF8*)s; return *this;
			}
	GuardedUTF8Ptr	operator+(int i)	{ GuardedUTF8Ptr t(*this); t += i; return t; }
	GuardedUTF8Ptr	operator-=(int i)	{ return *this += -i; }
	GuardedUTF8Ptr	operator-(int i)	{ GuardedUTF8Ptr t(*this); t += -i; return t; }

	// incr/decr functions:
	GuardedUTF8Ptr	postincr()	{ GuardedUTF8Ptr save(*this); ++*this; return save; }
	GuardedUTF8Ptr&	preincr()	{ const UTF8* s = data; !at_eof() && UTF8Get(s); data = (const UTF8*)s; return *this; }
	GuardedUTF8Ptr	postdecr()	{ GuardedUTF8Ptr save(*this); --*this; return save; }
	GuardedUTF8Ptr&	predecr()	{ !at_bot() && (data = (const UTF8*)UTF8Backup(data)); return *this; }

	// incr/decr operators:
	GuardedUTF8Ptr	operator++(int)	{ return postincr(); }
	GuardedUTF8Ptr&	operator++()	{ return preincr(); }
	GuardedUTF8Ptr	operator--(int)	{ return postdecr(); }
	GuardedUTF8Ptr&	operator--()	{ return predecr(); }
	long		operator-(GuardedUTF8Ptr s)	{ return data-s.data; }
	long		operator-(const char* cp)	{ return data-cp; }
};
#endif	// UTF8POINTER_H
