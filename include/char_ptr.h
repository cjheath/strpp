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

class UnGuardedCharPtr		// Base class, provides no guard rails or overhead
{
protected:
	const char*	data;
public:
	UnGuardedCharPtr() : data(0) {}			// Null constructor
	UnGuardedCharPtr(const char* s) : data(s) {}		// Normal constructor
	UnGuardedCharPtr(UnGuardedCharPtr& c) : data(c.data) {} // Copy constructor
	UnGuardedCharPtr(const UnGuardedCharPtr& c) : data(c.data) {}	// Copy constructor
	~UnGuardedCharPtr() {}
	UnGuardedCharPtr&	operator=(UnGuardedCharPtr s)	// Assignment
			{ data = s.data; return *this; }

	bool		at_eof() { return *data == '\0'; }

	char		operator*() const		// Dereference to char under the pointer
			{ return *data; }
	operator const char*() const			// Access the string via the pointer
			{ return static_cast<const char*>(data); }

	// Add and subtract integers:
	UnGuardedCharPtr&	operator+=(int i)
			{ data += i; return *this; }
	UnGuardedCharPtr	operator+(int i){ UnGuardedCharPtr t(*this); t += i; return t; }
	UnGuardedCharPtr	operator-=(int i){ return *this += -i; }
	UnGuardedCharPtr	operator-(int i){ UnGuardedCharPtr t(*this); t += -i; return t; }

	// incr/decr functions:
	UnGuardedCharPtr&	preincr()	{ data++; return *this; }
	UnGuardedCharPtr	postincr()	{ UnGuardedCharPtr save(*this); ++*this; return save; }

	UnGuardedCharPtr	postdecr()	{ const char* save = data; --data; return save; }
	UnGuardedCharPtr&	predecr()	{ data--; return *this; }

	// incr/decr operators:
	UnGuardedCharPtr	operator++(int)	{ return postincr(); }
	UnGuardedCharPtr&	operator++()	{ return preincr(); }
	UnGuardedCharPtr	operator--(int)	{ return postdecr(); }
	UnGuardedCharPtr&	operator--()	{ return predecr(); }
	long			operator-(UnGuardedCharPtr s)	{ return data-s.data; }
	long			operator-(const char* cp)	{ return data-cp; }
};

class	NulGuardedCharPtr
: public UnGuardedCharPtr
{
public:
	NulGuardedCharPtr() : UnGuardedCharPtr(0) {}		// Null constructor
	NulGuardedCharPtr(const char* s) : UnGuardedCharPtr(s) {} // Normal constructor
	NulGuardedCharPtr(NulGuardedCharPtr& c) : UnGuardedCharPtr(c.data) {}		// Copy constructor
	NulGuardedCharPtr(const NulGuardedCharPtr& c) : UnGuardedCharPtr(c.data) {}	// Copy constructor
	~NulGuardedCharPtr() {}
	NulGuardedCharPtr&	operator=(NulGuardedCharPtr s)	// Assignment
			{ data = s.data; return *this; }

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
	NulGuardedCharPtr&	preincr()	{ const char* s = data; !at_eof() && s++; data = s; return *this; }
	NulGuardedCharPtr	postincr()	{ NulGuardedCharPtr save(*this); ++*this; return save; }

	// incr/decr operators:
	NulGuardedCharPtr	operator++(int)	{ return postincr(); }
	NulGuardedCharPtr&	operator++()	{ return preincr(); }
	long			operator-(NulGuardedCharPtr s)	{ return data-s.data; }
	long			operator-(const char* s)	{ return data-s; }
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
