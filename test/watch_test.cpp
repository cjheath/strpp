/*
 * Watch: finding deadlocks and stalls from what each thread holds and waits for.
 * Built with STRPP_MONITOR, which `make watch_test` arranges.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<cstring>
#include	<functional>

#include	<thread.h>
#include	<condition.h>
#include	<lock.h>
#include	<watch.h>

static int	fails = 0;

static void
expect(const char* what, bool ok)
{
	if (!ok)
		fails++;
	printf("  %-60s %s\n", what, ok ? "ok" : "FAIL");
}

// Runs a function in a named thread
class	Runner
: public Thread
{
	std::function<void()>	body;
public:
	Runner(const char* a_name, std::function<void()> a_body)
	: Thread(params(a_name)), body(a_body)
	{ resume(); }

	int	run()	{ body(); return 0; }

private:
	static const ThreadParams*	params(const char* name)
	{
		static ThreadParams	p[8];
		static int		next = 0;
		ThreadParams&		t = p[next++ % 8];
		t.name = name;
		return &t;
	}
};

static unsigned
snapshot(WatchRecord* records)
{
	return WatchSnapshot(records, STRPP_WATCH_THREADS);
}

static int
find_named(const WatchRecord* records, unsigned count, const char* name)
{
	for (unsigned i = 0; i < count; i++)
		if (records[i].name && !strcmp(records[i].name, name))
			return (int)i;
	return -1;
}

static int	lock_a, lock_b, lock_c;		// Addresses stand in for locks

static WatchRecord
record(const char* name)
{
	WatchRecord	r;
	memset(&r, 0, sizeof r);
	r.name = name;
	return r;
}

static void
hold(WatchRecord& r, const void* object, WatchHold mode)
{
	r.held[r.held_count].object = object;
	r.held[r.held_count].mode = mode;
	r.held_count++;
}

static void
wait(WatchRecord& r, const void* object, WatchMode mode, uint32_t since = 0)
{
	r.wait_kind = WatchLock;
	r.wait_object = object;
	r.wait_mode = mode;
	r.wait_since = since;
}

static void
analysis_tests()
{
	printf("\nFinding cycles and stalls in a snapshot\n");
	unsigned	out[8];

	WatchRecord	two[2] = { record("x"), record("y") };
	hold(two[0], &lock_a, HoldWriter);	wait(two[0], &lock_b, WatchForWriter);
	hold(two[1], &lock_b, HoldWriter);	wait(two[1], &lock_a, WatchForWriter);
	expect("two threads each waiting for the other's lock: a cycle", WatchFindCycle(two, 2, out, 8) == 2);
	expect("...which names both", (out[0] == 0 && out[1] == 1) || (out[0] == 1 && out[1] == 0));

	WatchRecord	three[3] = { record("x"), record("y"), record("z") };
	hold(three[0], &lock_a, HoldWriter);	wait(three[0], &lock_b, WatchForWriter);
	hold(three[1], &lock_b, HoldWriter);	wait(three[1], &lock_c, WatchForWriter);
	hold(three[2], &lock_c, HoldWriter);	wait(three[2], &lock_a, WatchForWriter);
	expect("a cycle of three is found", WatchFindCycle(three, 3, out, 8) == 3);

	WatchRecord	chain[3] = { record("x"), record("y"), record("z") };
	hold(chain[0], &lock_a, HoldWriter);	wait(chain[0], &lock_b, WatchForWriter);
	hold(chain[1], &lock_b, HoldWriter);	wait(chain[1], &lock_c, WatchForWriter);
	hold(chain[2], &lock_c, HoldWriter);
	expect("a chain that ends in a thread not waiting is no cycle", WatchFindCycle(chain, 3, out, 8) == 0);

	WatchRecord	readers[2] = { record("x"), record("y") };
	hold(readers[0], &lock_a, HoldShared);	wait(readers[0], &lock_b, WatchForShared);
	hold(readers[1], &lock_b, HoldShared);	wait(readers[1], &lock_a, WatchForShared);
	expect("shared holds do not hold up a shared wait", WatchFindCycle(readers, 2, out, 8) == 0);

	WatchRecord	upgrade[2] = { record("x"), record("y") };
	hold(upgrade[0], &lock_a, HoldShared);	wait(upgrade[0], &lock_a, WatchForWriter);	// Waits for the writer...
	hold(upgrade[1], &lock_a, HoldWriter);	wait(upgrade[1], &lock_a, WatchForDrain);	// ...which waits for the reader
	expect("an upgrade waiting for a reader that waits for the writer", WatchFindCycle(upgrade, 2, out, 8) == 2);

	WatchRecord	cond[2] = { record("x"), record("y") };
	hold(cond[0], &lock_a, HoldWriter);
	cond[1].wait_kind = WatchCondition;
	cond[1].wait_object = &lock_a;
	cond[1].wait_mode = WatchForSignal;
	expect("a condition wait is never part of a cycle", WatchFindCycle(cond, 2, out, 8) == 0);

	WatchRecord	stall[3] = { record("x"), record("y"), record("z") };
	wait(stall[0], &lock_a, WatchForWriter, 1000);		// Not holding anything
	wait(stall[1], &lock_b, WatchForWriter, 1000);	hold(stall[1], &lock_c, HoldShared);
	wait(stall[2], &lock_b, WatchForWriter, 4900);	hold(stall[2], &lock_a, HoldShared);
	expect("waits of at least the threshold are stalls", WatchFindStalls(stall, 3, 6000, 5000, false, out, 8) == 2);
	expect("...and only those holding something, if asked", WatchFindStalls(stall, 3, 6000, 5000, true, out, 8) == 1 && out[0] == 1);
	expect("a threshold not reached finds none", WatchFindStalls(stall, 3, 5000, 5000, false, out, 8) == 0);
}

static void
record_tests()
{
	printf("\nWhat a thread's record shows\n");
	WatchRecord	records[STRPP_WATCH_THREADS];
	SIXLock		lock;
	Latch		latch;

	unsigned	me = (unsigned)-1;
	{
		SharedLock	reading(lock);
		unsigned	count = snapshot(records);
		for (unsigned i = 0; i < count; i++)
			if (records[i].id == Thread::currentId())
				me = i;
		expect("this thread has a record", me != (unsigned)-1);
		expect("...listing the SharedLock it holds",
			me != (unsigned)-1 && records[me].held_count >= 1
			&& records[me].held[records[me].held_count-1].object == (const void*)&lock
			&& records[me].held[records[me].held_count-1].mode == HoldShared);
		expect("...and no wait, since it is not waiting", me != (unsigned)-1 && records[me].wait_kind == WatchNothing);
	}
	unsigned	count = snapshot(records);
	bool		listed = false;
	for (unsigned i = 0; i < count; i++)
		for (unsigned j = 0; records[i].id == Thread::currentId() && j < records[i].held_count; j++)
			listed |= records[i].held[j].object == (const void*)&lock;
	expect("releasing it takes it off the list", !listed);

	latch.enter();
	count = snapshot(records);
	listed = false;
	for (unsigned i = 0; i < count; i++)
		for (unsigned j = 0; records[i].id == Thread::currentId() && j < records[i].held_count; j++)
			listed |= records[i].held[j].object == (const void*)&latch && records[i].held[j].mode == HoldWriter;
	latch.leave();
	expect("a Latch it holds is listed as a writer hold", listed);

	// Waits
	Condition	cond;
	Latch		cond_latch;
	Runner		waiter("cond-waiter", [&]
	{
		cond_latch.enter();
		long	ms = 600;
		cond.wait(ms, &cond_latch);
		cond_latch.leave();
	});
	Thread::yield(Milliseconds(200));
	count = snapshot(records);
	int		w = find_named(records, count, "cond-waiter");
	expect("a thread waiting on a Condition shows the wait", w >= 0 && records[w].wait_kind == WatchCondition
		&& records[w].wait_object == (const void*)&cond);
	expect("...and not the latch it let go of while it waits", w >= 0 && records[w].held_count == 0);
	waiter.join();

	Runner		blocked("lock-waiter", [&]
	{
		SharedLock	reading(lock, Milliseconds(600));
		(void)reading;
	});
	IntentLock	writing(lock);		// Held while the other thread waits for a shared lock
	Thread::yield(Milliseconds(200));
	count = snapshot(records);
	w = find_named(records, count, "lock-waiter");
	expect("a thread waiting for a SIXLock shows the SIXLock, not its inner latch",
		w >= 0 && records[w].wait_kind == WatchLock && records[w].wait_object == (const void*)&lock
		&& records[w].wait_mode == WatchForShared);
	blocked.join();
	writing.release();
}

static void
deadlock_tests()
{
	printf("\nA real deadlock, found while it lasts\n");
	SIXLock		first, second;

	Runner		one("one", [&]
	{
		ExclLock	a(first);
		Thread::yield(Milliseconds(200));
		ExclLock	b(second, Milliseconds(1500));	// Gives up, or this test would never end
	});
	Runner		two("two", [&]
	{
		ExclLock	a(second);
		Thread::yield(Milliseconds(200));
		ExclLock	b(first, Milliseconds(1500));
	});
	Thread::yield(Milliseconds(700));

	WatchRecord	records[STRPP_WATCH_THREADS];
	unsigned	count = snapshot(records);
	unsigned	cycle[8];
	unsigned	length = WatchFindCycle(records, count, cycle, 8);
	expect("two threads that lock in opposite orders form a cycle", length == 2);
	bool		named = false;
	for (unsigned i = 0; i < length; i++)
		for (unsigned j = 0; j < length; j++)
			named |= i != j && records[cycle[i]].name && records[cycle[j]].name
				&& !strcmp(records[cycle[i]].name, "one") && !strcmp(records[cycle[j]].name, "two");
	expect("...and the cycle names both of them", named);

	unsigned	stalls[8];
	unsigned	stalled = WatchFindStalls(records, count, WatchNowMs(), 300, true, stalls, 8);
	expect("both have also waited a while with a lock held", stalled >= 2);

	one.join();
	two.join();
	count = snapshot(records);
	expect("once they give up there is no cycle", WatchFindCycle(records, count, cycle, 8) == 0);
}

static void
window_stall_tests()
{
	printf("\nA reader holding a lock and waiting for a message that never comes\n");
	SIXLock		lock;
	Condition	nothing_comes;
	Latch		latch;

	Runner		reader("reader", [&]
	{
		SharedLock	window(lock);
		latch.enter();
		long	ms = 900;
		nothing_comes.wait(ms, &latch);
		latch.leave();
	});
	Thread::yield(Milliseconds(400));

	WatchRecord	records[STRPP_WATCH_THREADS];
	unsigned	count = snapshot(records);
	unsigned	stalls[8];
	unsigned	stalled = WatchFindStalls(records, count, WatchNowMs(), 200, true, stalls, 8);
	bool		found = false;
	for (unsigned i = 0; i < stalled; i++)
		found |= records[stalls[i]].name && !strcmp(records[stalls[i]].name, "reader");
	expect("it is reported as stalled while holding a lock", found);
	reader.join();
}

static void
lifetime_tests()
{
	printf("\nRecords belong to a thread only while it runs\n");
	WatchRecord	records[STRPP_WATCH_THREADS];
	SIXLock		lock;

	Runner		first("lifetime", [&] { SharedLock s(lock); Thread::yield(Milliseconds(100)); });
	first.join();
	unsigned	after_first = snapshot(records);
	expect("a finished thread's record is gone", find_named(records, after_first, "lifetime") < 0);

	Runner		second("lifetime2", [&] { SharedLock s(lock); Thread::yield(Milliseconds(100)); });
	Thread::yield(Milliseconds(50));
	unsigned	during = snapshot(records);
	expect("a new thread reuses it rather than adding one", during <= after_first+1);
	second.join();
}

int
main(int argc, const char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	analysis_tests();
	record_tests();
	deadlock_tests();
	window_stall_tests();
	lifetime_tests();

	printf("\n%s\n", fails ? "FAILED" : "all watch checks passed");
	return fails != 0;
}
