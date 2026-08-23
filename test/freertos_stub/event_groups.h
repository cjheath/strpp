#if !defined(FREERTOS_STUB_EVENT_GROUPS_H)
#define FREERTOS_STUB_EVENT_GROUPS_H
/*
 * COMPILE-CHECK-ONLY stub - see FreeRTOS.h in this directory for what this is
 * (and, importantly, is not).
 */
#include	"FreeRTOS.h"

struct	EventGroup_stub;
typedef struct EventGroup_stub*	EventGroupHandle_t;

inline EventGroupHandle_t	xEventGroupCreate() { return (EventGroupHandle_t)1; }
inline void			vEventGroupDelete(EventGroupHandle_t) {}
inline EventBits_t		xEventGroupSetBits(EventGroupHandle_t, EventBits_t bits) { return bits; }
inline EventBits_t		xEventGroupClearBits(EventGroupHandle_t, EventBits_t bits) { return bits; }
inline EventBits_t
xEventGroupWaitBits(
	EventGroupHandle_t	group,
	EventBits_t		uxBitsToWaitFor,
	BaseType_t		xClearOnExit,
	BaseType_t		xWaitForAllBits,
	TickType_t		xTicksToWait
)
{
	(void)group; (void)xClearOnExit; (void)xWaitForAllBits; (void)xTicksToWait;
	return uxBitsToWaitFor;
}

#endif
