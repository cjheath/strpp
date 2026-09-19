/*
 * Compile-check for the thread_local.h branches that cannot be run here.
 *
 * Nothing in the library instantiates a ThreadSlot, so without this the
 * HAVE_FREERTOS branch is never compiled at all, and the fallback branch (no
 * threading model this library knows) never is either. This file is compiled
 * twice by `make freertos_check` - once against the FreeRTOS stub, once with
 * no threading model defined - and is never run or linked: the FreeRTOS stub
 * has no scheduler, so anything that ran it would hang.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<thread_local.h>

static ThreadSlot	slot;
static ThreadLocal<int>	value;

void	thread_local_branch_check()
{
	slot.set(0);
	(void)slot.get();

	(void)value.get();
	(void)value.peek();
	value.clear();
}
