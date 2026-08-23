#if !defined(FREERTOS_STUB_SEMPHR_H)
#define FREERTOS_STUB_SEMPHR_H
/*
 * COMPILE-CHECK-ONLY stub - see FreeRTOS.h in this directory for what this is
 * (and, importantly, is not).
 */
#include	"FreeRTOS.h"
#include	"task.h"

struct	StaticSemaphore_stub;
typedef struct StaticSemaphore_stub*	SemaphoreHandle_t;

inline SemaphoreHandle_t	xSemaphoreCreateMutex() { return (SemaphoreHandle_t)1; }
inline SemaphoreHandle_t	xSemaphoreCreateRecursiveMutex() { return (SemaphoreHandle_t)1; }
inline void			vSemaphoreDelete(SemaphoreHandle_t) {}
inline BaseType_t		xSemaphoreTake(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
inline BaseType_t		xSemaphoreGive(SemaphoreHandle_t) { return pdTRUE; }
inline BaseType_t		xSemaphoreTakeRecursive(SemaphoreHandle_t, TickType_t) { return pdTRUE; }
inline BaseType_t		xSemaphoreGiveRecursive(SemaphoreHandle_t) { return pdTRUE; }
inline TaskHandle_t		xSemaphoreGetMutexHolder(SemaphoreHandle_t) { return (TaskHandle_t)0; }

#endif
