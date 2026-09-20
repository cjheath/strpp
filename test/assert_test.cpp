/*
 * Standalone test suite for StrppAssert (see include/strassert.h).
 *
 * A failed assertion reports to the error buffer, dumps it, and aborts, so
 * this suite runs the failures in a child process and examines what the child
 * wrote and how it died. The dump goes through strpp_panic_write, which the
 * child points at a pipe rather than at standard error.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<strassert.h>
#include	<variant.h>

#include	<cstdio>
#include	<cstring>
#include	<csignal>
#include	<unistd.h>
#include	<sys/wait.h>

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

// The pipe the failing child's dump is written into
static int	dump_fd = -1;

static void
write_to_pipe(const char* data, int length)
{
	if (dump_fd >= 0)
		(void)write(dump_fd, data, length);
}

// Run `body` in a child process with the panic dump going into `out`.
// Answers true if the child died by aborting.
static bool
aborts(void (*body)(), char* out, int out_size)
{
	int	fds[2];

	if (pipe(fds) != 0)
		return false;

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
assert_false()
{
	StrppAssert(1 == 2);
}

static void
assert_true()
{
	StrppAssert(1 == 1);
	_exit(0);				// Passes, so the child must leave of its own accord
}

// A writer that itself asserts: the dump must die at once, not recurse
static void
asserting_writer(const char* data, int length)
{
	(void)data;
	(void)length;
	StrppAssert(!"the dump's own writer is not to be trusted");
}

static void
assert_while_dumping()
{
	strpp_panic_write = asserting_writer;
	StrppAssert(1 == 2);
}

static int
run_quietly()
{
	// A child that passes its assertion: no signal, exit code 0
	pid_t	child = fork();
	if (child == 0)
	{
		assert_true();
		_exit(1);			// Only if assert_true returns, which it should not
	}
	int	status = 0;
	waitpid(child, &status, 0);
	return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

int
main(int argc, const char** argv)
{
	if (argc > 1 && 0 == strcmp("-p", argv[1]))
		show_passes = true;

	char	out[2048];

	test_group("StrppAssert: a failed assertion dumps and dies");
	expect("an assertion that fails aborts the process", aborts(assert_false, out, sizeof(out)));
	expect("...naming the condition it failed on", strstr(out, "`1 == 2`") != 0);
	expect("...naming the file it is in", strstr(out, "assert_test.cpp") != 0);
	expect("...and the error number of the message", strstr(out, "error A") != 0);
	expect("...with the message's default text, not a formatted copy",
		strstr(out, "Assertion failed:") != 0);

	test_group("StrppAssert: an assertion that holds is not a failure");
	expect("an assertion that holds leaves the process alone", run_quietly());

	test_group("StrppAssert: a failure while dumping dies at once");
	expect("a writer that asserts does not make a second dump",
		aborts(assert_while_dumping, out, sizeof(out)));
	expect("...and what it was to write is abandoned, not written again",
		strstr(out, "Assertion failed") == 0);

	printf("Completed %d tests with %d failures\n", test_count, failure_count);
	return failure_count == 0 ? 0 : 1;
}
