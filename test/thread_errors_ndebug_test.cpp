/*
 * A thread that ends with errors still in its buffer: whoever joins it gets them,
 * as a cascade. Built with NDEBUG, since a build with assertions stops at the end of
 * such a thread instead.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<cstring>

#include	<thread.h>
#include	<strpp_msg.h>

static int	fails = 0;

static void
expect(const char* what, bool ok)
{
	if (!ok)
		fails++;
	printf("  %-60s %s\n", what, ok ? "ok" : "FAIL");
}

class	Reporter
: public Thread
{
	int		errors;
public:
	Reporter(const char* a_name, int a_errors)
	: Thread(params(a_name)), errors(a_errors)
	{ resume(); }

	int	run()
	{
		for (int i = 0; i < errors; i++)
			ErrorTHR_CreateFailed("something", i+1);
		return 7;
	}

private:
	static const ThreadParams*	params(const char* name)
	{
		static ThreadParams	p[4];
		static int		next = 0;
		p[next].name = name;
		return &p[next++ % 4];
	}
};

int
main(int argc, const char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);
	printf("\nA thread that ends with errors\n");

	ErrBuffer()->clear();
	Reporter	clean("clean", 0);
	expect("a thread with no errors joins with its exit code", clean.join() == 7);
	expect("...and leaves nothing in the joiner's buffer", ErrBuffer()->count() == 0);

	Reporter	doomed("doomed", 2);
	expect("a thread with errors still joins with its exit code", doomed.join() == 7);
	ErrBuf*		eb = ErrBuffer();
	expect("...and its errors arrive, then one to say where from", eb->count() == 3);
	expect("...the cause first", eb->error(0) == THRERR_CREATE_FAILED && eb->error(1) == THRERR_CREATE_FAILED);
	expect("...and the result last", eb->error(2) == THRERR_ENDED_WITH_ERRORS);
	{
		ErrBuf::Message	m = eb->message(2);
		expect("...naming the thread and how many errors it left",
			m.parameters.length() == 2 && m.parameters[0].as_strval() == "doomed" && m.parameters[1].as_int() == 2);
		ErrBuf::Message	first = eb->message(1);
		expect("...with each error's own parameters", first.parameters[1].as_int() == 2);
	}
	eb->clear();

	Reporter	another("another", 1);
	Thread*		ended = Thread::joinAny();
	expect("joinAny delivers them too", ended == &another && ErrBuffer()->count() == 2);
	ErrBuffer()->clear();
	another.join();

	printf("\n%s\n", fails ? "FAILED" : "all thread-error checks passed");
	return fails != 0;
}
