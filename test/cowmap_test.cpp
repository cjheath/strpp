/*
 * Tests for CowMap (cowmap.h), now backed by the persistent red-black tree
 * in redblack.h rather than std::map. There was previously no dedicated
 * test for CowMap - it was only exercised indirectly via variant.h and
 * variant_test.cpp - so this covers the public API end-to-end, plus the
 * copy-on-write behaviour at the CowMap level that this whole exercise was
 * about (see the plan: Unshare() used to rebuild the whole map element by
 * element on every mutation of a shared map; it's now O(1)).
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<cowmap.h>
#include	<strval.h>

#include	<cstdio>
#include	<cstring>

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

void
expect_eq_str(const char* when, StrVal got, const char* want)
{
	StrVal	wantv(want);
	bool	ok = got.length() == wantv.length() && got == wantv;
	char	detail[512];
	if (!ok)
		snprintf(detail, sizeof(detail), "(wanted \"%s\" got \"%s\")", want, got.asUTF8());
	report(when, ok, ok ? 0 : detail);
}

using	SIMap = CowMap<int, StrVal>;

void		basic_tests();
void		put_and_overwrite_tests();
void		remove_tests();
void		iteration_tests();
void		cow_sharing_tests();
void		bulk_construct_tests();

int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	basic_tests();
	put_and_overwrite_tests();
	remove_tests();
	iteration_tests();
	cow_sharing_tests();
	bulk_construct_tests();

	printf("Completed %d tests with %d failures\n", test_count, failure_count);
	return failure_count == 0 ? 0 : 1;
}

void
basic_tests()
{
	test_group("Basic: empty map");
	SIMap	m;
	expect_eq_int("empty map size", (long)m.size(), 0);
	expect("empty map doesn't contain a key", !m.contains(StrVal("x")));
	expect("find on empty map returns end()", m.find(StrVal("x")) == m.end());
	expect_eq_int("operator[] on missing key returns default value", m[StrVal("x")], 0);

	test_group("Basic: insert and find");
	m.insert(StrVal("one"), 1);
	m.insert(StrVal("two"), 2);
	m.insert(StrVal("three"), 3);
	expect_eq_int("size after 3 inserts", (long)m.size(), 3);
	expect("contains an inserted key", m.contains(StrVal("two")));
	expect("doesn't contain an absent key", !m.contains(StrVal("four")));
	expect_eq_int("operator[] finds the right value", m[StrVal("two")], 2);
	auto	it = m.find(StrVal("three"));
	expect("find() succeeds", it != m.end());
	expect_eq_int("found value via ->second", it->second, 3);
	expect_eq_str("found key via ->first", it->first, "three");

	test_group("Basic: clear");
	m.clear();
	expect_eq_int("size after clear", (long)m.size(), 0);
	expect("cleared map doesn't contain a previously-inserted key", !m.contains(StrVal("one")));
}

void
put_and_overwrite_tests()
{
	test_group("put(): insert new, and overwrite existing");
	SIMap	m;
	m.put(StrVal("a"), 1);
	expect_eq_int("put() inserted a new key", m[StrVal("a")], 1);
	expect_eq_int("size is 1", (long)m.size(), 1);
	m.put(StrVal("a"), 2);
	expect_eq_int("put() overwrote the existing key's value", m[StrVal("a")], 2);
	expect_eq_int("size unchanged by overwrite", (long)m.size(), 1);

	test_group("insert(): overwrite existing key via plain insert too");
	m.insert(StrVal("a"), 99);
	expect_eq_int("insert() also overwrites", m[StrVal("a")], 99);
	expect_eq_int("size still 1", (long)m.size(), 1);
}

void
remove_tests()
{
	test_group("remove(): removes a present key");
	SIMap	m;
	m.insert(StrVal("a"), 1);
	m.insert(StrVal("b"), 2);
	m.insert(StrVal("c"), 3);
	m.remove(StrVal("b"));
	expect_eq_int("size decremented", (long)m.size(), 2);
	expect("removed key is gone", !m.contains(StrVal("b")));
	expect("other keys survive", m.contains(StrVal("a")) && m.contains(StrVal("c")));

	test_group("remove(): no-op on an absent key");
	m.remove(StrVal("nonexistent"));
	expect_eq_int("size unchanged", (long)m.size(), 2);

	test_group("remove(): every key, one at a time");
	m.remove(StrVal("a"));
	m.remove(StrVal("c"));
	expect_eq_int("size is 0", (long)m.size(), 0);
	expect("begin() == end() once empty", m.begin() == m.end());
}

void
iteration_tests()
{
	test_group("Iteration: in-order by key (StrVal byte ordering)");
	SIMap	m;
	m.insert(StrVal("banana"), 2);
	m.insert(StrVal("apple"), 1);
	m.insert(StrVal("cherry"), 3);

	const char*	expectedKeys[] = {"apple", "banana", "cherry"};
	int		expectedVals[] = {1, 2, 3};
	int		i = 0;
	bool		ok = true;
	for (auto it = m.begin(); it != m.end(); it++)
	{
		if (i >= 3 || !(it->first == StrVal(expectedKeys[i])) || it->second != expectedVals[i])
		{
			ok = false;
			break;
		}
		i++;
	}
	expect("iteration order and content correct", ok && i == 3);

	test_group("Iteration: dereference via (*it).first/.second (as variant.h's as_json() uses it)");
	auto	it2 = m.begin();
	expect_eq_str("(*it2).first", (*it2).first, "apple");
	expect_eq_int("(*it2).second", (*it2).second, 1);
}

void
cow_sharing_tests()
{
	test_group("COW: copying a CowMap is cheap and shares content");
	SIMap	original;
	for (int i = 0; i < 20; i++)
	{
		char	key[8];
		snprintf(key, sizeof(key), "k%02d", i);
		original.insert(StrVal(key), i);
	}
	SIMap	copy(original);
	expect_eq_int("copy has the same size", (long)copy.size(), (long)original.size());
	expect_eq_int("copy has the same content", copy[StrVal("k05")], 5);

	test_group("COW: mutating the copy doesn't affect the original");
	copy.insert(StrVal("new_key"), 999);
	copy.remove(StrVal("k03"));
	copy.put(StrVal("k07"), 12345);
	expect_eq_int("original size unaffected", (long)original.size(), 20);
	expect("original doesn't see the copy's new key", !original.contains(StrVal("new_key")));
	expect("original doesn't see the copy's removal", original.contains(StrVal("k03")));
	expect_eq_int("original doesn't see the copy's overwrite", original[StrVal("k07")], 7);

	expect_eq_int("copy reflects its own mutations (size)", (long)copy.size(), 20);	// +1 new, -1 removed = net 0
	expect_eq_int("copy reflects the overwrite", copy[StrVal("k07")], 12345);
	expect("copy reflects the new key", copy.contains(StrVal("new_key")));
	expect("copy reflects the removal", !copy.contains(StrVal("k03")));

	test_group("COW: mutating the original doesn't affect an earlier copy");
	SIMap	original2;
	original2.insert(StrVal("x"), 1);
	SIMap	snapshot(original2);
	original2.insert(StrVal("y"), 2);
	original2.remove(StrVal("x"));
	expect("snapshot doesn't see the later insert", !snapshot.contains(StrVal("y")));
	expect("snapshot doesn't see the later removal", snapshot.contains(StrVal("x")));
	expect_eq_int("snapshot size frozen at the copy point", (long)snapshot.size(), 1);

	test_group("COW: assignment operator shares then diverges on mutation");
	SIMap	src;
	src.insert(StrVal("shared"), 42);
	SIMap	dst;
	dst = src;
	expect_eq_int("dst equals src after assignment", dst[StrVal("shared")], 42);
	dst.insert(StrVal("only_in_dst"), 1);
	expect("src unaffected by dst's mutation", !src.contains(StrVal("only_in_dst")));
}

void
bulk_construct_tests()
{
	test_group("Bulk construction from key/value arrays");
	StrVal	keys[] = {StrVal("a"), StrVal("b"), StrVal("c")};
	int	values[] = {1, 2, 3};
	SIMap	m(keys, values, 3);
	expect_eq_int("size", (long)m.size(), 3);
	expect_eq_int("a", m[StrVal("a")], 1);
	expect_eq_int("b", m[StrVal("b")], 2);
	expect_eq_int("c", m[StrVal("c")], 3);
}
