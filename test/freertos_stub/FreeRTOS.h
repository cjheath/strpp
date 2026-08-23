#if !defined(FREERTOS_STUB_FREERTOS_H)
#define FREERTOS_STUB_FREERTOS_H
/*
 * A MINIMAL, COMPILE-CHECK-ONLY stand-in for FreeRTOS's own FreeRTOS.h.
 *
 * This is NOT a real FreeRTOS port: there is no scheduler, no task switching,
 * nothing here actually behaves like an RTOS. Its only purpose is to let the
 * HAVE_FREERTOS branches of thread.h/thread.cpp/condition.h/condition.cpp/
 * lockfree.h/lockfree.cpp be compiled (not linked, not run) against
 * declarations shaped like the real FreeRTOS API, to catch transcription
 * errors (wrong parameter types/counts, wrong constant names) that a purely
 * by-inspection port could miss. It says nothing about runtime correctness -
 * that needs real target hardware or QEMU with the genuine FreeRTOS sources.
 *
 * Types and constants here are declared with the same names and (as far as
 * this project's usage is concerned) call signatures as real FreeRTOS, per
 * https://www.freertos.org/ - not copied from FreeRTOS source, which is
 * separately licensed.
 */
#include	<cstdint>

typedef long		BaseType_t;
typedef unsigned long	UBaseType_t;
typedef uint32_t	TickType_t;
typedef uint32_t	StackType_t;
typedef uint32_t	EventBits_t;

#define	pdTRUE		((BaseType_t)1)
#define	pdFALSE		((BaseType_t)0)
#define	pdPASS		pdTRUE
#define	pdFAIL		pdFALSE

#define	portMAX_DELAY		((TickType_t)0xFFFFFFFFUL)
#define	portTICK_PERIOD_MS	1
#define	pdMS_TO_TICKS(ms)	((TickType_t)(ms))

#endif
