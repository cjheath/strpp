#if !defined(ERROR_HXX)
#define	ERROR_HXX
/*
 * Error numbering system.
 *
 * Each subsystem is statically allocated a 16-bit subsystem "set" number.
 * Each set contains up to 16384 messages indicated by a 14-bit code.
 * The set number 0 corresponds to the system errno, and the msg codes are the errno codes.
 *
 * For compliance with the Microsoft error scheme, the high-order (sign) bit is always set.
 * Because Microsoft subsystems also avoid using the second-top bit (they call it CUST),
 * this bit is always set, to keep clear of collisions with Microsoft subsystems.
 *
 * Each message set has an associated error catalog containing the text for each message,
 * for each supported language. Binary versions of these message catalogs may be shipped
 * with programs to be read at runtime. Default message text should also be compiled in
 * to programs.
 *
 * Each message text contains printf-style format codes, augmented with parameter numbers,
 * so that after translation into other languages, the parameters may be formatted in a
 * different order than in the default text.
 *
 * Message formatting applies the message template to a typed array of parameters. The
 * types can be checked to conform to the default text, even when being formatted using
 * an alternate translation.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */
#include	<stdint.h>
#include	<limits.h>
#include	<errno.h>
#if     defined(MSW)
#include <winerror.h>
#endif  /* MSW */

#include	<refcount.h>

class ErrNum
{
public:
	static const int32_t	ERR_FLAG = 0x8000000;	// Sign bit is used to indicate an error, allowing quick checks
	static const int32_t	ERR_CUST = 0x4000000;	// Including this bit guarantees no collision with Microsoft subsystem codes

	constexpr ErrNum()
			: errnum(0) {}
	constexpr ErrNum(int set, int msg)
			: errnum(ERR_FLAG | ERR_CUST | ((set & 0xFFFF) << 14) | (msg & 0x3FFF)) {}
	constexpr ErrNum(int32_t setmsg)
			: errnum(setmsg | ERR_FLAG | ERR_CUST) {}
	ErrNum(const ErrNum& c)
			: errnum(c.errnum) {}
	ErrNum		operator=(const ErrNum& c)
			{ errnum = c.errnum; return *this; }
	constexpr int	set() const
			{ return (errnum >> 14) & 0xFFFF; }
	constexpr int	msg() const
			{ return errnum & 0x3FFF; }
	bool		operator==(ErrNum x) const
			{ return errnum == x.errnum; }
	bool		operator!=(ErrNum x) const
			{ return errnum != x.errnum; }
	bool		operator==(int x) const
			{ return errnum == x; }
	bool		operator<(int x) const
			{ return errnum < x; }
	bool		operator>(int x) const
			{ return errnum > x; }
	constexpr operator int32_t() const		// Allows use in switch statements
			{ return errnum; }
private:
	int32_t		errnum;
};

/*
 * A returned error has an ErrNum, the default text, and a parameter list.
 * The data is returned by referencem
 * REVISIT: This class has a stub implementation of parameter lists
 */
class	Error
{
	class Body;
public:
	Error()
			: body(0) {}
	Error(int32_t setmsg, const char* d = 0, const void* p = 0)
			: body(new Body(setmsg, d, p)) {}	// REVISIT: Use a special allocator
	Error&		operator=(const Error& e)
			{ body = e.body;  return *this; }
	Error(const Error& e)
			: body(e.body) {}
	operator ErrNum() const
			{ return body ? body->error_num() : ErrNum(0); }
	const char*	default_text() const
			{ return body ? body->default_text() : 0; }
	const void*	parameters() const
			{ return body ? body->parameters() : 0; }
	operator int32_t() const { return body ? (int32_t)body->error_num() : 0; }

private:
	Ref<Body>	body;

	class Body
	: public RefCounted
	{
	public:
		Body(ErrNum n, const char* d, const void* p)
		: num(n), def_text(d), params(p)
		{}
		ErrNum		error_num() const { return num; }
		const char*	default_text() const { return def_text; }
		const void*	parameters() const { return params; }

	protected:
		ErrNum		num;
		const char*	def_text;
		const void*	params;			// Placeholder for proper implementation
	};
};

#endif
