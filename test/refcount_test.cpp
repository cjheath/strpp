/*
 * RefCounted and Ref<T>: the count, the copy/assign/destroy behaviour that
 * keeps it right, and the two counts that must stop the program rather than
 * carry on - a release with no reference left, and an increment past the end.
 *
 * Neither of those two can happen without the class's own bookkeeping being
 * wrong, and neither is recoverable: a count that has gone negative makes a
 * shared body look private to every isShared() test, so the next write through
 * it corrupts data another value still holds. They are the library's
 * assertion, so they report, dump and stop in every build - which is what the
 * forked children here check, in the idiom test/assert_test.cpp uses.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<refcount.h>
#include	<strassert.h>

#include	<cstdio>
#include	<cstring>
#include	<climits>
#include	<unistd.h>
#include	<sys/wait.h>
#include	<signal.h>

static int	fails = 0;

static void
expect(const char* what, bool ok)
{
	if (!ok)
		fails++;
	printf("  %-56s %s\n", what, ok ? "ok" : "FAIL");
}

static void
expect_int(const char* what, long got, long want)
{
	expect(what, got == want);
	if (got != want)
		printf("      wanted %ld, got %ld\n", want, got);
}

class	Counted
	: public RefCounted
{
public:
	static int	live;

	Counted() { live++; }
	~Counted() { live--; }

	// The count is protected, so a test reaches it only through a subclass -
	// which is how the overflow path is reached without two billion increments
	void		SetCount(int count) { ref_count = count; }
};

int	Counted::live = 0;

// The pipe the failing child's dump is written into
static int	dump_fd = -1;

static void
write_to_pipe(const char* data, int length)
{
	if (dump_fd >= 0)
		(void)write(dump_fd, data, length);
}

// Run `body` in a child process with the panic dump going into `out`.
// Returns true if the child died by aborting.
static bool
aborts(void (*body)(), char* out, int out_size)
{
	int	fds[2];

	if (pipe(fds) != 0)
		return false;

	fflush(stdout);		// Or the child's abort flushes what it inherited, once per child

	pid_t	child = fork();
	if (child == 0)
	{
		// The child: dump into the pipe, then run the body
		close(fds[0]);
		dump_fd = fds[1];
		strpp_panic_write = write_to_pipe;
		body();
		_exit(0);			// Not reached: the body is expected to die
	}

	close(fds[1]);
	int	got = 0;
	int	n;
	while (got < out_size-1 && (n = (int)read(fds[0], out+got, out_size-1-got)) > 0)
		got += n;
	out[got] = '\0';
	close(fds[0]);

	int	status = 0;
	waitpid(child, &status, 0);
	return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

static void
release_with_nothing_to_release()
{
	Counted	c;
	c.Release();		// The count is already 0, so it would go to -1
}

static void
increment_past_the_end()
{
	Counted	c;
	c.SetCount(INT_MAX);
	c.AddRef();		// The count would wrap round to INT_MIN
}

int
main(int argc, const char** argv)
{
	printf("RefCounted\n");
	Counted*	counted = new Counted();	// Release() deletes it, so it must be on the heap
	expect_int("a new object is unreferenced", counted->GetRefCount(), 0);
	counted->AddRef();
	expect_int("AddRef counts one", counted->GetRefCount(), 1);
	counted->AddRef();
	expect_int("...and another", counted->GetRefCount(), 2);
	counted->Release();
	expect_int("Release takes one back", counted->GetRefCount(), 1);
	expect_int("...and the object is still there", Counted::live, 1);
	counted->Release();
	expect_int("...and the last release destroyed it", Counted::live, 0);

	printf("\nRef<T>\n");
	Counted*	c = new Counted();
	{
		Ref<Counted>	r1(c);
		expect_int("a Ref takes a reference", c->GetRefCount(), 1);
		{
			Ref<Counted>	r2(r1);
			expect_int("a copy takes another", c->GetRefCount(), 2);
			expect("both refer to the same object", (Counted*)r1 == (Counted*)r2);
		}
		expect_int("the copy's destruction gives it back", c->GetRefCount(), 1);

		Ref<Counted>	r3;
		expect("a default Ref is empty", !(bool)r3);
		r3 = r1;
		expect_int("assigning takes another reference", c->GetRefCount(), 2);
	}
	expect_int("the last Ref released it, and it was destroyed", Counted::live, 0);

	printf("\nThe two counts that must not carry on\n");
	{
		char	out[4000];

		expect("a release with no reference left stops the program",
			aborts(release_with_nothing_to_release, out, sizeof out));
		expect("...reporting what was asserted", strstr(out, "was > 0") != 0);

		expect("an increment past the end of the count stops the program",
			aborts(increment_past_the_end, out, sizeof out));
		expect("...reporting what was asserted", strstr(out, "fetch_add") != 0);
	}

	printf("\n%s\n", fails ? "FAILED" : "all refcount checks passed");
	return fails != 0;
}
