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

#include	<FreeRTOS.h>
#include	<task.h>

typedef TaskHandle_t	ThreadId;
typedef int		ProcessId;	// No real concept of a "process" under FreeRTOS; see currentProcessId()

#elif	defined(MSW)

typedef DWORD		ThreadId;
typedef ThreadId	ProcessId;

//#else Add more threading systems here
#endif

#endif	// THREAD_ID
