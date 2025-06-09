#if !defined(COWMAP_H)
#define COWMAP_H
/*
 * A copy-on-write Map template.
 */
#include	<cstdlib>
#include	<cstdint>
#include	<functional>
#include	<map>

#include	<refcount.h>
#include	<strval.h>

template<typename V, typename K> class CowMapBody;
template<typename V, typename K = StrVal, typename Body = CowMapBody<V, K>> class CowMap;

template<
	typename V,
	typename K,
	typename Body
> class	CowMap
{
	using	BaseVP = typename Body::value_type;	// Key-Value pair, the way std:map likes it.
public:
	using	Iter = typename Body::Iter;	// Mutating a CowMap while iterating will mutate a fresh Body
	using	Value = V;
	using	Key = K;

	~CowMap() {}				// Destructor
	CowMap()				// Empty map
			: body(0)
			{
				Body* b = 0;
				b = new Body();
				body = b;
			}
	CowMap(const CowMap& s1)		// Normal copy constructor
			: body(s1.body) {}
	CowMap& operator=(const CowMap& s1)	// Assignment operator
			{ body = s1.body; return *this; }
	CowMap(const Key* keys, const Value* values, int size)	// construct by copying data
			: body(0)
			{
				body = new Body();
				for (int i = 0; i < size; i++)
					body.insert(BaseVP(keys[i], values[i]));
			}
	void	insert(const Key k, const Value v)
			{
				Unshare();
				body->insert(BaseVP(k, v));
			}
	Value	operator[](const Key& k)
			{
				return (*body)[k];
			}
	Iter	begin() { return body->begin(); }
	Iter	end() { return body->end(); }

private:
	Ref<Body>	body;		// The storage structure for the elements

	void		Unshare()	// Get our own copy of Body that we can safely mutate
			{
				if (body && body->GetRefCount() <= 1)
					return;

				/*
				printf(
					"Unsharing a CowMap with %d references and %ld entries\n",
					body->GetRefCount(), body->size()
				);
				*/

				// Copy the old body's data
				Body* newbody = new Body();
				for (Iter it = begin(); it != end(); it++)
					newbody->insert(BaseVP(it->first, it->second));
				body = newbody;
			}
};

template<
	typename V,
	typename K
> class	CowMapBody
	: public std::map<K, V>
	, public RefCounted
{
	using	Base = std::map<K, V>;
public:
	using	Iter = typename Base::const_iterator;
	using	Value = V;
	using	Key = K;

	CowMapBody() { }
};
#endif // COWMAP_H
