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
 */
#include	<stdint.h>
#include	<limits.h>
#include	<errno.h>
#if     defined(MSW)
#include <winerror.h>
#endif  /* MSW */

class ErrNum
{
public:
	static const int32_t	ERR_FLAG = 0x8000000;	// Sign bit is used to indicate an error, allowing quick checks
	static const int32_t	ERR_CUST = 0x4000000;	// Including this bit guarantees no collision with Microsoft subsystem codes

	ErrNum(int set, int msg)
			: errnum(ERR_FLAG | ERR_CUST | ((set & 0xFFFF) << 14) | (msg & 0x3FFF)) {}
	ErrNum(int32_t setmsg)
			: errnum(setmsg) { if (errnum) errnum |= (ERR_FLAG | ERR_CUST); }
	~ErrNum() {}
	ErrNum(const ErrNum& c)
			: errnum(c.errnum) {}
	ErrNum		operator=(const ErrNum& c)
			{ errnum = c.errnum; return *this; }
	int		set() const
			{ return (errnum >> 14) & 0xFFFF; }
	int		msg() const
			{ return errnum & 0x3FFF; }
	bool		operator==(int x) const
			{ return errnum == x; }
	bool		operator<(int x) const
			{ return errnum < x; }
	bool		operator>(int x) const
			{ return errnum > x; }
	operator int32_t() const		// Allows use in switch statements
			{ return errnum; }
private:
	int32_t		errnum;
};

#define	ERRNUM(set, msg)	((int32_t)(0xC000000 | (((set) & 0xFFFF) << 12) | ((msg) & 0xFFF)))

/*
 * A returned error has an ErrNum, the default text, and a parameter list.
 * This class has a stub implementation of parameter lists
 */
class	Error
{
public:
	Error()
			: num(0), params(0) {}
	Error(int32_t setmsg, const char* d = 0, const void* p = 0)
			: num(setmsg), def_text(d), params(p) {}
	Error&		operator=(const Error& e)
			{ num = e.num; def_text = e.def_text; params = e.params; return *this; }
	Error(const Error& e)
			: num(e.num), def_text(e.def_text), params(e.params) {}
	operator int32_t() const
			{ return num; }
	const void*	parameters() const
			{ return params; }
	const char*	default_text() const
			{ return def_text; }

private:
	ErrNum		num;
	const char*	def_text;
	const void*	params;			// Placeholder for proper implementation
};

#endif
