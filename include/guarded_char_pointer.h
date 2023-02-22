#if !defined(GUARDED_CHAR_POINTER_H)
#define GUARDED_CHAR_POINTER_H
/*
 * Wrap a const char* to guard against walking past the end or back past the start
 * Every operation here works as for a char*, but it also supports at_eof()/at_bot() to detect the ends.
 * More pointer-ish methods are possible, but this is limited...
 */

class	GuardedCharPtr
{
protected:
	const char*	data;
	const char*	origin;
public:
	GuardedCharPtr() : data(0), origin(0) {}		// Null constructor
	GuardedCharPtr(const char* s) : data(s), origin(s) {}			// Normal constructor
	GuardedCharPtr(GuardedCharPtr& c) : data(c.data), origin(c.origin) {}		// Copy constructor
	GuardedCharPtr(const GuardedCharPtr& c) : data(c.data), origin(c.origin) {}	// Copy constructor
	~GuardedCharPtr() {};
	GuardedCharPtr&	operator=(GuardedCharPtr s)	// Assignment
			{ data = s.data; origin = s.origin; return *this; }

	bool		at_eof() { return *data == '\0'; }
	bool		at_bot() { return data == origin; }

	char		operator*() const		// Dereference to char under the pointer
			{ return *data; }
	operator const char*()				// Access the char under the pointer
			{ return static_cast<const char*>(data); }
	GuardedCharPtr	operator++()			// Prefix increment
			{ if (!at_eof()) data++; return *this; }
};

#endif
