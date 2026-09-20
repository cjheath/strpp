#if !defined(ERROR_HXX)
#define	ERROR_HXX
/*
 * Error numbering system.
 *
 * A 32-bit value carrying a message set and a message number, laid out so that
 * it can travel in the same word as a Microsoft HRESULT without being mistaken
 * for one:
 *
 *   bits 31-30  severity: 00 success, 01 informational, 10 warning, 11 error
 *   bit  29     customer bit, always set on ours
 *   bit  28     reserved, must stay clear
 *   bits 27-10  the message set number, 18 bits
 *   bits  9-0   the message number, 10 bits, within its set
 *
 * A message number uses bit 31 for a failure and bit 30 as the designator for
 * information or a warning, and the two are mutually exclusive - so either can
 * be tested with a single bit, and FAILED()-style tests work on ours as they do
 * on a system error.
 *
 * Set numbers run to 262143, and 1024 messages to a set: much smaller sets, and
 * far more of them, than the earlier scheme's 16-bit set and 14-bit code.
 *
 * Set number 0 is the system's own: its message numbers are errno codes, left
 * exactly as the system reports them, with no customer bit set - a bit set on
 * them would stop them being the numbers the system reports.
 *
 * Because every value Microsoft defines leaves the customer bit clear, no
 * HRESULT of any conforming Windows subsystem can collide with a message
 * number. HRESULT's facility field amounts to a subsystem number too, and
 * occupies the same bits as a set number; that is not a collision either, for
 * the same reason.
 *
 * A message number, once used, is never re-used, and set and message numbers
 * are meant to be stable over the life of the software: a number written into a
 * log, a manual or a customer's report must still mean the same thing years
 * later. Which numbers inside a set are used for what is the set author's
 * decision, not a policy of the system.
 *
 * Each message set has an associated error catalog holding the text for each
 * message in each supported language. Binary catalogs may be shipped with a
 * program to be read at run time, and the default text is also compiled in.
 * Each text names its substitution points by position, so a translation may put
 * the parameters in a different order from the default text. Formatting applies
 * a text to a typed array of parameters, and the types can be checked against
 * the default text even when an alternate translation is being used.
 *
 * See Notes/ErrorManagement.md, which is the design this implements.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */
#include	<cstdint>
#include	<limits.h>
#include	<errno.h>
#if     defined(MSW)
#include <winerror.h>
#endif  /* MSW */

#include	<refcount.h>

class ErrNum
{
public:
	static const int32_t	ERR_FLAG = (int32_t)0x80000000;	// A failure: the sign bit, so one test settles it
	static const int32_t	ERR_INFO = (int32_t)0x40000000;	// Information or a warning instead: not a failure
	static const int32_t	ERR_CUST = (int32_t)0x20000000;	// Set on ours, so no Microsoft subsystem can collide
	static const int32_t	ERR_RSVD = (int32_t)0x10000000;	// Reserved: must stay clear

	constexpr ErrNum()
			: errnum(0) {}
	constexpr ErrNum(int set, int msg)
			: errnum(ERR_FLAG | ERR_CUST | ((set & 0x3FFFF) << 10) | (msg & 0x3FF)) {}
	ErrNum(int32_t system)
			: errnum(system) {}	// A system's own number, left exactly as it stands
	ErrNum(const ErrNum& c)
			: errnum(c.errnum) {}
	ErrNum		operator=(const ErrNum& c)
			{ errnum = c.errnum; return *this; }
	constexpr int	set() const
			{ return (errnum >> 10) & 0x3FFFF; }
	constexpr int	msg() const
			{ return errnum & 0x3FF; }
	bool		is_failure() const		// REVISIT: names to settle with the reporting API
			{ return (errnum & ERR_FLAG) != 0; }
	bool		is_info() const
			{ return (errnum & ERR_INFO) != 0; }
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

#endif
