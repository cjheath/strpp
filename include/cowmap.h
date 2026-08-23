#if !defined(COWMAP_H)
#define COWMAP_H
/*
 * A copy-on-write Map template.
 * You can cheaply pass a CowMap (passing it doesn't copy the contents).
 * When you try to change a CowMap that has any other reference,
 * only the root is (cheaply) copied before your change is attempted.
 *
 * Internally, it's a persistent (path-copying) red-black tree - see
 * redblack.h - so that cheap copy is O(1) (it shares the same root with
 * the original), and only the O(log n) nodes actually touched by a
 * subsequent mutation get copied; everything else remains shared with
 * whichever other CowMap(s) still reference the old version.
 */
#include	<cstdlib>
#include	<cstdint>
#include	<functional>
#include	<utility>

#include	<refcount.h>
#include	<strval.h>
#include	<redblack.h>

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
				body = new Body(*body);	// O(1): shares the same root, refcounted
			}
};

template<
	typename V,
	typename K
> class	CowMapBody
	: public RefCounted
{
	using	Tree = RbTree<K, V>;
	// TaggedRef<RbNode<K,V>> directly, not "typename Tree::Link": see the
	// note next to RbTree::Link in redblack.h - a dependent nested-type
	// lookup would require RbTree<K,V> (and by cascading, RbNode<K,V>,
	// which embeds V) to be complete merely to name this member's type,
	// which is exactly the problem in variant.h: StrVariantMap derives
	// from CowMap<Variant,StrVal> before Variant is complete.
	using	Link = TaggedRef<RbNode<K, V>>;

	Link	root;
	size_t	count;
public:
	using	Iter = typename Tree::Iter;
	using	Value = V;
	using	Key = K;
	using	value_type = std::pair<const K, V>;	// BaseVP in CowMap resolves to this, as it did via std::map

	CowMapBody() : root(), count(0) { }
	// Deliberately a real copy constructor, not deleted like ArrayBody's:
	// copying just copies root (an O(1) TaggedRef copy - an AddRef, not a
	// data copy) and count. That's safe and correct specifically because
	// the tree is persistent/shareable, unlike ArrayBody's flat mutable
	// buffer, where a shallow copy would be actively wrong. This is what
	// lets CowMap::Unshare() (above) be O(1) instead of rebuilding the
	// whole map element by element.
	CowMapBody(const CowMapBody& o) : root(o.root), count(o.count) { }

	Iter		find(const Key& k) const { return Tree::find(root, k); }
	Iter		begin() const { return Tree::begin(root); }
	Iter		end() const { return Tree::end(); }
	size_t		size() const { return count; }

	void		insert(value_type kv)
			{
				if (find(kv.first) == end())
					count++;
				Tree::insert(root, kv.first, kv.second);
			}
	void		erase(const Key& k)
			{
				if (find(k) != end())
					count--;
				Tree::erase(root, k);
			}
};
#endif // COWMAP_H
