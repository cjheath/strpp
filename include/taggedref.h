#if !defined(TAGGEDREF_H)
#define TAGGEDREF_H
/*
 * TaggedRef<T> - a reference-counted smart pointer to a RefCounted object,
 * which also stores a small integer "tag" in the pointer's own low-order
 * bits. Those bits are always zero in a validly-aligned T*, so packing a
 * tag into them costs no extra memory - intended for structures like a
 * persistent tree, where a child pointer's own storage can also carry a
 * per-edge marker (e.g. a red-black colour) at zero space cost.
 *
 * TaggedRef<T> is deliberately NOT a subclass of Ref<T> (see refcount.h).
 * Ref<T>'s destructor/copy-constructor/operator= are not virtual, and C++
 * always runs a base class's destructor after a derived one finishes, so
 * a subclass reusing Ref<T>'s own pointer storage for tagging would need
 * every one of those base operations to also know about masking first -
 * a hazard not worth taking on for a handful of duplicated lines. This
 * class is small and self-contained instead, and Ref<T> itself is
 * untouched.
 *
 * T must derive from RefCounted, exactly as for Ref<T>.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdint>
#include	<assert.h>

#include	<refcount.h>

template<class T>
class	TaggedRef
{
public:
	/*
	 * Every low-order bit guaranteed zero by T's alignment is available as a
	 * tag bit. Deliberately using static member *functions*, not static const
	 * data members with an in-class initializer.
	 * The latter would force alignof(T) to be evaluated (and so T to be a
	 * complete type) as soon as TaggedRef<T> is merely named as a type,
	 * - e.g. as another class's data member - whereas a function body,
	 * like any other member function, is only instantiated when actually
	 * called, deferring the completeness requirement to first real use.
	 * This matters in practice: a template using this may use
	 * TaggedRef<RbNode<K,V>> before V (e.g. Variant, in variant.h) is
	 * complete.
	 */
	static uintptr_t
			TagMask() { return alignof(T) - 1; }
	static uintptr_t
			PtrMask() { return ~TagMask(); }

	~TaggedRef()
			{ T* o = get(); if (o) o->Release(); }
	TaggedRef()					// Empty
			: bits(0) {}
	TaggedRef(T* o, uintptr_t tag = 0)		// Construct from a raw pointer (+ optional tag)
			: bits(pack(o, tag))
			{ if (o) o->AddRef(); }
	TaggedRef(const TaggedRef& other)		// Normal copy constructor
			: bits(other.bits)
			{ T* o = get(); if (o) o->AddRef(); }

	TaggedRef&	operator=(const TaggedRef& other)
			{
				T*	o = other.get();
				if (o)
					o->AddRef();
				T*	old = get();
				bits = other.bits;
				if (old)
					old->Release();
				return *this;
			}
	TaggedRef&	operator=(T* other)		// Keep the current tag, change the pointee
			{
				if (other)
					other->AddRef();
				T*	old = get();
				bits = pack(other, Tag());
				if (old)
					old->Release();
				return *this;
			}

	T*		get() const { return reinterpret_cast<T*>(bits & PtrMask()); }
			operator T*() const { return get(); }
	T*		operator->() const { assert(get()); return get(); }
	T&		operator*() const { assert(get()); return *get(); }
			operator bool() const { return get() != 0; }

	uintptr_t	Tag() const { return bits & TagMask(); }
	void		SetTag(uintptr_t tag)		// Change only the tag; the pointee (and its refcount) is untouched
			{
				assert((tag & ~TagMask()) == 0);
				bits = (bits & PtrMask()) | tag;
			}
	TaggedRef	WithTag(uintptr_t tag) const	// A new reference to the same pointee, with a different tag
			{ return TaggedRef(get(), tag); }

	bool		operator==(const TaggedRef& other) const { return bits == other.bits; }
	bool		operator!=(const TaggedRef& other) const { return bits != other.bits; }

	// Requires T::GetRefCount(). There is deliberately no generic Unshare():
	// nodes here are meant to be replaced by freshly-constructed nodes with
	// explicit field values (see taggedref.h's header comment), not cloned
	// via a copy constructor and then mutated - and, matching the rest of
	// this codebase's RefCounted "body" types (e.g. ArrayBody), a node type
	// is expected to disable copying outright.
	int		GetRefCount() const
			{ T* o = get(); return o ? o->GetRefCount() : 0; }

private:
	uintptr_t	bits;

	static uintptr_t
			pack(T* o, uintptr_t tag)
			{
				assert((reinterpret_cast<uintptr_t>(o) & TagMask()) == 0);	// o must be properly aligned
				assert((tag & ~TagMask()) == 0);					// tag must fit in the spare bits
				return reinterpret_cast<uintptr_t>(o) | tag;
			}
};
#endif // TAGGEDREF_H
