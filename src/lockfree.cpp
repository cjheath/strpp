/*
 * Lockfree locking primitives based on atomic transfers
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */
#include	<assert.h>
#include	<lockfree.h>

#if	defined(HAVE_PTHREADS)
pthread_mutexattr_t	Latch::attr;
std::atomic<bool>	Latch::initialised;
#elif	defined(HAVE_FREERTOS)
// No static state needed: Latch's mutex is a per-instance SemaphoreHandle_t.
#elif	defined(MSW)

int			Latch::num_cores;

#else
// NO_THREAD, or no model selected (which thread.h reports): no static state
#endif
