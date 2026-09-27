#if !defined(REFCOUNT_H)
#define REFCOUNT_H
/*
 * Thread-safe reference counting with delete on last release
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include	<assert.h>
#include	<atomic>
#include	<climits>

#include	<strassert.h>			// A count that cannot be right stops the program

class	RefCounted
{
public:
	virtual		~RefCounted() { }
			RefCounted() : ref_count(0) {}

	/*
	 * The count is never allowed to wrap, and the test is of what the atomic
	 * itself returned - testing a second load of it, after the increment, is
	 * how the check here came to be dead code that never fired.
	 *
	 * A wrapped count is not a wrong number in a corner: GetRefCount() <= 1 is
	 * how Array, StrVal, CowMap and RbTree decide that a body is theirs to
	 * write through, so a count past its end makes a shared body look private,
	 * and the next write corrupts data another value still holds. A count that
	 * has gone below zero never reaches zero again, so the body is never freed.
	 * Neither is recoverable by returning something else, so neither carries
	 * on: they stop, with the reason reported, in every build.
	 *
	 * A report could not be made from here even if one were wanted: Error()
	 * builds a VariantArray, whose construction calls AddRef.
	 */
	void		AddRef()
			{
				StrppAssert(ref_count.fetch_add(1) != INT_MAX);
			}
	void		Release()
			{
				int	was = ref_count.fetch_sub(1);
				StrppAssert(was > 0);	// Released more often than it was referenced
				if (was == 1)
					delete this;
			}
			// Only for debugging, may be instantly stale unless == 1:
	int		GetRefCount() volatile const { return (int)ref_count; }

protected:
        std::atomic<int>	ref_count;
};

template <class T>
class Ref
{
	T*	ptr;	// This does not need to be atomic, only the ref_count
	// std::atomic<T*>	ptr;

public:
			~Ref() { if (*this) (*this)->Release(); }
			Ref() : ptr(0) {}
			Ref(T* o) { if (o) o->AddRef(); ptr = o; }
			Ref(const Ref& other) { T* o = other; if (o) o->AddRef(); ptr = o; }
	Ref&		operator=(const Ref& other)
			{
				T*      o = other;
				if (o)
					o->AddRef();
				T*      old = ptr;
				ptr = other;
				if (old)
					old->Release();
				return *this;
			}
	Ref&		operator=(T* other)
			{
				if (other)
					other->AddRef();
				T*      old = ptr;
				ptr = other;
				if (old)
					old->Release();
				return *this;
			}

			operator T*() const { return ptr; }
	T*		operator->() const { return ptr; }
	T&		operator*() const { return *ptr; }

	// Casting to other Ref types
	template <class S> Ref<S>
			Cast()
			{ return Ref<S>(static_cast<S*>(*this)); }

	bool		operator==(const Ref& other) const { return *this == *other; }
	bool		operator!=(const Ref& other) const { return *this != *other; }
	bool		operator<(const Ref& other) const { return *this < *other; }
	bool		operator>(const Ref& other) const { return *this > *other; }
			operator bool() const { return ptr != 0; }

	// The following methods require T::GetRefCount():
	void		Unshare()	// For classes that want to hide mutation
			{
				T*      o = (T*)ptr;
				if (!o || o->GetRefCount() <= 1)
					return;
				// Copy ref'd object
				*this = new T(*o);
			}
	int		GetRefCount()
			{	// If the value is > 1 another thread might change it before we use it
				T*      o = (T*)ptr;
				return o ? o->GetRefCount() : 0;
			}
};

template<class T>
inline bool
operator==(const Ref<T>& r1, const Ref<T>& r2)
{
        return *r1 == *r2;
}
#endif
