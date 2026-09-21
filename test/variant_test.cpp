#include	"memory_monitor.h"
#include	<variant.h>
#include	<errbuf.h>		// The child reads its own error buffer

#include	<cassert>
#include	<csignal>
#include	<fcntl.h>
#include	<unistd.h>
#include	<sys/wait.h>

void variant_array_tests();
void variant_tests();
void unsigned_tests();

// The pipe a dying child's error buffer is written into
static int	report_fd = -1;

/*
 * The report is made before the assertion kills the process, so the only way to
 * read it is from inside the dying child. abort() raises SIGABRT, so a handler
 * for it can drain the buffer into the pipe first and then let abort finish.
 *
 * Each Message holds a slice of the buffer's parameter array, and delivered()
 * refuses while any slice is outstanding, so the Message must be gone before it
 * is called - hence the inner scope, as in strassert.cpp.
 */
static void
dump_error_buffer(int sig)
{
	ErrBuf*	buf = ErrBuffer();
	if (buf && report_fd >= 0)
		for (ErrBuf::MsgIndex i = 0; i < buf->count(); i++)
		{
			StrVal	line;
			{
				ErrBuf::Message	msg = buf->message(i);
				line = StrVal::fromInt32((int32_t)msg.error, 'X')+": "
					+ StrVal::format(msg.default_text, msg.parameters)+"\n";
			}
			(void)!write(report_fd, line.asUTF8(), line.numBytes());
			buf->delivered();
		}
	signal(sig, SIG_DFL);		// Return, and abort() finishes the job
}

/*
 * A coercion that must fail kills the process, so it is run in a child and
 * judged by how the child died and what it had reported - the same way
 * assert_test.cpp tests a failure of StrppAssert. What the child says on
 * standard error is sent nowhere, so the output stays clean.
 */
static bool
coercion_aborts(bool (*body)(), StrVal& report)
{
	int	fds[2];
	if (pipe(fds) != 0)
		return false;

	// Empty this process's buffered output before forking: abort() flushes
	// what a child inherited, so every line printed so far would come out
	// once per child as well
	fflush(stdout);

	pid_t	child = fork();
	if (child == 0)
	{
		close(fds[0]);
		int	devnull = open("/dev/null", O_WRONLY);
		if (devnull >= 0)
			dup2(devnull, 2);
		report_fd = fds[1];
		signal(SIGABRT, dump_error_buffer);
		body();				// Expected to die, not to return
		_exit(0);
	}

	close(fds[1]);
	char	buf[512];
	int	got = 0;
	int	n;
	while (got < 511 && (n = (int)read(fds[0], buf+got, 511-got)) > 0)
		got += n;
	buf[got] = '\0';
	close(fds[0]);
	report = StrVal(buf);

	int	status = 0;
	waitpid(child, &status, 0);
	return WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT;
}

// Beyond what a signed int holds, so reading it as one would change the number
static bool	uint_does_not_fit_int()
{
	Variant	too_big(4000000000u);
	int	overflowed = too_big.as_int();
	(void)overflowed;
	return true;
}

// The same one width up: an unsigned long beyond LONG_MAX read as a long
static bool	ulong_does_not_fit_long()
{
	Variant	too_big(18000000000000000000ul);
	long	overflowed = too_big.as_long();
	(void)overflowed;
	return true;
}

// Beyond LONG_MAX as well, so no signed type holds it and as_signed() has none
// to choose. On a target where a long is as wide as a long long, the test above
// is already this case; it is written out anyway so that it is tested wherever.
static bool	ullong_does_not_fit_signed()
{
	Variant	too_big(18446744073709551615ull);
	long long	overflowed = too_big.as_signed();
	(void)overflowed;
	return true;
}

int
main(int argc, const char** argv)
{
#if defined(MEMCHECK)
	start_recording_allocations();
#endif

	printf("sizeof(Variant) == %ld\n", sizeof(Variant));
	printf("sizeof(StrRef) == %ld\n", sizeof(StrRef));
	printf("sizeof(StrVal) == %ld\n", sizeof(StrVal));
	printf("sizeof(StringArray) == %ld\n", sizeof(StringArray));
	printf("sizeof(VariantArray) == %ld\n", sizeof(VariantArray));
	printf("sizeof(StrVariantMap) == %ld\n", sizeof(StrVariantMap));

	variant_array_tests();
	variant_tests();
	unsigned_tests();

#if defined(MEMCHECK)
	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();
#endif

	return 0;
}

void variant_array_from_param(VariantArray a)
{
	printf("VariantArray from param = %s\n", Variant(a).as_json().asUTF8());
}

void variant_array_tests()
{
	VariantArray	va;
	StringArray	sa;

	sa << "foo";
	sa << "bar";

	VariantArray	va2;
	va2 << 69;
	va2 << 81729L;

	StrVariantMap	map;
	map.put("fred", 23);
	map.put("fly", "boo");

	va << true;	// Gets cast to int
	va << 4;
	va << 4L;
	va << 8LL;
	va << "baz";
	va << sa;
	va << va2;
	va << map;
	printf("VariantArray = %s\n", Variant(va).as_json().asUTF8());

	va.clear();

	va = Variant(23) << "appendage";	// Get an array by appending to a Variant
	va << 1234567890123456789LL;		// And again
	printf("VariantArray from append = %s\n", Variant(va).as_json().asUTF8());

	VariantArray	na("boo");		// Construct from value coerced to Variant
	na << va[2];				// Append a 2nd value
	printf("VariantArray from element = %s\n", Variant(na).as_json().asUTF8());

	// variant_array_from_param("bah");	// Unfortunately this can't be made to work
	variant_array_from_param(VariantArray("bah"));			// This works
	variant_array_from_param(VariantArray("baz") << 31);		// and this
	variant_array_from_param(VariantArray() << "bah" << 47);	// this too
	variant_array_from_param("bah" << Variant(53));			// So does this
	variant_array_from_param(Variant(29));

	/*
	 * The closing bracket of a nested structure stands at its *parent's*
	 * indent, one level less than the opening. Sizing it as "the separator
	 * less one level per depth" instead took the parent's indent away too,
	 * which looks right at the outermost level and is wrong everywhere else -
	 * and nothing here had covered the indented mode at depth until px's
	 * golden files caught it.
	 */
	{
		VariantArray	inner;
		inner << 2 << 3;
		VariantArray	outer;
		outer << 1 << inner;
		Variant		v(outer);

		assert(v.as_json(-2) == "[1,[2,3]]");		// Tight
		assert(v.as_json(-1) == "[ 1, [ 2, 3 ] ]");	// Compact: spaces, inside too
		assert(v.as_json(0) == "[\n  1,\n  [\n    2,\n    3\n  ]\n]");
		assert(v.as_json(1) == "[\n    1,\n    [\n      2,\n      3\n    ]\n  ]");
		printf("as_json: tight, compact and both indented forms close at the parent's indent\n");
	}
}

/*
 * The unsigned types. Each shares its signed twin's storage, so what is worth
 * checking is that the value is neither lost nor reinterpreted, including where
 * a signed reading of those bits would have to be a different number.
 */
void unsigned_tests()
{
	// Const, so that the accessors used below are the ones that only read: the
	// mutable as_strval() coerces, and would leave these as Strings
	const Variant	u(4000000000u);			// Beyond what a signed int holds
	const Variant	ul(18000000000000000000ul);	// And beyond a signed long
	const Variant	ull(18446744073709551615ull);	// The largest there is

	printf("unsigned types: %s, %s, %s\n", u.type_name(), ul.type_name(), ull.type_name());
	assert(u.type() == Variant::UInteger);
	assert(ul.type() == Variant::ULong);
	assert(ull.type() == Variant::ULongLong);

	assert(u.as_uint() == 4000000000u);
	assert(ul.as_ulong() == 18000000000000000000ul);
	assert(ull.as_ulonglong() == 18446744073709551615ull);

	// A signed reading of the same bits is a different number: these are the
	// digits of the unsigned value, which is what the types are for
	// A copy, because the const accessors only assert: it is the mutable ones
	// that coerce, and the coercion is what renders the unsigned digits
	Variant	mu(u), mul(ul), mull(ull);
	StrVal	us = mu.as_strval();
	StrVal	uls = mul.as_strval();
	StrVal	ulls = mull.as_strval();
	assert(us == "4000000000");
	assert(uls == "18000000000000000000");
	assert(ulls == "18446744073709551615");
	printf("as text: %s, %s, %s\n", us.asUTF8(), uls.asUTF8(), ulls.asUTF8());

	assert(Variant(u).as_json() == "4000000000");
	assert(Variant(ull).as_json() == "18446744073709551615");

	// A copy carries the type and the value, and coerces as its signed twin
	// does. Widening goes by value, not by bits, and the type follows: after
	// the coercion it is a LongLong and reads as one.
	Variant	back(u);
	assert(back.as_longlong() == 4000000000LL);
	assert(back.type() == Variant::LongLong);

	// as_signed() answers the value in the closest signed type that holds it,
	// which is the read that does not have to be told the width
	{
		Variant	a(5u);
		assert(a.as_signed() == 5 && a.type() == Variant::Integer);
		Variant	b(u);				// 4000000000, beyond INT_MAX
		assert(b.as_signed() == 4000000000LL && b.type() == Variant::Long);
		Variant	c(5ul);
		assert(c.as_signed() == 5 && c.type() == Variant::Long);
		Variant	d(5ull);
		assert(d.as_signed() == 5 && d.type() == Variant::LongLong);
		Variant	e(47L);				// Already signed: answered as it stands
		assert(e.as_signed() == 47 && e.type() == Variant::Long);
		Variant	f("42");
		assert(f.as_signed() == 42 && f.type() == Variant::Integer);
		printf("as_signed: 5u as Integer, 4000000000u as Long, \"42\" as Integer\n");
	}

	// A signed type of the same width cannot hold those values, so the coercion
	// is refused rather than reinterpreting the bits. Each of these dies, so
	// each is run in a child, which reports through the error buffer before it
	// goes. as_signed() dies for the last two as well: no signed type holds
	// them, so there is none for it to choose.
	StrVal	report;
	assert(coercion_aborts(uint_does_not_fit_int, report));
	assert(report == "A0000802: Cannot convert to a `Integer` because the value"
			 " 4000000000 does not fit\n");
	assert(coercion_aborts(ulong_does_not_fit_long, report));
	assert(report == "A0000802: Cannot convert to a `Long` because the value"
			 " 18000000000000000000 does not fit\n");
	assert(coercion_aborts(ullong_does_not_fit_signed, report));
	assert(report == "A0000802: Cannot convert to a `LongLong` because the value"
			 " 18446744073709551615 does not fit\n");
	printf("refused: a UInteger beyond INT_MAX, a ULong beyond LONG_MAX\n");
	printf("...each naming the value it could not hold\n");
}

void variant_tests()
{
	Variant vi(23);
	Variant vll(47LL);
	Variant vstr("foo");
	Variant vmap(Variant::StrVarMap);

	StrVal		s("foo");
	StrVariantMap	v;
	v.insert(s, 23LL);
	v.insert("baz", vll);

	v.insert("bar", vmap);
printf("v has %ld elements\n", v.size());

	StrVariantMap	vm = vmap.as_variant_map();
	StrVariantMap	vm2 = vm;
	// This will Unshare vm
	vm.insert("foo", vll);
printf("vm2 has %ld elements\n", vm2.size());
printf("vm has %ld elements\n", vm.size());

	Variant f = vm["foo"];
	printf("Found \"foo\" as type %s\n", f.type_name());

	int	fl = f.as_long();
	printf("Found foo=%d as_long\n", fl);

	StrVal	fs = f.as_strval();

	printf("Found foo=\"%s\" as strval\n", fs.asUTF8());
}
