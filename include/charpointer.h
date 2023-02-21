#if !defined(CHARPOINTER_H)
#define CHARPOINTER_H

#include	<utf8pointer.h>

/*
 * Wrap a const char* to adapt it e.g. as a PEG parser input.
 * Every operation here works as for a char*, but it also supports at_eof()/at_bot() to detect the ends.
 * More pointer-ish methods are possible, but this is limited to the methods needed for Pegexp.
 */
class	TextPtrChar
{
private:
	const char*	data;
	const char*	origin;
public:
	TextPtrChar() : data(0), origin(0) {}				// Null constructor
	TextPtrChar(const char* s) : data(s), origin(s) {}			// Normal constructor
	TextPtrChar(TextPtrChar& c) : data(c.data), origin(c.origin) {}		// Copy constructor
	TextPtrChar(const TextPtrChar& c) : data(c.data), origin(c.origin) {}	// Copy constructor
	~TextPtrChar() {};
	TextPtrChar&		operator=(TextPtrChar s)	// Assignment
				{ data = s.data; origin = s.origin; return *this; }
	operator const char*()					// Access the char under the pointer
				{ return static_cast<const char*>(data); }
	char			operator*()			// Dereference to char under the pointer
				{ return *data; }

	bool			at_eof() { return *data == '\0'; }
	bool			at_bot() { return data == origin; }
	TextPtrChar		operator++(int)	{ return data++; }	// Postfix increment
};

#if	defined(UNICODE)

using	CharPointer = UTF8P;
using	Char = UTF8;

#else

using	CharPointer = TextPtrChar;
using	Char = char;

#endif

#endif
