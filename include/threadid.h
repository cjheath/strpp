#if	!defined(THREAD_ID)
#define	THREAD_ID
/*
 * Define ThreadId for use in lockfree.h and thread.h
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */

#if	defined(HAVE_PTHREADS)
#include	<unistd.h>
#include	<cerrno>
#include	<pthread.h>

typedef pthread_t	ThreadId;
typedef pid_t		ProcessId;

#elif	defined(HAVE_FREERTOS)

#include	<freertos/FreeRTOS.h>
#include	<freertos/task.h>

typedef TaskHandle_t	ThreadId;
typedef int		ProcessId;	// No real concept of a "process" under FreeRTOS; see currentProcessId()

#elif	defined(MSW)

typedef DWORD		ThreadId;
typedef ThreadId	ProcessId;

#elif	defined(NO_THREAD)
/*
 * No threading at all. The library still builds and runs, with one thread and
 * locks that never block; anything that would create a thread cannot be used.
 */
typedef int		ThreadId;
typedef int		ProcessId;

#else
/*
 * No model selected. These stand in only so that the headers below compile far
 * enough for thread.h to say so, which is where the error belongs.
 */
typedef int		ThreadId;
typedef int		ProcessId;
#endif

#endif	// THREAD_ID
