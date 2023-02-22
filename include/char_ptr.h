#if !defined(CHAR_PTR_H)
#define CHAR_PTR_H
/*
 * Wrap a const char* to guard against walking past the end (or back past the start)
 * Every operation here works as for a char*, but it also supports at_eof()/at_bot() to detect the ends.
 *
 * NulGuardedCharPtr is guarded to not run past a Nul termination.
 * GuardedCharPtr is also guarded to never back up past the start.
 * Length-guarded pointers are left as an exerise for the reader :)
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */

class	NulGuardedCharPtr
{
protected:
	const char*	data;
public:
	NulGuardedCharPtr() : data(0) {}			// Null constructor
	NulGuardedCharPtr(const char* s) : data(s) {}		// Normal constructor
	NulGuardedCharPtr(NulGuardedCharPtr& c) : data(c.data) {} // Copy constructor
	NulGuardedCharPtr(const NulGuardedCharPtr& c) : data(c.data) {}	// Copy constructor
	~NulGuardedCharPtr() {}
	NulGuardedCharPtr&	operator=(NulGuardedCharPtr s)	// Assignment
			{ data = s.data; return *this; }

	bool		at_eof() { return *data == '\0'; }

	char		operator*() const		// Dereference to char under the pointer
			{ return *data; }
	operator const char*() const			// Access the string via the pointer
			{ return static_cast<const char*>(data); }

	// Add and subtract integers:
	NulGuardedCharPtr&	operator+=(int i)
			{	const char* s = data;
				while (i > 0 && *s != '\0') { s++; i--;}// Advance slowly
				if (i < 0) s -= i;		// Or backup suddenly
				data = s; return *this;
			}
	NulGuardedCharPtr	operator+(int i){ NulGuardedCharPtr t(*this); t += i; return t; }
	NulGuardedCharPtr	operator-=(int i){ return *this += -i; }
	NulGuardedCharPtr	operator-(int i){ NulGuardedCharPtr t(*this); t += -i; return t; }

	// incr/decr functions:
	NulGuardedCharPtr	postincr()	{ NulGuardedCharPtr save(*this); ++*this; return save; }
	NulGuardedCharPtr&	preincr()	{ const char* s = data; !at_eof() && s++; data = s; return *this; }
	NulGuardedCharPtr	postdecr()	{ NulGuardedCharPtr save(*this); --*this; return save; }
	NulGuardedCharPtr&	predecr()	{ data--; return *this; }

	// incr/decr operators:
	NulGuardedCharPtr	operator++(int)	{ return postincr(); }
	NulGuardedCharPtr&	operator++()	{ return preincr(); }
	NulGuardedCharPtr	operator--(int)	{ return postdecr(); }
	NulGuardedCharPtr&	operator--()	{ return predecr(); }
	long			operator-(NulGuardedCharPtr s)	{ return data-s.data; }
	long			operator-(const char* cp)	{ return data-cp; }
};

class	GuardedCharPtr
: public NulGuardedCharPtr
{
protected:
	const char*	origin;
public:
	GuardedCharPtr() : NulGuardedCharPtr(0), origin(0) {}		// Null constructor
	GuardedCharPtr(const char* s) : NulGuardedCharPtr(s), origin(s) {}			// Normal constructor
	GuardedCharPtr(GuardedCharPtr& c) : NulGuardedCharPtr(c.data), origin(c.origin) {}	// Copy constructor
	GuardedCharPtr(const GuardedCharPtr& c) : NulGuardedCharPtr(c.data), origin(c.origin) {}	// Copy constructor
	~GuardedCharPtr() {}
	GuardedCharPtr&	operator=(GuardedCharPtr s)	// Assignment
			{ data = s.data; origin = s.origin; return *this; }

	bool		at_bot() { return data == origin; }

	GuardedCharPtr&	operator+=(int i)
			{	const char* s = data;
				while (i > 0 && *s != '\0') { s++; i--;}// Advance
				if (-i > s-origin)	// Or backup
					s = origin;	// Either return as far as the origin
				else
					s += i;		// or backup as far as requested
				data = s; return *this;
			}
};

#endif
