#if !defined(REFCOUNT_H)
#define REFCOUNT_H
/*
 * Reference counting with delete on last release
 */
#include	<atomic>
class	RefCounted
{
public:
	virtual		~RefCounted() { }
			RefCounted() : ref_count(0) {}
	void		AddRef() { (void)ref_count++; }
	void		Release() { if (--ref_count == 0) delete this; }
			// Only for debugging, may be instantly stale unless == 1:
	int		GetRefCount() volatile const { return (int)ref_count; }

protected:
        std::atomic<int>	ref_count;
};


template <class T>
class Ref
{
	std::atomic<T*>	ptr;

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
				o = (T*)ptr.exchange(o);
				if (o)
					o->Release();
				return *this;
			}
	Ref&		operator=(T* other)
			{
				if (other)
					other->AddRef();

				T*      o = (T*)ptr.exchange(other);
				if (o)
					o->Release();
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
