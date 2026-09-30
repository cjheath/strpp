/*
 * Queue: push/pop, blocking pop woken by another thread, and pop(timeout).
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdio>
#include	<time.h>

#include	<lockfree.h>
#include	<thread.h>
#include	<condition.h>
#include	<queue.h>

static int	fails = 0;

static void
expect(const char* what, bool ok)
{
	if (!ok)
		fails++;
	printf("  %-56s %s\n", what, ok ? "ok" : "FAIL");
}

// Milliseconds on the same clock the timed wait uses
static long
now_ms()
{
	struct timespec	ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	return (long)(ts.tv_sec*1000 + ts.tv_nsec/1000000);
}

// Pushes one item onto a given Queue, after a delay
class	Pusher
: public Thread
{
	Queue*		target;
	long		delay_ms;
	int		value;
public:
	Pusher(Queue* a_target, long a_delay, int a_value)
	: target(a_target)
	, delay_ms(a_delay)
	, value(a_value)
	{ resume(); }

	int	run()
	{
		yield(Milliseconds(delay_ms));
		target->push(Variant(value));
		return 0;
	}
};

static void
push_pop_tests()
{
	printf("\nQueue push/pop, single thread\n");
	Queue		q;
	expect("a new queue is empty", q.isEmpty());

	Variant		unused;
	expect("try_pop on an empty queue returns false", !q.try_pop(unused));

	q.push(Variant(42));
	expect("a pushed queue is not empty", !q.isEmpty());

	Variant		got = q.pop();
	expect("pop() returns what was pushed", got.as_int() == 42);
	expect("popping the only item empties the queue", q.isEmpty());

	// A batch pushes in order, and pops come out in the same order (FIFO)
	VariantArray	batch;
	batch.push(Variant(1));
	batch.push(Variant(2));
	batch.push(Variant(3));
	q.push(batch);
	expect("batch item 1 pops first", q.pop().as_int() == 1);
	expect("batch item 2 pops second", q.pop().as_int() == 2);
	expect("batch item 3 pops third", q.pop().as_int() == 3);
	expect("the queue is empty again", q.isEmpty());
}

static void
timeout_tests()
{
	printf("\nQueue::pop(timeout)\n");
	Queue		q;

	long	start = now_ms();
	Variant	got = q.pop(Milliseconds(50));
	long	waited = now_ms()-start;
	expect("pop(timeout) on an empty queue returns a null Variant", got.is_null());
	expect("...and waited at least 40ms", waited >= 40);
	expect("...and not far longer than it was asked to", waited < 1000);
}

static void
blocking_pop_tests()
{
	printf("\nQueue::pop() blocks until another thread pushes\n");
	Queue		q;

	Pusher		pusher(&q, 200, 99);
	long		start = now_ms();
	Variant		got = q.pop();		// Would block forever if nothing signalled it
	long		waited = now_ms()-start;
	pusher.join();

	expect("pop() returns the value the other thread pushed", got.as_int() == 99);
	expect("pop() waited roughly as long as the push was delayed", waited >= 150 && waited < 4000);
}

int
main(int argc, const char** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	push_pop_tests();
	timeout_tests();
	blocking_pop_tests();

	printf("\n%s\n", fails ? "FAILED" : "all queue checks passed");
	return fails != 0;
}
