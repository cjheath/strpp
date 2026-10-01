#if !defined(MSGQUEUE_H)
#define MSGQUEUE_H
/*
 * MessageQueue: a thread-safe FIFO of Variants. A Thread may create/access its
 * incoming work queue (saved as a ThreadLocal) by calling MessageQueue::mine().
 *
 * A sender needs to be handed a pointer or reference to a recipient thread's
 * MessageQueue before it can push() onto that queue.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<lockfree.h>
#include	<condition.h>
#include	<thread_local.h>
#include	<datetime.h>
#include	<variant.h>

class	MessageQueue
{
public:
	MessageQueue() {}
	~MessageQueue() {}

	// A queue may not be copied: see Latch, which it holds one of
	MessageQueue(const MessageQueue&) = delete;
	MessageQueue&		operator=(const MessageQueue&) = delete;

	void		push(const Variant& item);
	void		push(const VariantArray& items);	// Several, in order, under one lock

	Variant		pop();				// Wait as long as it takes
	Variant		pop(Milliseconds timeout);	// Give up after timeout; a null Variant means nothing arrived
	bool		try_pop(Variant& item);	// Take one now if there is one, without waiting

	bool		isEmpty();

	// A MessageQueue that serves as the calling thread's own inbox (created on first use)
	static MessageQueue*	mine()
			{
				static ThreadLocal<MessageQueue>	mailbox;
				return mailbox.get();
			}

private:
	Latch		latch;
	Condition	not_empty;
	VariantArray	items;
};

inline void
MessageQueue::push(const Variant& item)
{
	latch.enter();
	items.push(item);
	not_empty.signal();
	latch.leave();
}

inline void
MessageQueue::push(const VariantArray& to_add)
{
	latch.enter();
	for (VariantArray::Index i = 0; i < to_add.length(); i++)
		items.push(to_add[i]);
	if (to_add.length() > 0)
		not_empty.signal();	// One signal wakes a waiter, who then drains what's there
	latch.leave();
}

inline Variant
MessageQueue::pop()
{
	latch.enter();
	while (items.isEmpty())
		not_empty.wait(&latch);
	Variant		item = items.shift();
	latch.leave();
	return item;
}

inline Variant
MessageQueue::pop(Milliseconds timeout)
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
MessageQueue::try_pop(Variant& item)
{
	latch.enter();
	bool		got = !items.isEmpty();
	if (got)
		item = items.shift();
	latch.leave();
	return got;
}

inline bool
MessageQueue::isEmpty()
{
	latch.enter();
	bool		empty = items.isEmpty();
	latch.leave();
	return empty;
}

#endif	// MSGQUEUE_H
