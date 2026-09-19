/*
 * Tests for thread-local slots (include/thread_local.h)
 *
 * What has to hold: a slot holds one void* per thread, a ThreadLocal holds one
 * object per thread created on first use, neither is visible to another thread,
 * and clear() destroys this thread's object without disturbing anybody else's.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<atomic>

#include	<thread_local.h>
#include	<thread.h>

#define	WORKERS		8

static ThreadSlot		slot;		// One slot, shared by every thread
static std::atomic<int>		failures(0);

struct	Counter
{
	static std::atomic<int>	constructed;
	static std::atomic<int>	destroyed;

	int			value;

	Counter() : value(0) { constructed++; }
	~Counter() { destroyed++; }
};
std::atomic<int>	Counter::constructed(0);
std::atomic<int>	Counter::destroyed(0);

static ThreadLocal<Counter>	object;		// One Counter per thread

static void
fail(const char* what)
{
	printf("FAIL: %s\n", what);
	failures++;
}

class	Worker
: public Thread
{
	int	n;
public:
	Worker(int a_n)
	: n(a_n)
	{ resume(); }

	int	run()
	{
		// Our own value in the shared slot, re-read while other threads run
		int	mine = n;
		slot.set(&mine);
		for (int i = 0; i < 50; i++)
		{
			yield();
			if (slot.get() != &mine)
			{
				fail("another thread's slot value was visible");
				return 0;
			}
		}

		// One object for this thread, made on first use
		Counter*	c = object.get();
		if (c->value != 0)
			fail("a new thread's object was not new");
		c->value = n;

		yield();				// Let the others make theirs

		if (object.get() != c)
			fail("a thread's object changed under it");
		if (object.get()->value != n)
			fail("another thread's object was visible");
		if (object.peek() != c)
			fail("peek() disagreed with get()");

		return 0;
	}
};

int
main(int argc, const char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	// The main thread gets its own value and its own object from the same pair
	int	main_value = 1;
	slot.set(&main_value);
	if (slot.get() != &main_value)
		fail("the main thread's slot value was not its own");

	Counter*	main_object = object.get();
	main_object->value = 1;

	for (int i = 0; i < WORKERS; i++)
		(void)new Worker(i+2);

	/*
	 * joinAny() hands back every thread that has ended, then 0. Counted, not
	 * merely looped: it used to stop one short, returning 0 while a worker was
	 * still registered - which left that worker running while this test read
	 * the counters below.
	 */
	int	joined = 0;
	Thread*	ended = 0;
	while ((ended = Thread::joinAny()) != 0)
	{
		delete ended;
		joined++;
	}
	printf("joinAny() handed back %d of %d workers\n", joined, WORKERS);
	if (joined != WORKERS)
		fail("joinAny() did not hand back every worker");

	// None of that may have touched the main thread
	if (slot.get() != &main_value)
		fail("a worker clobbered the main thread's slot value");
	if (object.get() != main_object)
		fail("a worker clobbered the main thread's object");
	if (main_object->value != 1)
		fail("a worker changed the main thread's object");

	printf("one object per thread: built %d (expect %d)\n",
		Counter::constructed.load(), WORKERS+1);
	if (Counter::constructed != WORKERS+1)
		fail("not one object per thread was built");
	if (Counter::destroyed != 0)
		fail("an object was destroyed while its thread was still using it");

	// clear() destroys this thread's object; the next get() makes a new one
	object.clear();
	if (object.peek() != 0)
		fail("clear() left the object in the slot");
	if (Counter::destroyed != 1)
		fail("clear() did not destroy the object");

	/*
	 * Not a pointer comparison against main_object: the allocator is free to
	 * hand back the address just freed, so that would pass or fail by luck.
	 * What matters is that a new object was constructed.
	 */
	int	built_before = Counter::constructed.load();
	Counter*	fresh = object.get();
	if (Counter::constructed != built_before+1)
		fail("get() after clear() did not construct a new object");
	if (fresh->value != 0)
		fail("get() after clear() did not make a fresh object");

	printf("after clear(): built %d, destroyed %d\n",
		Counter::constructed.load(), Counter::destroyed.load());
	printf("%s\n", failures ? "FAILED" : "passed");
	return failures != 0;
}
