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
#else

int			Latch::num_cores;

#endif
