/*
 * Standalone test suite for TaggedRef<T> (see include/taggedref.h).
 *
 * TaggedRef packs a small tag into the spare low-order bits of a
 * reference-counted pointer. Because it manages its own storage rather
 * than reusing Ref<T>'s (see the comment at the top of taggedref.h for
 * why), the main things worth verifying in isolation, before any tree
 * code is built on top of it, are: the refcounting is exactly right
 * (no leaks, no premature frees, no double-frees) across construction,
 * copying, assignment and self-assignment; the tag bits round-trip
 * correctly and never corrupt the pointer; and the tag width correctly
 * follows the pointee's alignment.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<taggedref.h>

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

/*
 * A RefCounted node whose live instance count we track, so leaks,
 * premature frees and double-frees all show up as a nonzero count
 * at points where we expect zero.
 */
struct TestNode : RefCounted
{
	int		value;
	static int	live_count;
	TestNode(int v) : value(v) { live_count++; }
	TestNode(const TestNode&) = delete;	// Matches this codebase's RefCounted "body" convention (e.g. ArrayBody)
	~TestNode() { live_count--; }
};
int	TestNode::live_count = 0;

// A second type with a larger explicit alignment, to confirm the tag
// width tracks alignof(T) rather than being hard-coded.
struct alignas(16) BigAlignNode : RefCounted
{
	int		value;
	static int	live_count;
	BigAlignNode(int v) : value(v) { live_count++; }
	BigAlignNode(const BigAlignNode&) = delete;
	~BigAlignNode() { live_count--; }
};
int	BigAlignNode::live_count = 0;

void		basic_lifecycle_tests();
void		tag_bit_tests();
void		copy_and_assignment_tests();
void		self_assignment_tests();
void		alignment_scaling_tests();
void		stress_tests();

int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	basic_lifecycle_tests();
	tag_bit_tests();
	copy_and_assignment_tests();
	self_assignment_tests();
	alignment_scaling_tests();
	stress_tests();

	printf("Completed %d tests with %d failures\n", test_count, failure_count);
	return failure_count == 0 ? 0 : 1;
}

void
basic_lifecycle_tests()
{
	test_group("Basic lifecycle: default (empty) TaggedRef");
	{
		TaggedRef<TestNode>	empty;
		expect("default-constructed is falsy", !(bool)empty);
		expect("default-constructed get() is null", empty.get() == 0);
		expect_eq_int("default-constructed Tag() is 0", (long)empty.Tag(), 0);
		expect_eq_int("default-constructed GetRefCount() is 0", empty.GetRefCount(), 0);
		// Destroying an empty TaggedRef must not crash or touch live_count.
	}
	expect_eq_int("live_count is 0 after empty TaggedRef destroyed", TestNode::live_count, 0);

	test_group("Basic lifecycle: construct from a raw pointer, then destroy");
	{
		TaggedRef<TestNode>	r(new TestNode(42));
		expect("non-empty is truthy", (bool)r);
		expect_eq_int("value round-trips", r->value, 42);
		expect_eq_int("live_count is 1 while held", TestNode::live_count, 1);
		expect_eq_int("GetRefCount() is 1 (sole owner)", r.GetRefCount(), 1);
		expect_eq_int("default tag is 0", (long)r.Tag(), 0);
	}
	expect_eq_int("live_count is 0 after sole owner destroyed", TestNode::live_count, 0);
}

void
tag_bit_tests()
{
	test_group("Tag bits: construct with an explicit tag, pointer stays correct");
	// TestNode derives from RefCounted, whose atomic<int> member gives at least
	// 4-byte alignment, so at least 2 tag bits (mask 0x3) are always available.
	expect("at least 2 tag bits available on TestNode", TaggedRef<TestNode>::TagMask() >= 3);

	TestNode*	raw = new TestNode(7);
	{
		TaggedRef<TestNode>	r(raw, 1);
		expect_eq_int("Tag() is 1", (long)r.Tag(), 1);
		expect("get() returns the exact original pointer, tag masked off", r.get() == raw);
		expect_eq_int("value still correct through the tagged pointer", r->value, 7);

		test_group("Tag bits: SetTag() changes only the tag");
		r.SetTag(0);
		expect_eq_int("Tag() is now 0", (long)r.Tag(), 0);
		expect("get() unchanged after SetTag", r.get() == raw);
		expect_eq_int("GetRefCount() unaffected by SetTag", r.GetRefCount(), 1);
		r.SetTag(TaggedRef<TestNode>::TagMask());
		expect_eq_int("Tag() round-trips the maximum mask value", (long)r.Tag(), (long)TaggedRef<TestNode>::TagMask());
		expect("get() still unchanged", r.get() == raw);

		test_group("Tag bits: WithTag() shares the pointee at a different tag");
		TaggedRef<TestNode>	r2 = r.WithTag(0);
		expect("WithTag result points at the same node", r2.get() == raw);
		expect_eq_int("WithTag result has the requested tag", (long)r2.Tag(), 0);
		expect_eq_int("original's tag is untouched by WithTag", (long)r.Tag(), (long)TaggedRef<TestNode>::TagMask());
		expect_eq_int("GetRefCount() is now 2 (r and r2 both own it)", r.GetRefCount(), 2);
		expect_eq_int("live_count is still just 1 node", TestNode::live_count, 1);
	}
	expect_eq_int("live_count is 0 after both tagged refs destroyed", TestNode::live_count, 0);

	test_group("Tag bits: operator== considers both pointer and tag");
	{
		TaggedRef<TestNode>	a(new TestNode(1), 1);
		TaggedRef<TestNode>	b = a.WithTag(1);
		TaggedRef<TestNode>	c = a.WithTag(0);
		expect("same pointee, same tag -> equal", a == b);
		expect("same pointee, different tag -> not equal", a != c);
	}
	expect_eq_int("live_count is 0 after equality-test scope ends", TestNode::live_count, 0);
}

void
copy_and_assignment_tests()
{
	test_group("Copy constructor shares the pointee and refcount");
	{
		TaggedRef<TestNode>	a(new TestNode(10), 1);
		TaggedRef<TestNode>	b(a);
		expect("copy shares the same pointee", a.get() == b.get());
		expect_eq_int("copy has the same tag", (long)b.Tag(), 1);
		expect_eq_int("GetRefCount() is 2", a.GetRefCount(), 2);
		expect_eq_int("live_count is still 1", TestNode::live_count, 1);
	}
	expect_eq_int("live_count is 0 after both copies destroyed", TestNode::live_count, 0);

	test_group("operator=(const TaggedRef&) releases the old pointee and adopts the new one");
	{
		TaggedRef<TestNode>	a(new TestNode(1));
		TaggedRef<TestNode>	b(new TestNode(2));
		expect_eq_int("live_count is 2 before assignment", TestNode::live_count, 2);
		a = b;
		expect_eq_int("a now shares b's pointee", a->value, 2);
		expect_eq_int("live_count drops to 1 (a's old node freed)", TestNode::live_count, 1);
		expect_eq_int("GetRefCount() is 2 (a and b share it)", a.GetRefCount(), 2);
	}
	expect_eq_int("live_count is 0 after scope ends", TestNode::live_count, 0);

	test_group("operator=(T*) keeps the current tag, replaces the pointee");
	{
		TaggedRef<TestNode>	a(new TestNode(1), 1);
		TestNode*		raw2 = new TestNode(2);
		a = raw2;
		expect_eq_int("value updated", a->value, 2);
		expect_eq_int("tag preserved across pointer-assignment", (long)a.Tag(), 1);
		expect_eq_int("live_count is 1 (old node freed, new node held)", TestNode::live_count, 1);
		expect_eq_int("GetRefCount() is 1", a.GetRefCount(), 1);
	}
	expect_eq_int("live_count is 0 after scope ends", TestNode::live_count, 0);

	test_group("Assigning an empty TaggedRef releases the old pointee");
	{
		TaggedRef<TestNode>	a(new TestNode(1));
		TaggedRef<TestNode>	empty;
		a = empty;
		expect("a is now empty", !(bool)a);
		expect_eq_int("live_count is 0 (old node released)", TestNode::live_count, 0);
	}
}

void
self_assignment_tests()
{
	test_group("Self-assignment via operator=(const TaggedRef&) is safe");
	{
		TaggedRef<TestNode>	a(new TestNode(5), 1);
		a = a;
		expect_eq_int("value unchanged", a->value, 5);
		expect_eq_int("tag unchanged", (long)a.Tag(), 1);
		expect_eq_int("GetRefCount() still 1 (not incremented)", a.GetRefCount(), 1);
		expect_eq_int("live_count still 1 (not freed)", TestNode::live_count, 1);
	}
	expect_eq_int("live_count is 0 after scope ends", TestNode::live_count, 0);

	test_group("Self-assignment via operator=(T*) with the same pointer is safe");
	{
		TestNode*		raw = new TestNode(6);
		TaggedRef<TestNode>	a(raw);
		a = raw;
		expect_eq_int("value unchanged", a->value, 6);
		expect_eq_int("GetRefCount() still 1", a.GetRefCount(), 1);
		expect_eq_int("live_count still 1", TestNode::live_count, 1);
	}
	expect_eq_int("live_count is 0 after scope ends", TestNode::live_count, 0);
}

void
alignment_scaling_tests()
{
	test_group("Tag width scales with alignof(T)");
	expect_eq_int("TestNode TagMask matches alignof-1", (long)TaggedRef<TestNode>::TagMask(), (long)(alignof(TestNode) - 1));
	expect_eq_int("BigAlignNode TagMask is 15 (align 16)", (long)TaggedRef<BigAlignNode>::TagMask(), 15);

	BigAlignNode*	raw = new BigAlignNode(3);
	{
		TaggedRef<BigAlignNode>	r(raw, 13);	// exercise a tag value that needs all 4 bits
		expect_eq_int("wide tag round-trips", (long)r.Tag(), 13);
		expect("pointer round-trips exactly", r.get() == raw);
		expect_eq_int("value correct through a widely-tagged pointer", r->value, 3);
	}
	expect_eq_int("BigAlignNode live_count is 0 after scope ends", BigAlignNode::live_count, 0);
}

void
stress_tests()
{
	test_group("Stress: repeated create/copy/reassign/destroy cycles leave no leaks");
	{
		TaggedRef<TestNode>	slots[8];
		for (int round = 0; round < 200; round++)
		{
			int	slot = round % 8;
			int	tagBits = (int)TaggedRef<TestNode>::TagMask();
			slots[slot] = TaggedRef<TestNode>(new TestNode(round), round & tagBits);
			if (slot > 0)
				slots[slot - 1] = slots[slot];		// share a node between two slots
		}
	}
	expect_eq_int("live_count is 0 after the stress loop's slots are all destroyed", TestNode::live_count, 0);
}
