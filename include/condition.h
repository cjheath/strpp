#if !defined(CONDITION_H)
#define CONDITION_H
/*
 * Condition variable
 *
 * Any thread waiting on a condition variable sleeps until the condition is signalled.
 * When it is, one or all waiting threads continue. Until signalled threads have
 * continued, any thread that tries to signal it again will block.
 *
 * This implementation is the generation-count version from
 * <http://www.cs.wustl.edu/~schmidt/win32-cv-1.html>.
 * It tries to provide fairness, but doesn't completely guarantee it.
 *
 * (c) Copyright Clifford Heath 2025. See LICENSE file for usage rights.
 */

#include <lockfree.h>

#if	defined(HAVE_FREERTOS)
#include <freertos/event_groups.h>
#endif

class Condition
{
public:
	~Condition();
	Condition();
	bool			ok() const;

	void			wait(
					Latch* = 0
				);
	void			wait(
					long&	delay_ms,
					Latch* = 0
				);

	void			signal();	// Signal one thread to wake
	void			broadcast();	// Signal all threads to wake
#if	!defined(HAVE_PTHREADS)
	int			waiters() { return waiters_count; }
#endif

private:
#if	defined(HAVE_PTHREADS)
	pthread_cond_t		cond;
#elif	defined(HAVE_FREERTOS)
	/*
	 * The same generation-count algorithm as the MSW implementation below (Schmidt's
	 * technique, <http://www.cs.wustl.edu/~schmidt/win32-cv-1.html>), built on an
	 * EventGroupHandle_t in place of a Win32 manual-reset Event: FreeRTOS has no
	 * native condition variable primitive, but xEventGroupSetBits/ClearBits/WaitBits
	 * have essentially the same shape as SetEvent/ResetEvent/WaitForSingleObject.
	 */
	Latch			latch;
	int			waiters_count;	// Number of threads waiting
	int			release_count;	// Number of threads to release
	int			generation_count; // Fairness control
	EventGroupHandle_t	eventGroup;
#elif	defined(MSW)
	Latch			latch;
	int			waiters_count;	// Number of threads waiting
	int			release_count;	// Number of threads to release
	int			generation_count; // Fairness control
	HANDLE			hEvent;
#else
#error	"Condition variables are not implemented"
#endif
};

#endif /* CONDITION_HXX */
