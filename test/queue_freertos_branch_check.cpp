/*
 * Compile-check for the queue.h branches that cannot be run here.
 *
 * Nothing in the library instantiates a Queue, so without this the
 * HAVE_FREERTOS branch (via Latch/Condition) is never compiled at all, and
 * the no-threading-model branch never is either. This file is compiled twice
 * by `make freertos_check` - once against the FreeRTOS stub, once with no
 * threading model defined - and is never run or linked: the FreeRTOS stub
 * has no scheduler, so anything that ran it would hang.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<queue.h>

void	queue_branch_check()
{
	Queue*		mine = Queue::mine();
	mine->push(Variant(1));
	VariantArray	batch;
	batch.push(Variant(2));
	mine->push(batch);

	(void)mine->pop();
	(void)mine->pop(Milliseconds(10));

	Variant		item;
	(void)mine->try_pop(item);
	(void)mine->isEmpty();
}
