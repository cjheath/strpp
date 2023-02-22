#if !defined(CHAR_PTR_H)
#define CHAR_PTR_H
/*
 * Wrap a const char* to guard against walking past the end (or back past the start)
 * Every operation here works as for a char*, but it also supports at_eof()/at_bot() to detect the ends.
 * More pointer-ish methods are possible, but this is limited...
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */

class	UnguardedCharPtr
{
protected:
	const char*	data;
public:
	UnguardedCharPtr() : data(0) {}			// Null constructor
	UnguardedCharPtr(const char* s) : data(s) {}			// Normal constructor
	UnguardedCharPtr(UnguardedCharPtr& c) : data(c.data) {}		// Copy constructor
	UnguardedCharPtr(const UnguardedCharPtr& c) : data(c.data) {}	// Copy constructor
	~UnguardedCharPtr() {}
	UnguardedCharPtr&	operator=(UnguardedCharPtr s)		// Assignment
			{ data = s.data; return *this; }

	bool		at_eof() { return *data == '\0'; }

	char		operator*() const		// Dereference to char under the pointer
			{ return *data; }
	operator const char*() const			// Access the string via the pointer
			{ return static_cast<const char*>(data); }
	UnguardedCharPtr	operator++()		// Prefix increment
			{ if (!at_eof()) data++; return *this; }
};

class	GuardedCharPtr
: public UnguardedCharPtr
{
protected:
	const char*	origin;
public:
	GuardedCharPtr() : UnguardedCharPtr(0), origin(0) {}		// Null constructor
	GuardedCharPtr(const char* s) : UnguardedCharPtr(s), origin(s) {}			// Normal constructor
	GuardedCharPtr(GuardedCharPtr& c) : UnguardedCharPtr(c.data), origin(c.origin) {}	// Copy constructor
	GuardedCharPtr(const GuardedCharPtr& c) : UnguardedCharPtr(c.data), origin(c.origin) {}	// Copy constructor
	~GuardedCharPtr() {}
	GuardedCharPtr&	operator=(GuardedCharPtr s)	// Assignment
			{ data = s.data; origin = s.origin; return *this; }

	bool		at_bot() { return data == origin; }
};

#endif
