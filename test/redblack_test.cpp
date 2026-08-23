/*
 * Tests for the persistent (path-copying) Left-Leaning Red-Black tree,
 * include/redblack.h - the underlying map for CowMap.
 *
 * Uses the same white-box-access convention as test/reassembly_test.cpp
 * (#define private public before the include) to reach RbTree's internal
 * primitives (ensureUnique, insertNode, ...) directly, since those need
 * isolated testing before they're trusted inside the full algorithm.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#define		private		public
#include	<redblack.h>
#undef		private

#include	<strval.h>
#include	<array.h>

#include	<cstdio>
#include	<cstring>
#include	<cstdlib>
#include	<climits>
#include	<map>

bool		show_passes = false;
int		test_count;
int		failure_count;
const char*	new_group;

void
test_group(const char* group)
{
	new_group = group;
}

static void
report(const char* when, bool passed, const char* detail)
{
	test_count++;
	if (!passed)
	{
		if (new_group)
		{
			printf("%s:\n", new_group);
			new_group = 0;
		}
		printf("%d:\t%s: FAIL%s%s\n", test_count, when, detail ? " " : "", detail ? detail : "");
		failure_count++;
	}
	else if (show_passes)
	{
		if (new_group)
		{
			printf("%s:\n", new_group);
			new_group = 0;
		}
		printf("%d:\t%s: PASS\n", test_count, when);
	}
}

void
expect(const char* when, bool cond)
{
	report(when, cond, 0);
}

void
expect_eq_int(const char* when, long got, long want)
{
	char	detail[64];
	bool	ok = got == want;
	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted %ld got %ld)", want, got);
	report(when, ok, ok ? 0 : detail);
}

using	Tree = RbTree<int, int>;
using	Link = Tree::Link;
using	Node = Tree::Node;

/*
 * Returns the black-height of this subtree if every LLRB invariant holds
 * (BST ordering within (minKey, maxKey), no right-leaning red link, no two
 * reds in a row, equal black-height on every path, root black), or -1 (with
 * a diagnostic printed) on the first violation found.
 */
static int
checkInvariants(Link h, long minKey, long maxKey, bool isRoot = false)
{
	if (!h)
		return 1;		// NIL counts as black, height 1
	if (isRoot && h.Tag() == RB_RED)
	{
		printf("  INVARIANT VIOLATION: root is red\n");
		return -1;
	}
	if (h->right && h->right.Tag() == RB_RED)
	{
		printf("  INVARIANT VIOLATION: right-leaning red link at key %d\n", h->key);
		return -1;
	}
	if (h.Tag() == RB_RED && h->left && h->left.Tag() == RB_RED)
	{
		printf("  INVARIANT VIOLATION: two red links in a row at key %d\n", h->key);
		return -1;
	}
	if (!(h->key > minKey && h->key < maxKey))
	{
		printf("  INVARIANT VIOLATION: BST order violated at key %d (bounds %ld..%ld)\n", h->key, minKey, maxKey);
		return -1;
	}
	int	leftHeight = checkInvariants(h->left, minKey, h->key);
	if (leftHeight < 0)
		return -1;
	int	rightHeight = checkInvariants(h->right, h->key, maxKey);
	if (rightHeight < 0)
		return -1;
	if (leftHeight != rightHeight)
	{
		printf("  INVARIANT VIOLATION: unequal black height at key %d (%d vs %d)\n", h->key, leftHeight, rightHeight);
		return -1;
	}
	return leftHeight + (h.Tag() == RB_BLACK ? 1 : 0);
}

void		basic_operations_tests();
void		ensure_unique_isolation_tests();
void		differential_tests();
void		persistence_tests();
void		leak_tests();
void		stress_tests();
void		strval_key_tests();

int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	basic_operations_tests();
	ensure_unique_isolation_tests();
	differential_tests();
	persistence_tests();
	leak_tests();
	stress_tests();
	strval_key_tests();

	printf("Completed %d tests with %d failures\n", test_count, failure_count);
	return failure_count == 0 ? 0 : 1;
}

/*
 * --------------------------------------------------------------------
 * Small, hand-checked insert/find/erase cases
 * --------------------------------------------------------------------
 */
void
basic_operations_tests()
{
	test_group("Basic: empty tree");
	Link	root;
	expect("find on empty tree returns end()", Tree::find(root, 5) == Tree::end());
	expect("begin() == end() on empty tree", Tree::begin(root) == Tree::end());

	test_group("Basic: insert and find");
	Tree::insert(root, 5, 50);
	expect("found after insert", Tree::find(root, 5) != Tree::end());
	expect_eq_int("found value correct", (*Tree::find(root, 5)).second, 50);
	Tree::insert(root, 3, 30);
	Tree::insert(root, 8, 80);
	Tree::insert(root, 1, 10);
	expect("RB invariants hold", checkInvariants(root, LONG_MIN, LONG_MAX, true) >= 0);

	test_group("Basic: in-order iteration");
	{
		int	expectedKeys[] = {1, 3, 5, 8};
		int	expectedVals[] = {10, 30, 50, 80};
		int	i = 0;
		bool	ok = true;
		for (Tree::Iter it = Tree::begin(root); it != Tree::end(); ++it, i++)
		{
			if (i >= 4 || (*it).first != expectedKeys[i] || (*it).second != expectedVals[i])
			{
				ok = false;
				break;
			}
		}
		expect("sequence matches expected", ok && i == 4);
	}

	test_group("Basic: insert on existing key updates value only");
	Tree::insert(root, 5, 999);
	expect_eq_int("value updated", (*Tree::find(root, 5)).second, 999);
	{
		int	i = 0;
		for (Tree::Iter it = Tree::begin(root); it != Tree::end(); ++it)
			i++;
		expect_eq_int("count unchanged by overwrite", i, 4);
	}

	test_group("Basic: erase of absent key is a no-op");
	Tree::erase(root, 42);
	expect("tree unaffected", Tree::find(root, 5) != Tree::end());
	expect("RB invariants still hold", checkInvariants(root, LONG_MIN, LONG_MAX, true) >= 0);

	test_group("Basic: erase all keys one at a time");
	int	keys[] = {1, 3, 5, 8};
	for (int k : keys)
	{
		Tree::erase(root, k);
		char	msg1[64], msg2[64];
		snprintf(msg1, sizeof(msg1), "invariants hold after erasing %d", k);
		snprintf(msg2, sizeof(msg2), "key %d is gone after erasing it", k);
		expect(msg1, checkInvariants(root, LONG_MIN, LONG_MAX, true) >= 0);
		expect(msg2, Tree::find(root, k) == Tree::end());
	}
	expect("tree empty after erasing everything", root.get() == 0);
}

/*
 * --------------------------------------------------------------------
 * ensureUnique() in isolation, before it's trusted inside the algorithm
 * --------------------------------------------------------------------
 */
void
ensure_unique_isolation_tests()
{
	test_group("ensureUnique(): no-op when uniquely owned");
	Link	a;
	Tree::insertNode(a, 1, 100);
	Node*	before = a.get();
	expect_eq_int("fresh node has refcount 1", a.GetRefCount(), 1);
	Tree::ensureUnique(a);
	expect("no-op: same address", a.get() == before);
	expect_eq_int("still refcount 1", a.GetRefCount(), 1);

	test_group("ensureUnique(): clones when shared, preserving content");
	Link	b = a;
	expect_eq_int("shared refcount is 2", a.GetRefCount(), 2);
	Tree::ensureUnique(a);
	expect("clones: different address", a.get() != before);
	expect_eq_int("key preserved", a->key, 1);
	expect_eq_int("value preserved", a->value, 100);
	expect_eq_int("a is now uniquely owned", a.GetRefCount(), 1);
	expect_eq_int("b (the untouched original) is uniquely owned by itself", b.GetRefCount(), 1);
	expect("b still points at the original node", b.get() == before);
}

/*
 * --------------------------------------------------------------------
 * Differential testing against std::map<int,int>
 * --------------------------------------------------------------------
 */
static void
runDifferential(int iterations, int keyRange, unsigned seed, bool checkSequenceEveryTime)
{
	srand(seed);
	Link			root;
	std::map<int, int>	reference;
	for (int iter = 0; iter < iterations; iter++)
	{
		int	key = rand() % keyRange;
		int	val = rand() % 1000;
		if (rand() % 4 == 0)
		{
			Tree::erase(root, key);
			reference.erase(key);
		}
		else
		{
			Tree::insert(root, key, val);
			reference[key] = val;
		}

		char	msg[96];
		snprintf(msg, sizeof(msg), "invariants hold after iteration %d", iter);
		int	bh = checkInvariants(root, LONG_MIN, LONG_MAX, true);
		expect(msg, bh >= 0);
		if (bh < 0)
			break;		// avoid a flood of cascading failures from one root cause

		if (checkSequenceEveryTime || iter == iterations - 1)
		{
			Array<int>	gotKeys;
			for (Tree::Iter it = Tree::begin(root); it != Tree::end(); ++it)
				gotKeys.push((*it).first);
			bool	matches = (int)gotKeys.length() == (int)reference.size();
			if (matches)
			{
				int	i = 0;
				for (auto& kv : reference)
				{
					if (gotKeys[i] != kv.first)
					{
						matches = false;
						break;
					}
					i++;
				}
			}
			snprintf(msg, sizeof(msg), "in-order sequence matches std::map after iteration %d", iter);
			expect(msg, matches);
		}

		bool	allFindsCorrect = true;
		for (int k = 0; k < keyRange; k++)
		{
			bool	refHas = reference.count(k) != 0;
			bool	treeHas = Tree::find(root, k) != Tree::end();
			if (refHas != treeHas
			 || (refHas && (*Tree::find(root, k)).second != reference[k]))
			{
				allFindsCorrect = false;
				break;
			}
		}
		snprintf(msg, sizeof(msg), "find() results match std::map after iteration %d", iter);
		expect(msg, allFindsCorrect);
	}
}

void
differential_tests()
{
	test_group("Differential: random insert/erase vs std::map<int,int>");
	runDifferential(500, 50, 12345, true);
}

/*
 * --------------------------------------------------------------------
 * Persistence: a snapshot must be completely unaffected by later
 * mutations of the version it was copied from
 * --------------------------------------------------------------------
 */
void
persistence_tests()
{
	test_group("Persistence: snapshot before mutation stays untouched");
	Link	v1;
	for (int k = 0; k < 20; k++)
		Tree::insert(v1, k, k * 10);
	Link	snapshot = v1;			// shares the same root; refcount now 2

	Tree::insert(v1, 100, 999);		// new key
	Tree::erase(v1, 5);			// remove an existing key
	Tree::insert(v1, 3, 12345);		// overwrite an existing key's value

	bool	snapshotIntact = true;
	for (int k = 0; k < 20; k++)
	{
		Tree::Iter it = Tree::find(snapshot, k);
		if (it == Tree::end() || (*it).second != k * 10)
		{
			snapshotIntact = false;
			break;
		}
	}
	if (Tree::find(snapshot, 100) != Tree::end())
		snapshotIntact = false;		// must not see the new key
	expect("snapshot unaffected by mutations on the live version", snapshotIntact);

	expect("live version has the new key", Tree::find(v1, 100) != Tree::end());
	expect("live version no longer has the erased key", Tree::find(v1, 5) == Tree::end());
	Tree::Iter	it3 = Tree::find(v1, 3);
	expect("live version has the overwritten value", it3 != Tree::end() && (*it3).second == 12345);

	test_group("Persistence: interleaved snapshots");
	Link	snapshot2 = v1;
	Tree::insert(v1, 200, 1);
	Tree::erase(v1, 100);
	expect("snapshot2 doesn't have a key added after it was taken", Tree::find(snapshot2, 200) == Tree::end());
	expect("snapshot2 still has a key later erased from the live version", Tree::find(snapshot2, 100) != Tree::end());
	expect("live version reflects the further mutation",
		Tree::find(v1, 200) != Tree::end() && Tree::find(v1, 100) == Tree::end());

	test_group("Persistence: all versions still satisfy RB invariants");
	expect("live version", checkInvariants(v1, LONG_MIN, LONG_MAX, true) >= 0);
	expect("snapshot", checkInvariants(snapshot, LONG_MIN, LONG_MAX, true) >= 0);
	expect("snapshot2", checkInvariants(snapshot2, LONG_MIN, LONG_MAX, true) >= 0);
}

/*
 * --------------------------------------------------------------------
 * Leak / double-free checking via the node's live-instance counter
 * --------------------------------------------------------------------
 */
void
leak_tests()
{
	test_group("Leak/double-free: randomized snapshot/mutate/drop cycles");
	long	before = Node::live_count;
	{
		srand(999);
		Array<Link>	snapshots;
		Link		root;
		for (int iter = 0; iter < 300; iter++)
		{
			int	key = rand() % 30;
			int	val = rand() % 1000;
			if (rand() % 4 == 0)
				Tree::erase(root, key);
			else
				Tree::insert(root, key, val);
			if (rand() % 5 == 0)
				snapshots.push(root);
			if (snapshots.length() > 0 && rand() % 7 == 0)
				snapshots.remove(0, 1);
		}
	}
	expect_eq_int("live_count returns to baseline once everything is out of scope", Node::live_count, before);
}

/*
 * --------------------------------------------------------------------
 * A larger stress run, wider key range, to shake out rare-shape bugs
 * --------------------------------------------------------------------
 */
void
stress_tests()
{
	test_group("Stress: larger randomized run");
	runDifferential(3000, 200, 424242, false);
}

/*
 * --------------------------------------------------------------------
 * Sanity check with the actual intended key type, StrVal. The algorithm
 * itself is exercised thoroughly above with int keys (it's key-type
 * agnostic - it only relies on operator<), so this doesn't repeat the
 * full invariant/differential battery, just confirms real usage works.
 * --------------------------------------------------------------------
 */
void
strval_key_tests()
{
	test_group("Works with the real intended key type, StrVal");
	using	STree = RbTree<StrVal, int>;
	STree::Link	root;
	STree::insert(root, StrVal("banana"), 2);
	STree::insert(root, StrVal("apple"), 1);
	STree::insert(root, StrVal("cherry"), 3);

	expect_eq_int("apple value", (*STree::find(root, StrVal("apple"))).second, 1);
	expect_eq_int("banana value", (*STree::find(root, StrVal("banana"))).second, 2);
	expect_eq_int("cherry value", (*STree::find(root, StrVal("cherry"))).second, 3);
	expect("missing key not found", STree::find(root, StrVal("date")) == STree::end());

	const char*	expected[] = {"apple", "banana", "cherry"};
	int		i = 0;
	bool		ok = true;
	for (STree::Iter it = STree::begin(root); it != STree::end(); ++it, i++)
	{
		if (i >= 3 || !((*it).first == StrVal(expected[i])))
		{
			ok = false;
			break;
		}
	}
	expect("alphabetical in-order iteration", ok && i == 3);

	STree::erase(root, StrVal("banana"));
	expect("erase works with StrVal keys", STree::find(root, StrVal("banana")) == STree::end());
	expect("other keys survive the erase",
		STree::find(root, StrVal("apple")) != STree::end() && STree::find(root, StrVal("cherry")) != STree::end());
}
