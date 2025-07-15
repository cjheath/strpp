#if !defined(COWMAP_H)
#define COWMAP_H
/*
 * A copy-on-write Map template.
 * You can cheaply pass a CowMap (passing it doesn't copy the contents).
 * When you try to change a CowMap that has any other reference,
 * the entire map is copied before your change is attempted.
 *
 * Internally, it's just a std::map, which is a red-black tree.
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
protected:
	using	BaseVP = typename Body::value_type;	// Key-Value pair, the way std:map likes it.
public:
	using	Iter = typename Body::Iter;	// Mutating a CowMap while iterating will mutate a fresh Body
	using	Value = V;
	using	Key = K;

	~CowMap() { }				// Destructor
	CowMap()				// Empty map
			: body(new Body()) { }
	CowMap(const CowMap& s1)		// Normal copy constructor
			: body(s1.body) { }
	CowMap(const Key* keys, const Value* values, int size)	// construct by copying data
			: body(0)
			{
				body = new Body();
				for (int i = 0; i < size; i++)
					body->insert(BaseVP(keys[i], values[i]));
			}
	CowMap& operator=(const CowMap& s1)	// Assignment operator
			{ body = s1.body; return *this; }

	Value	operator[](const Key& k)
			{
				auto	it = (*body).find(k);
				if (it != (*body).end())
					return it->second;
				return Value();
			}
	bool	contains(const Key& k)
			{
				auto search = find(k);
				return search != end();
			}
	Iter	find(const Key& k) { return body->find(k); }
	Iter	begin() const
			{ return body->begin(); }
	Iter	end() const
			{ return body->end(); }
	size_t	size() const
			{ return body->size(); }

	// Mutating methods:
	void	clear() { body = new Body(); }
	void	insert(const Key k, const Value v)
			{ Unshare(); body->insert(BaseVP(k, v)); }
	void	remove(const Key& k)
			{ Unshare(); body->erase(k); }
	Key	put(const Key& k, Value v)
			{
				auto search = find(k);
				if (search != end())
					body->erase(k);
				insert(k, v);
				return k;
			}

	// Functional methods (these don't mutate or Unshare the subject):
	Array<Key>	keys() const
			{
				Array<K>	all_keys((Value*)0, 0, size());
				for (Iter it = begin(); it != end(); it++)
					all_keys.append(it.first);
				return all_keys;
			}
	Array<Value>	values() const
			{
				Array<Value>	all_values((Value*)0, 0, size());
				for (Iter it = begin(); it != end(); it++)
					all_values.append(it.second);
				return all_values;
			}

	void		each(std::function<void(const Key& k, const Value& v)> operation) const
			{
				for (Iter it = begin(); it != end(); it++)
					operation(it.first, it.second);
			}
	bool		all(std::function<bool(const Key& k, const Value& v)> condition) const	// Do all elements satisfy the condition?
			{
				for (Iter it = begin(); it != end(); it++)
					if (!condition(it.first, it.second))
						return false;
				return true;
			}
	bool		any(std::function<bool(const Key& k, const Value& v)> condition) const	// Does any element satisfy the condition?
			{
				for (Iter it = begin(); it != end(); it++)
					if (condition(it.first, it.second))
						return true;
				return false;
			}
	bool		one(std::function<bool(const Key& k, const Value& v)> condition) const	// Exactly one element satisfies the condition
			{
				bool		found = false;
				for (Iter it = begin(); it != end(); it++)
					if (condition(it.first, it.second))
					{
						if (found)
							return false;
						found = true;
					}
				return found;
			}

	CowMap		select(std::function<bool(const Key& k, const Value& v)> condition) const
			{
				CowMap	selected;

				for (Iter it = begin(); it != end(); it++)
					if (condition(it.first, it.second))
						selected.put(it.first, it.second);
				return selected;
			}
	template<typename V2 = V, typename K2 = K>
	CowMap<V2, K2>	map(std::function<std::pair<K2, V2>(const Key& k, const Value& v)> map1) const
			{
				CowMap<V2, K2>	output;
				for (Iter it = begin(); it != end(); it++)
					output.append(map1(it.first, it.second));
				return output;
			}
	template<typename J>
	J		inject(const J& start, std::function<J(J&, const Key& k, const Value& v)> injection) const
			{
				J	accumulator = start;
				for (Iter it = begin(); it != end(); it++)
					accumulator = injection(accumulator, it.first, it.second);
				return accumulator;
			}

private:
	Ref<Body>	body;		// The storage structure for the elements

	void		Unshare()	// Get our own copy of Body that we can safely mutate
			{
				if (body && body->GetRefCount() <= 1)
					return;

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
