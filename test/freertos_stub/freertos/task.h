#if !defined(FREERTOS_STUB_TASK_H)
#define FREERTOS_STUB_TASK_H
/*
 * COMPILE-CHECK-ONLY stub - see FreeRTOS.h in this directory for what this is
 * (and, importantly, is not).
 */
#include	"FreeRTOS.h"

struct	tskTaskControlBlock;
typedef struct tskTaskControlBlock*	TaskHandle_t;
typedef void (*TaskFunction_t)(void*);

inline BaseType_t
xTaskCreate(
	TaskFunction_t	pxTaskCode,
	const char*	pcName,
	uint32_t	usStackDepth,
	void*		pvParameters,
	UBaseType_t	uxPriority,
	TaskHandle_t*	pxCreatedTask
)
{
	(void)pxTaskCode; (void)pcName; (void)usStackDepth; (void)pvParameters; (void)uxPriority;
	if (pxCreatedTask)
		*pxCreatedTask = (TaskHandle_t)1;	// non-null, so id()/assert(t->id()) checks pass
	return pdPASS;
}

inline void		vTaskDelete(TaskHandle_t) {}
inline void		vTaskSuspend(TaskHandle_t) {}
inline void		vTaskResume(TaskHandle_t) {}
inline void		vTaskDelay(TickType_t) {}
inline void		taskYIELD_stub() {}
#define			taskYIELD()	taskYIELD_stub()
inline TaskHandle_t	xTaskGetCurrentTaskHandle() { return (TaskHandle_t)1; }
inline TickType_t	xTaskGetTickCount() { return 0; }

#endif
