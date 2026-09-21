/*
 * What happens when an assertion fails: see include/strassert.h.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<strassert.h>
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

	Error(STRPPERR_ASSERT, "Assertion failed: `{1}` at {2}:{3}",
		VariantArray() << condition << file << line);

	ErrBuf*	buf = error_buffer().peek();
	if (buf)
	{
		for (ErrBuf::MsgIndex i = 0; i < buf->count(); i++)
		{
			StrVal	written;

			{	// The message's parameters are a slice of the buffer's
				// array, so the message, and the string built from it,
				// are both gone before delivered() is called
				ErrBuf::Message	msg = buf->message(i);
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
