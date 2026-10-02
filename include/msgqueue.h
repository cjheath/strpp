#if !defined(MSGQUEUE_H)
#define MSGQUEUE_H
/*
 * MessageQueue: a thread-safe FIFO of Variants. A Thread may create/access its
 * incoming work queue (saved as a ThreadLocal) by calling MessageQueue::mine().
 *
 * A sender needs to be handed a pointer or reference to a recipient thread's
 * MessageQueue before it can push() onto that queue.
 *
 * A queue holds at most MSGQUEUE_HIGH_WATER items (default 16). A push to a
 * full queue waits for a pop to make room, or fails if you gave it a timeout.
 * A queue that stays full means its consumer is not keeping up: see doc/msgqueue.md.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<lockfree.h>
#include	<condition.h>
#include	<thread.h>
#include	<thread_local.h>
#include	<datetime.h>
#include	<variant.h>
#include	<watch.h>

#if	!defined(MSGQUEUE_HIGH_WATER)
#define	MSGQUEUE_HIGH_WATER	16
#endif

#if	defined(STRPP_MONITOR)
#include	<atomic>
#include	<registry.h>

// One queue as a monitor saw it
struct	MessageQueueRecord
{
	const void*		queue;
	const char*		name;		// Given to the queue, or null
	unsigned		depth;		// Items now
	unsigned		peak;		// The most items it has held
	unsigned		blocked;	// Threads now waiting for room
	unsigned		pushed;		// Items put in, over all time
	unsigned		popped;
	bool			has_consumer;	// Whether any thread has popped it
	ThreadId		consumer;	// The one that last did
};
#endif

class	MessageQueue
{
public:
	MessageQueue(const char* a_name = 0)
			: name(a_name), has_consumer(false), consumer()
#if	defined(STRPP_MONITOR)
			, stat_depth(0), stat_peak(0), stat_blocked(0), stat_pushed(0), stat_popped(0)
			, registry_next(0)
			{ Registry<MessageQueue>::instance().add(this); }
	~MessageQueue()	{ Registry<MessageQueue>::instance().remove(this); }
#else
			{}
	~MessageQueue() {}
#endif

	// A queue may not be copied: see Latch, which it holds one of
	MessageQueue(const MessageQueue&) = delete;
	MessageQueue&		operator=(const MessageQueue&) = delete;

	// Each of these waits while the queue is full, unless it has a timeout
	void		push(const Variant& item);
	void		push(const VariantArray& items);	// Several, in order, under one lock
	bool		push(const Variant& item, Milliseconds timeout);	// False if there was no room in time
	bool		push(const VariantArray& items, Milliseconds timeout);
	bool		try_push(const Variant& item)		// False if the queue is full
			{ return push(item, Milliseconds(0)); }
	bool		try_push(const VariantArray& items)
			{ return push(items, Milliseconds(0)); }

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

#if	defined(STRPP_MONITOR)
	// Every queue there is, as of now; returns how many it wrote
	static unsigned	snapshot(MessageQueueRecord* out, unsigned max_out);
#endif

private:
	Latch		latch;
	Condition	not_empty;
	Condition	not_full;
	VariantArray	items;
	const char*	name;
	bool		has_consumer;	// A thread has popped this queue...
	ThreadId	consumer;	// ...and this one did last: it is who a full queue waits for

#if	defined(STRPP_MONITOR)
	std::atomic<unsigned>	stat_depth;
	std::atomic<unsigned>	stat_peak;
	std::atomic<unsigned>	stat_blocked;
	std::atomic<unsigned>	stat_pushed;
	std::atomic<unsigned>	stat_popped;
	friend class	Registry<MessageQueue>;
public:
	MessageQueue*	registry_next;
private:
#endif

	void		note_pop();			// Latch held
	bool		wait_for_room(const Milliseconds* timeout);	// Latch held
	void		added(unsigned count);		// Latch held
	void		took();				// Latch held
};

inline void
MessageQueue::note_pop()
{
	has_consumer = true;
	consumer = Thread::currentId();
}

inline bool
MessageQueue::wait_for_room(const Milliseconds* timeout)
{
	if (items.length() < MSGQUEUE_HIGH_WATER)
		return true;

	// Waiting for room in a queue you pop is waiting for yourself
	StrppAssert(timeout || !has_consumer || consumer != Thread::currentId());

#if	defined(STRPP_MONITOR)
	stat_blocked++;
#endif
	WatchWait	waiting(this, WatchQueue, WatchForSpace, has_consumer ? &consumer : 0);
	long		remaining = timeout ? (long)timeout->ms() : 0;
	bool		room = true;
	while (items.length() >= MSGQUEUE_HIGH_WATER)
	{
		if (!timeout)
			not_full.wait(&latch);
		else if (remaining <= 0)
		{
			room = false;
			break;
		}
		else
			not_full.wait(remaining, &latch);
	}
#if	defined(STRPP_MONITOR)
	stat_blocked--;
#endif
	return room;
}

inline void
MessageQueue::added(unsigned count)
{
#if	defined(STRPP_MONITOR)
	unsigned	depth = (unsigned)items.length();
	stat_depth = depth;
	if (depth > stat_peak)
		stat_peak = depth;
	stat_pushed += count;
#else
	(void)count;
#endif
}

inline void
MessageQueue::took()
{
#if	defined(STRPP_MONITOR)
	stat_depth = (unsigned)items.length();
	stat_popped++;
#endif
	not_full.signal();
}

inline bool
MessageQueue::push(const Variant& item, Milliseconds timeout)
{
	latch.enter();
	bool		room = wait_for_room(&timeout);
	if (room)
	{
		items.push(item);
		added(1);
		not_empty.signal();
	}
	latch.leave();
	return room;
}

inline void
MessageQueue::push(const Variant& item)
{
	latch.enter();
	wait_for_room(0);
	items.push(item);
	added(1);
	not_empty.signal();
	latch.leave();
}

// A batch waits for room once, and may then take the queue past its high water
inline bool
MessageQueue::push(const VariantArray& to_add, Milliseconds timeout)
{
	latch.enter();
	bool		room = wait_for_room(&timeout);
	if (room)
	{
		for (VariantArray::Index i = 0; i < to_add.length(); i++)
			items.push(to_add[i]);
		added((unsigned)to_add.length());
		if (to_add.length() > 0)
			not_empty.signal();	// One signal wakes a waiter, who then drains what's there
	}
	latch.leave();
	return room;
}

inline void
MessageQueue::push(const VariantArray& to_add)
{
	latch.enter();
	wait_for_room(0);
	for (VariantArray::Index i = 0; i < to_add.length(); i++)
		items.push(to_add[i]);
	added((unsigned)to_add.length());
	if (to_add.length() > 0)
		not_empty.signal();
	latch.leave();
}

inline Variant
MessageQueue::pop()
{
	latch.enter();
	note_pop();
	while (items.isEmpty())
		not_empty.wait(&latch);
	Variant		item = items.shift();
	took();
	latch.leave();
	return item;
}

inline Variant
MessageQueue::pop(Milliseconds timeout)
{
	long		remaining_ms = (long)timeout.ms();
	latch.enter();
	note_pop();
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
	took();
	latch.leave();
	return item;
}

inline bool
MessageQueue::try_pop(Variant& item)
{
	latch.enter();
	note_pop();
	bool		got = !items.isEmpty();
	if (got)
	{
		item = items.shift();
		took();
	}
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
