/*
 * Tests for the per-thread error buffer (include/errbuf.h)
 *
 * What has to hold: what a caller reports is what the buffer holds, with its
 * parameters intact; messages are consecutively numbered, a recovery gives its
 * numbers back, and a delivered number is never re-used; a rollback drops a
 * callee's entries and parameters and leaves the caller's; an action that
 * reports and delivers allocates nothing; and each thread has a buffer of its
 * own.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<cstdlib>
#include	<cstring>
#include	<new>
#include	<atomic>

#include	<errbuf.h>
#include	<thread.h>

static std::atomic<long>	allocations(0);
static std::atomic<int>		failures(0);

void*	operator new(size_t n)
{
	allocations++;
	void*	p = malloc(n ? n : 1);
	if (!p)
		throw std::bad_alloc();
	return p;
}
void	operator delete(void* p) noexcept		{ free(p); }
void	operator delete(void* p, size_t) noexcept	{ free(p); }

static void
fail(const char* what)
{
	printf("FAIL: %s\n", what);
	failures++;
}

static void
check(const char* what, long got, long expect)
{
	if (got != expect)
	{
		printf("FAIL: %s: got %ld, expected %ld\n", what, got, expect);
		failures++;
	}
}

/*
 * What a generated reporting function will do: fill a scratch array that is
 * emptied between reports rather than rebuilt, so that its storage is reused.
 * Building a fresh VariantArray per report costs two allocations - measured,
 * before this was changed - which is the cost this shape exists to avoid.
 */
static ThreadLocal<VariantArray>	scratch_params;

static ErrNum
report_with(VariantArray& params, int msg)
{
	return Error(ErrNum(100, msg), "a default text", params);
}

static ErrNum
report_one(int msg, int value, const char* text)
{
	VariantArray&	params = *scratch_params.get();
	params.evacuate();
	params.append(Variant(value));
	params.append(Variant(text));
	return report_with(params, msg);
}

int
main(int argc, const char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);
	ErrBuf*	buf = ErrBuffer();

	// Reporting answers the number it was given, and records it
	ErrNum	reported = report_one(1, 42, "first");
	check("Error answers its argument", (int32_t)reported == (int32_t)ErrNum(100, 1), 1);
	check("count after one report", buf->count(), 1);

	// A message carries its number, its default text and its parameters
	ErrBuf::Message	m = buf->message(0);
	check("the message's error number", (int32_t)m.error == (int32_t)ErrNum(100, 1), 1);
	check("its default text", m.default_text && !strcmp(m.default_text, "a default text"), 1);
	check("its parameter count", m.parameters.length(), 2);
	check("its first parameter", m.parameters[0].as_int(), 42);
	check("its second parameter", m.parameters[1].as_strval() == "first", 1);
	check("and the number alone", (int32_t)buf->error(0) == (int32_t)ErrNum(100, 1), 1);

	// A zero error number reports nothing
	Error(ErrNum(), "no error", VariantArray());
	check("a zero ErrNum is not recorded", buf->count(), 1);

	// Messages are consecutively numbered
	ErrBuf::MsgSequence	base = buf->checkpoint();
	report_one(2, 43, "second");
	check("one report takes one number", buf->checkpoint(), base+1);

	// Checkpoint and rollback: what a callee reports can be dropped
	ErrBuf::MsgSequence	mark = buf->checkpoint();
	report_one(3, 44, "callee's");
	report_one(4, 45, "callee's too");
	check("the callee's entries are there", buf->count(), 4);
	check("and numbered on from the caller's", buf->checkpoint(), mark+2);
	buf->rollback(mark);
	check("rollback drops the callee's", buf->count(), 2);
	check("and gives their numbers back", buf->checkpoint(), mark);

	// Which the next report takes again, so nothing is ever skipped
	report_one(5, 46, "after rollback");
	check("a recovered number is used again", buf->checkpoint(), mark+1);
	check("and the re-used entry is the new one", (int32_t)buf->error(2) == (int32_t)ErrNum(100,5), 1);
	check("with its own parameters", buf->message(2).parameters[0].as_int(), 46);
	check("and the caller's are untouched", buf->message(1).parameters[0].as_int(), 43);

	// Delivery takes the oldest, and its number is retired for good
	base = buf->checkpoint();
	buf->delivered();
	check("delivery drops the oldest", buf->count(), 2);
	buf->delivered();
	buf->delivered();
	check("delivery drains the buffer", buf->count(), 0);
	check("and the numbers are not handed back", buf->checkpoint(), base);
	report_one(6, 47, "after delivery");
	check("a later report numbers on", buf->checkpoint(), base+1);
	buf->clear();

	/*
	 * The point of the shape: once an action has run and been delivered, the
	 * next action of the same size allocates nothing. Measured, not assumed.
	 */
	VariantArray&	params = *scratch_params.get();
	for (int warm = 0; warm < 2; warm++)	// Two whole actions, so every array has
	{					// grown to the size being measured
		for (int i = 0; i < 100; i++)
		{
			params.evacuate();
			params.append(Variant(i));
			report_with(params, i+1);
		}
		while (buf->count() > 0)
			buf->delivered();
	}
	buf->clear();

	long	before = allocations.load();
	for (int round = 0; round < 3; round++)
	{
		for (int i = 0; i < 100; i++)
		{
			params.evacuate();
			params.append(Variant(i));
			report_with(params, i+1);
		}
		while (buf->count() > 0)
		{
			/*
			 * What a display does: take the message, use it, let it go, then
			 * mark it delivered. The message must not still be held at the
			 * delivery: its parameters are a slice of the buffer's parameter
			 * array, and while that slice lives the array cannot be reclaimed.
			 */
			{
				ErrBuf::Message	shown = buf->message(0);
				if (shown.parameters.length() != 1
				 || shown.parameters[0].as_int() != 100-(int)buf->count())
					fail("a message awaiting delivery was not the one expected");
			}
			buf->delivered();
		}
	}
	long	used = allocations.load() - before;
	printf("3 report-and-deliver actions of 100 allocated %ld times\n", used);
	check("reporting and delivering allocate nothing", used, 0);
	check("and delivery emptied the buffer", buf->count(), 0);

	// Each thread has a buffer of its own
	class	Reporter : public Thread
	{
	public:
		Reporter(int n) : count(n) { resume(); }
		int	run()
		{
			ErrBuf*	b = ErrBuffer();
			if (b->count() != 0)
				fail("a new thread's buffer was not empty");
			for (int i = 0; i < count; i++)
				report_one(i+1, i, "from a thread");
			if (b->count() != (unsigned)count)
				fail("a thread did not record its own reports");
			if (b->message(0).parameters[0].as_int() != 0)
				fail("a thread lost its first parameter");
			b->clear();
			return 0;
		}
		int	count;
	};

	report_one(9, 99, "the main thread's");
	for (int i = 0; i < 4; i++)
		(void)new Reporter(5);
	Thread*	ended = 0;
	while ((ended = Thread::joinAny()) != 0)
		delete ended;

	check("the main thread's buffer is untouched", buf->count(), 1);
	check("and holds its own parameter", buf->message(0).parameters[0].as_int(), 99);

	printf("%s\n", failures ? "FAILED" : "all checks passed");
	return failures != 0;
}
