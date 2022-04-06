#if !defined(UTF8POINTER_H)
#define UTF8POINTER_H
/*
 * Wrap a const UTF8* to make it behave like a readonly character pointer, but working in whole UTF8 characters.
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<char_encoding.h>

class	UTF8P
{
private:
	const UTF8*		data;
public:
	UTF8P() : data(0) {}			// Null constructor
	UTF8P(const UTF8* s) : data(s) {}	// Normal constructor
	UTF8P(UTF8P& c) : data(c.data) {}	// Copy constructor
	UTF8P(const UTF8P& c) : data(c.data) {}	// Copy constructor
	~UTF8P() {};
	UTF8P&		operator=(const UTF8* s)	// Assignment
			{ data = s; return *this; }
	operator const char*() { return static_cast<const char*>(data); }	// Access the UTF8 bytes
	UCS4		operator*()		// Dereference to char under the pointer
			{ const UTF8* s = data; return UTF8Get(s); }

	bool		at_eof() { return **this == '\0'; }

	static int	len(UCS4 ch)		// Length in bytes of this UCS4 character
			{ return UTF8Len(ch); }
	int		len()			// length in bytes of character under the pointer
			{ return UTF8Len(data); }
	static bool	is1st(const UTF8* s)	// Is this looking at the start of a UTF8 character?
			{ return UTF8Is1st(*s); }
	bool		is1st()			// Are we looking at the start of a UTF8 character?
			{ return UTF8Is1st(*data); }

	// Add and subtract integers:
	UTF8P&		operator+=(int i)
			{	const UTF8* s = data;
				while (i > 0) { UTF8Get(s); i--;}		// Advance
				while (i < 0) { s = UTF8Backup(s); i++;}	// Or backup
				data = (const UTF8*)s; return *this;
			}
	UTF8P		operator+(int i)	{ UTF8P t(*this); t += i; return t; }
	UTF8P		operator-=(int i)	{ return *this += -i; }
	UTF8P		operator-(int i)	{ UTF8P t(*this); t += -i; return t; }

	// incr/decr functions:
	UTF8P		postincr()	{ UTF8P save(*this); ++*this; return save; }
	UTF8P&		preincr()	{ const UTF8* s = data; UTF8Get(s); data = (const UTF8*)s; return *this; }
	UTF8P		postdecr()	{ UTF8P save(*this); --*this; return save; }
	UTF8P&		predecr()	{ data = (const UTF8*)UTF8Backup(data); return *this; }

	// incr/decr operators:
	UTF8P		operator++(int)	{ return postincr(); }
	UTF8P&		operator++()	{ return preincr(); }
	UTF8P		operator--(int)	{ return postdecr(); }
	UTF8P&		operator--()	{ return predecr(); }
};
#endif	// UTF8POINTER_H
