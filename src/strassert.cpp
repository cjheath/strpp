/*
 * What happens when an assertion fails: see include/strassert.h.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<strassert.h>
#include	<str_msg.h>			// The assertion's message
#include	<errbuf.h>			// The buffer the dump comes from

#include	<cstdlib>
#if	defined(HAVE_PTHREADS)
#include	<unistd.h>
#endif

#if	defined(HAVE_PTHREADS)
static void
write_to_stderr(const char* data, int length)
{
	(void)write(2, data, length);
}

void	(*strpp_panic_write)(const char* data, int length) = write_to_stderr;
#else
static void
write_nowhere(const char* data, int length)
{
	// No console of our own to write to: the application sets strpp_panic_write
	(void)data;
	(void)length;
}

void	(*strpp_panic_write)(const char* data, int length) = write_nowhere;
#endif

static void
panic_write(StrVal text)
{
	strpp_panic_write(text.asUTF8(), (int)text.numBytes());
}

void
strpp_assert_failed(const char* file, int line, const char* condition)
{
	// A failure while the dump is running must not report or dump again
	static bool	dying = false;

	if (dying)
		abort();
	dying = true;

	ErrorSTR_Assert(condition, file, line);

	ErrBuf*	buf = error_buffer().peek();
	if (buf)
	{
		/*
		 * Always the oldest message, and delivered before the next is read:
		 * delivered() advances past it, so an index that rose as the count
		 * fell would leave every other message undumped.
		 */
		while (buf->count() > 0)
		{
			StrVal	written;

			{	// The message's parameters are a slice of the buffer's
				// array, so the message, and the string built from it,
				// are both gone before delivered() is called
				ErrBuf::Message	msg = buf->message(0);
				written = StrVal("error ")
					+ StrVal::fromInt32((int32_t)msg.error, 'X')
					+ ": "
					+ StrVal::format(msg.default_text, msg.parameters)
					+ "\n";
			}
			panic_write(written);
			buf->delivered();
		}
	}
	abort();
}
