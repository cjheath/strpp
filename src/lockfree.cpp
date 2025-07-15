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
#else

int			Latch::num_cores;

#endif
