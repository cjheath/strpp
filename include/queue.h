#if !defined(QUEUE_H)
#define QUEUE_H
/*
 * Queue: a thread-safe FIFO of Variants. A Thread may create/access its
 * incoming work queue (saved as a ThreadLocal) by calling Queue::mine().
 *
 * A sender needs to be handed a pointer or reference to a recipient thread's
 * Queue before it can push() onto that queue.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<lockfree.h>
#include	<condition.h>
#include	<thread_local.h>
#include	<datetime.h>
#include	<variant.h>

class	Queue
{
public:
	Queue() {}
	~Queue() {}

	// A queue may not be copied: see Latch, which it holds one of
	Queue(const Queue&) = delete;
	Queue&		operator=(const Queue&) = delete;

	void		push(const Variant& item);
	void		push(const VariantArray& items);	// Several, in order, under one lock

	Variant		pop();				// Wait as long as it takes
	Variant		pop(Milliseconds timeout);	// Give up after timeout; a null Variant means nothing arrived
	bool		try_pop(Variant& item);	// Take one now if there is one, without waiting

	bool		isEmpty();

	// A Queue that serves as the calling thread's own inbox (created on first use)
	static Queue*	mine()
			{
				static ThreadLocal<Queue>	mailbox;
				return mailbox.get();
			}

private:
	Latch		latch;
	Condition	not_empty;
	VariantArray	items;
};

inline void
Queue::push(const Variant& item)
{
	latch.enter();
	items.push(item);
	not_empty.signal();
	latch.leave();
}

inline void
Queue::push(const VariantArray& to_add)
{
	latch.enter();
	for (VariantArray::Index i = 0; i < to_add.length(); i++)
		items.push(to_add[i]);
	if (to_add.length() > 0)
		not_empty.signal();	// One signal wakes a waiter, who then drains what's there
	latch.leave();
}

inline Variant
Queue::pop()
{
	latch.enter();
	while (items.isEmpty())
		not_empty.wait(&latch);
	Variant		item = items.shift();
	latch.leave();
	return item;
}

inline Variant
Queue::pop(Milliseconds timeout)
{
	long		remaining_ms = (long)timeout.ms();
	latch.enter();
	while (items.isEmpty())
	{
		if (remaining_ms <= 0)
		{
			latch.leave();
			return Variant();	// Null: nothing arrived in time
		}
		not_empty.wait(remaining_ms, &latch);
	}
	Variant		item = items.shift();
	latch.leave();
	return item;
}

inline bool
Queue::try_pop(Variant& item)
{
	latch.enter();
	bool		got = !items.isEmpty();
	if (got)
		item = items.shift();
	latch.leave();
	return got;
}

inline bool
Queue::isEmpty()
{
	latch.enter();
	bool		empty = items.isEmpty();
	latch.leave();
	return empty;
}

#endif	// QUEUE_H
