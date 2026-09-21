#if !defined(STRASSERT_H)
#define STRASSERT_H
/*
 * An assertion that says what happened before it dies.
 *
 * The library cannot use the C assert. It states its complaint on stderr and
 * dies, which on a target with no stderr states nothing at all - and the one
 * moment when the error buffer is worth reading is the moment after something
 * has gone wrong. So a failure here reports its condition to the thread's error
 * buffer as a message like any other, with its text in the message set and so
 * able to be translated, dumps that buffer, and aborts.
 *
 * A failure while the dump is running aborts at once, without reporting or
 * dumping again: a fault in the reporting path must not become a loop.
 *
 * This header has no dependencies at all: what it declares is implemented in
 * src/strassert.cpp, which is where the error buffer it dumps is known, and the
 * message it reports is in the library's message set - STRERR_ASSERT, the first
 * of the STR set. Nothing here names an error number, so any header may include
 * this one - see ArrayBody::resize, which refuses a size its index could not
 * count.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */

/*
 * Where a panic dump goes. An application sets this to somewhere a developer
 * will see it - a console, a UART, a log - and the library's own writes to
 * standard error where there is a write(2), and nowhere otherwise.
 */
extern void	(*strpp_panic_write)(const char* data, int length);

// Report a failed assertion, dump the error buffer and abort. StrppAssert calls it.
void	strpp_assert_failed(const char* file, int line, const char* condition);

#define	StrppAssert(condition)	\
	do { if (!(condition)) strpp_assert_failed(__FILE__, __LINE__, #condition); } while (0)

#endif	// STRASSERT_H
