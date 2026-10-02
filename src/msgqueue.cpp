/*
 * MessageQueue.
 *
 * Make a snapshot of the state of a MessageQueue to support monitoring.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<msgqueue.h>

#if	defined(STRPP_MONITOR)

unsigned
MessageQueue::snapshot(MessageQueueRecord* out, unsigned max_out)
{
	unsigned	n = 0;
	Registry<MessageQueue>::instance().each([&](MessageQueue& q)
	{
		if (n >= max_out)
			return;
		MessageQueueRecord&	r = out[n++];
		r.queue = &q;
		r.name = q.name;
		r.depth = q.stat_depth;
		r.peak = q.stat_peak;
		r.blocked = q.stat_blocked;
		r.pushed = q.stat_pushed;
		r.popped = q.stat_popped;
		r.has_consumer = q.has_consumer;		// May be a moment old: see watch.h
		r.consumer = q.consumer;
	});
	return n;
}

#endif
