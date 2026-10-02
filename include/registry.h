#if !defined(REGISTRY_H)
#define REGISTRY_H
/*
 * Registry.
 *
 * Keeps a list of every live object of one kind, so that a monitor can visit them.
 *
 * An object joins with add() when it is made and leaves with remove() when it
 * goes. It carries its own link, a pointer called registry_next. The registry
 * holds a Latch while visiting, so what each() calls must not wait for anything.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<lockfree.h>

template<class T>
class	Registry
{
public:
	// The one registry for T, made when it is first asked for
	static Registry&	instance()
				{ static Registry the_registry; return the_registry; }

	void		add(T* t)
			{
				latch.enter();
				t->registry_next = head;
				head = t;
				latch.leave();
			}

	void		remove(T* t)
			{
				latch.enter();
				for (T** p = &head; *p; p = &(*p)->registry_next)
					if (*p == t)
					{
						*p = t->registry_next;
						break;
					}
				latch.leave();
			}

	// Call f(T&) for each. It runs with the registry held, so it must not wait.
	template<class F>
	void		each(F f)
			{
				latch.enter();
				for (T* t = head; t; t = t->registry_next)
					f(*t);
				latch.leave();
			}

private:
	Registry() : head(0) {}

	Latch		latch;
	T*		head;
};

#endif	// REGISTRY_H
