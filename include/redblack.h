#if !defined(REDBLACK_H)
#define REDBLACK_H
/*
 * A persistent (path-copying) Left-Leaning Red-Black tree, used as the
 * underlying map for CowMap (see cowmap.h).
 *
 * - Nodes are reference-counted (RbNode : RefCounted) and their left/right
 *   children are TaggedRef<RbNode> (see taggedref.h), which packs the
 *   red/black colour of the link into the pointer's own spare low-order
 *   bits at zero extra space cost. Colour is a property of the *edge*
 *   (the parent's link to a child), not the node itself - correct for a
 *   persistent tree, since the same physical (shared) node can legitimately
 *   be reached via a red link from one older tree snapshot and a black
 *   link from a newer one simultaneously (this happens naturally after a
 *   rotation copies the parent but reuses the child).
 *
 * - A node with refcount == 1 is mutated in place; a node with refcount > 1
 *   (shared with another snapshot) is cloned first (same key/value/
 *   left/right, same tag), and the clone is mutated instead, leaving the
 *   shared original completely untouched. This is safe given a project-wide
 *   guarantee: a single Ref/TaggedRef is only ever visible to one thread at
 *   a time, and sharing across threads always goes through copying it - so
 *   refcount == 1 means nobody else, anywhere, can be concurrently
 *   observing or racing on that node.
 *
 * - The algorithm is Left-Leaning Red-Black (Sedgewick & Wayne, as used in
 *   "Algorithms, 4th Edition"), which collapses red-black deletion's usual
 *   many rebalancing cases down to three primitives (rotateLeft, rotateRight,
 *   flipColors) composed into moveRedLeft/moveRedRight/balance. Below, each
 *   function's original mutable pseudocode is given in a comment
 *   immediately above its translation into this in-place/by-reference,
 *   refcount-gated style, so the translation can be checked directly
 *   against the reference algorithm. Every mutating function takes a
 *   Link& - a reference to wherever the caller's link actually lives (a
 *   parent's field, or the top-level root variable) - and mutates it in
 *   place, rather than returning a new Link to be reassigned.
 *
 * - The one systematic difference from Sedgewick's original worth flagging:
 *   his colour is a mutable field on a shared object, so within a single
 *   function, reassigning it after a value has already been stored
 *   elsewhere is still visible through that stored reference. Here, a
 *   Link's tag is copied *by value* at the moment it's stored into another
 *   field, so anywhere a link is both re-tagged and stored into another
 *   field, the tag must be set *before* the store, not after (see
 *   rotateLeft/rotateRight).
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cstdint>

#include	<refcount.h>
#include	<taggedref.h>
#include	<array.h>

static const uintptr_t	RB_BLACK = 0;
static const uintptr_t	RB_RED = 1;

template<class K, class V>
class	RbNode
: public RefCounted
{
public:
	using	Link = TaggedRef<RbNode>;	// tag RB_BLACK/RB_RED = colour of the link reaching this node

	// Not const: mutated in place once ensureUnique() has confirmed this
	// node is uniquely owned (see RbTree::ensureUnique). Never mutated
	// otherwise - the discipline is centralised entirely in RbTree.
	K	key;
	V	value;
	Link	left, right;

	RbNode(const K& k, const V& v, Link l, Link r) : key(k), value(v), left(l), right(r) { ++live_count; }
	RbNode(const RbNode&) = delete;		// matches ArrayBody's convention - never copy a node via this ctor
	~RbNode() { --live_count; }

	// For tests/diagnostics only (leak/double-free detection) - not read or
	// written by any production code path, and not thread-safe (matching
	// this class's single-thread-per-Link design assumption throughout).
	static long	live_count;
};
template<class K, class V> long RbNode<K, V>::live_count = 0;

template<class K, class V>
class	RbTree
{
public:
	using	Node = RbNode<K, V>;
	// Deliberately TaggedRef<Node>, not "typename Node::Link": a dependent
	// nested-type lookup requires the named class to be a *complete* type
	// even for a plain alias, whereas naming TaggedRef<Node> directly does
	// not require Node (RbNode<K,V>, which embeds V) to be complete - see
	// the note in taggedref.h about TagMask()/PtrMask(). That distinction
	// matters here: CowMapBody names this Link type before V (e.g. Variant,
	// in variant.h) is necessarily complete.
	using	Link = TaggedRef<Node>;

	// Parent-pointer-free in-order iterator: a shared node can have multiple
	// "parents" across different tree versions, so a parent pointer isn't
	// even well-defined. Instead, an explicit path stack of ancestors
	// reached via a left turn, plus the current node on top - the standard
	// technique for this kind of traversal.
	class	Iter
	{
		// mutable: Array<>::last() isn't const-qualified, but current()/
		// operator==/!= need to be callable on a const Iter (or a const&)
		mutable Array<Link>	stack;

		void		pushLeft(Link n)
				{
					while (n)
					{
						stack.push(n);
						n = n.get()->left;
					}
				}

	public:
		Iter() {}					// end(): empty stack
		explicit Iter(Link root) { pushLeft(root); }

		// Builds the same kind of stack via a key-guided walk: push, then
		// descend left when key < node->key; descend right *without*
		// pushing when key > node->key (that node's role as a pending
		// backtrack point only matters while its left subtree is
		// unvisited); push and stop when found. Satisfies exactly the
		// same invariant as pushLeft, so operator++ works uniformly
		// afterward regardless of how the Iter was produced.
		static Iter	locate(Link root, const K& key)
				{
					Iter	it;
					Link	cur = root;
					while (cur)
					{
						if (key < cur->key)
						{
							it.stack.push(cur);
							cur = cur->left;
						}
						else if (cur->key < key)
						{
							cur = cur->right;
						}
						else
						{
							it.stack.push(cur);
							return it;	// found - the stack built up so far is exactly right
						}
					}
					return Iter();		// not found - discard the partial ancestor stack; must equal end()
				}

		Node*		current() const
				{ return stack.isEmpty() ? (Node*)0 : stack.last().get(); }

		bool		operator!=(const Iter& o) const { return current() != o.current(); }
		bool		operator==(const Iter& o) const { return current() == o.current(); }

		Iter&		operator++()			// pre-increment
				{
					Link	top = stack.pull();	// pop
					pushLeft(top.get()->right);
					return *this;
				}
		Iter		operator++(int)		// post-increment (CowMap::Unshare() uses it++)
				{
					Iter	tmp = *this;
					++(*this);
					return tmp;
				}

		// Presented as a std::map-like iterator: (*it).first/.second and
		// it->first/it->second, matching the only two ways an iterator is
		// actually dereferenced anywhere in this codebase today.
		struct KV { const K& first; const V& second; };
		KV		operator*() const
				{ Node* n = current(); return KV{n->key, n->value}; }
		struct ArrowProxy { KV kv; const KV* operator->() const { return &kv; } };
		ArrowProxy	operator->() const { return ArrowProxy{**this}; }
	};

	static Iter	begin(Link root) { return Iter(root); }
	static Iter	end() { return Iter(); }
	static Iter	find(Link root, const K& key) { return Iter::locate(root, key); }

	// Top-level insert/erase. root is kept at tag RB_BLACK always (it has
	// no incoming edge, so "colour" is a fiction for it, but reusing Link
	// here rather than introducing a second type keeps the code uniform).
	static void	insert(Link& root, const K& key, const V& val)
			{
				insertNode(root, key, val);
				root.SetTag(RB_BLACK);
			}
	static void	erase(Link& root, const K& key)
			{
				if (find(root, key) == end())		// no-op if the key isn't present
					return;
				if (!isRed(root->left) && !isRed(root->right))
					root.SetTag(RB_RED);
				doDelete(root, key);
				if (root)
					root.SetTag(RB_BLACK);
			}

private:
	static bool	isRed(const Link& h) { return h && h.Tag() == RB_RED; }

	// Ensures h's node is exclusively ours before we touch its fields (or
	// hand one of its fields out by mutable reference for further
	// recursion). See the file header comment for why refcount==1 makes
	// this safe.
	static void	ensureUnique(Link& h)
			{
				if (h.GetRefCount() > 1)
					h = Link(new Node(h->key, h->value, h->left, h->right), h.Tag());
			}

	// rotateLeft(h): x=h.right; h.right=x.left; x.left=h; x.color=h.color; h.color=RED; return x
	static void	rotateLeft(Link& h)
			{
				// Precondition: h->right is red.
				ensureUnique(h);
				Link	x = h->right;
				h->right = Link();		// release our extra reference before the uniqueness check -
								// otherwise x's refcount would read >=2 (the field, plus this
								// local copy) even when the subtree isn't genuinely shared,
								// forcing an unconditional (and pointless) clone every time.
				ensureUnique(x);
				uintptr_t	hColor = h.Tag();
				h->right = x->left;
				h.SetTag(RB_RED);		// set BEFORE storing h into x->left: a Link's tag is
								// copied by value at the moment it's stored, unlike
								// Sedgewick's original where colour is a mutable field on
								// a shared object and reassigning it afterward would still
								// be visible through x.left.
				x->left = h;
				x.SetTag(hColor);
				h = x;
			}
	// rotateRight(h): mirror image
	static void	rotateRight(Link& h)
			{
				// Precondition: h->left is red.
				ensureUnique(h);
				Link	x = h->left;
				h->left = Link();
				ensureUnique(x);
				uintptr_t	hColor = h.Tag();
				h->left = x->right;
				h.SetTag(RB_RED);
				x->right = h;
				x.SetTag(hColor);
				h = x;
			}
	// flipColors(h): h.color=!h.color; h.left.color=!h.left.color; h.right.color=!h.right.color
	static void	flipColors(Link& h)
			{
				ensureUnique(h);
				h->left.SetTag(!isRed(h->left));
				h->right.SetTag(!isRed(h->right));
				h.SetTag(!isRed(h));
			}

	// moveRedLeft(h): flipColors(h); if isRed(h.right.left): h.right=rotateRight(h.right); h=rotateLeft(h); flipColors(h)
	static void	moveRedLeft(Link& h)
			{
				flipColors(h);
				if (isRed(h->right->left))
				{
					rotateRight(h->right);
					rotateLeft(h);
					flipColors(h);
				}
			}
	// moveRedRight(h): flipColors(h); if isRed(h.left.left): h=rotateRight(h); flipColors(h)
	static void	moveRedRight(Link& h)
			{
				flipColors(h);
				if (isRed(h->left->left))
				{
					rotateRight(h);
					flipColors(h);
				}
			}
	// balance(h): if isRed(h.right) and not isRed(h.left): h=rotateLeft(h);
	//             if isRed(h.left) and isRed(h.left.left): h=rotateRight(h);
	//             if isRed(h.left) and isRed(h.right): flipColors(h)
	static void	balance(Link& h)
			{
				if (isRed(h->right) && !isRed(h->left)) rotateLeft(h);
				if (isRed(h->left) && isRed(h->left->left)) rotateRight(h);
				if (isRed(h->left) && isRed(h->right)) flipColors(h);
			}

	// insert(h,key,val): if h==null return new Node(key,val,RED);
	//   if key<h.key h.left=insert(h.left,...); elif key>h.key h.right=insert(h.right,...); else h.val=val;
	//   [same 3-line rebalance as balance()]; return h
	static void	insertNode(Link& h, const K& key, const V& val)
			{
				if (!h)
				{
					h = Link(new Node(key, val, Link(), Link()), RB_RED);
					return;
				}
				ensureUnique(h);
				if (key < h->key)
					insertNode(h->left, key, val);
				else if (h->key < key)
					insertNode(h->right, key, val);
				else
					h->value = val;	// key is intentionally NOT replaced - matches Sedgewick
							// and standard map-style insert/operator[] semantics
				if (isRed(h->right) && !isRed(h->left)) rotateLeft(h);
				if (isRed(h->left) && isRed(h->left->left)) rotateRight(h);
				if (isRed(h->left) && isRed(h->right)) flipColors(h);
			}

	// deleteMin(h): if h.left==null return null;
	//   if not isRed(h.left) and not isRed(h.left.left): h=moveRedLeft(h);
	//   h.left=deleteMin(h.left); return balance(h)
	static void	deleteMin(Link& h)
			{
				if (!h->left)
				{
					h = Link();
					return;
				}
				ensureUnique(h);
				if (!isRed(h->left) && !isRed(h->left->left))
					moveRedLeft(h);
				deleteMin(h->left);
				balance(h);
			}

	static Node*	leftmost(Node* n)		// pure read-only walk, no mutation, no ensureUnique needed
			{
				while (n->left)
					n = n->left.get();
				return n;
			}

	// delete(h,key): if key<h.key: [maybe moveRedLeft]; h.left=delete(h.left,key);
	//   else: if isRed(h.left): h=rotateRight(h);
	//         if key==h.key and h.right==null: return null;
	//         if not isRed(h.right) and not isRed(h.right.left): h=moveRedRight(h);
	//         if key==h.key: h.val=min(h.right).val; h.key=min(h.right).key; h.right=deleteMin(h.right);
	//         else: h.right=delete(h.right,key);
	//   return balance(h)
	//
	// Precondition (guaranteed by erase()'s find() guard): key is present
	// somewhere in this subtree.
	static void	doDelete(Link& h, const K& key)
			{
				ensureUnique(h);
				if (key < h->key)
				{
					if (!isRed(h->left) && !isRed(h->left->left))
						moveRedLeft(h);
					doDelete(h->left, key);
				}
				else
				{
					if (isRed(h->left))
						rotateRight(h);
					if (!(h->key < key) && !(key < h->key) && !h->right)
					{
						h = Link();
						return;
					}
					if (!isRed(h->right) && !isRed(h->right->left))
						moveRedRight(h);
					if (!(h->key < key) && !(key < h->key))
					{
						Node*	succ = leftmost(h->right.get());
						h->key = succ->key;		// copy successor's data into h BEFORE
						h->value = succ->value;	// deleteMin below can touch/free succ
						deleteMin(h->right);
					}
					else
					{
						doDelete(h->right, key);
					}
				}
				balance(h);
			}
};
#endif // REDBLACK_H
