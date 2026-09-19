#if !defined(THREAD_LOCAL_H)
#define THREAD_LOCAL_H
/*
 * Thread-local storage: a slot holds one void* per thread.
 *
 * Slots are process-wide and few, because every backend offers only a small
 * pool of them: pthreads a key, Windows a TLS index, FreeRTOS a compile-time
 * index into a fixed array of pointers per task. Claiming is therefore meant to
 * happen early and rarely - one slot per subsystem that needs one - and
 * exhausting the pool is a failure at startup rather than under load.
 *
 * There are deliberately no thread-exit destructors. A pthread key can run one,
 * but TlsAlloc and FreeRTOS task-local storage cannot, and behaviour that
 * differs by backend is worse than uniformly not having it. So what a thread
 * puts in a slot is released by the process, or by whoever owns the thread, or
 * by ThreadLocal::clear() - not by the slot.
 *
 * Not a compiler thread_local: that works on pthreads and Windows but not on
 * FreeRTOS, where it needs toolchain TLS-area support, so it cannot cover all
 * the models this library builds for.
 *
 * FreeRTOS builds must raise configNUM_THREAD_LOCAL_STORAGE_POINTERS (it
 * defaults to 0) to at least THREAD_LOCAL_MAX_SLOTS.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<assert.h>

#include	<threadid.h>

#if	!defined(THREAD_LOCAL_MAX_SLOTS)
#define	THREAD_LOCAL_MAX_SLOTS	16
#endif

/*
 * One void* per thread, per slot. Claimed at construction; released at
 * destruction, which for a slot claimed during static initialisation means
 * never - deliberately, since a slot outlives most things that might free it.
 */
class	ThreadSlot
{
public:
#if	defined(HAVE_PTHREADS)
	ThreadSlot()
			{
				claim();
				// No destructor function: see the note at the top of this file
				assert(pthread_key_create(&key, 0) == 0);
			}
	~ThreadSlot()
			{ pthread_key_delete(key); }

	void*		get() const	{ return pthread_getspecific(key); }
	void		set(void* v)	{ pthread_setspecific(key, v); }

private:
	pthread_key_t	key;

#elif	defined(HAVE_FREERTOS)
	ThreadSlot()
			{
				index = claim();
				assert(index < configNUM_THREAD_LOCAL_STORAGE_POINTERS);
			}
	~ThreadSlot()
			{ }

	void*		get() const
			{
				return pvTaskGetThreadLocalStoragePointer(
						xTaskGetCurrentTaskHandle(), index
					);
			}
	void		set(void* v)
			{
				vTaskSetThreadLocalStoragePointer(
						xTaskGetCurrentTaskHandle(), index, v
					);
			}

private:
	BaseType_t	index;

#elif	defined(MSW)
	ThreadSlot()
			{
				claim();
				index = TlsAlloc();
				assert(index != TLS_OUT_OF_INDEXES);
			}
	~ThreadSlot()
			{ if (index != TLS_OUT_OF_INDEXES) TlsFree(index); }

	void*		get() const	{ return TlsGetValue(index); }
	void		set(void* v)	{ TlsSetValue(index, v); }

private:
	DWORD		index;

#else
	/*
	 * No threading at all, or a threading model this library knows nothing
	 * about: every slot is a plain global, so there is one thread's worth.
	 */
	ThreadSlot()
			{ index = claim(); }
	~ThreadSlot()
			{ }

	void*		get() const	{ return values()[index]; }
	void		set(void* v)	{ values()[index] = v; }

private:
	int		index;

	static void**	values()
			{
				static void*	v[THREAD_LOCAL_MAX_SLOTS];
				return v;
			}
#endif

private:
	// Claim the next slot index, asserting that the pool has one left
	static int	claim()
			{
				int&	n = count();
				assert(n < THREAD_LOCAL_MAX_SLOTS);
				return n++;
			}
	static int&	count()		{ static int n = 0; return n; }
};

/*
 * A slot holding one object of type T per thread, created on first use in each
 * thread. get() is what the reporting path of the error system will call, so
 * after the first call in a thread it is one thread-local lookup and nothing
 * else.
 */
template<typename T>
class	ThreadLocal
{
public:
	T*	get()			// This thread's object, created if it has none
			{
				T*	p = (T*)slot.get();
				if (!p)
					slot.set(p = new T);
				return p;
			}
	T*	peek() const		{ return (T*)slot.get(); }	// May be null

	void	clear()			// Destroy this thread's object, if it has one
			{
				T*	p = (T*)slot.get();
				if (p)
				{
					slot.set(0);	// Before the destructor: it must not see itself
					delete p;
				}
			}

private:
	ThreadSlot	slot;
};

#endif	// THREAD_LOCAL_H
