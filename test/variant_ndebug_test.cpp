/*
 * What a refused coercion does where assertions are compiled out.
 *
 * Where they are on, the coercion reports and then dies, so nothing about the
 * Variant can be observed afterwards: variant_test.cpp can check only that it
 * died and what it said. That leaves the other half untested, and it is the
 * half that must not lose data - a program built without assertions has to keep
 * the value, because there is no one to tell but the error buffer.
 *
 * It needs NDEBUG throughout, including the library's own sources, so the
 * Makefile builds it separately: see the variant_ndebug_test rule.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<variant.h>
#include	<errbuf.h>

#include	<cstdio>

static int	test_count;
static int	failure_count;

static void
expect(const char* when, bool passed)
{
	test_count++;
	if (!passed)
	{
		printf("%d:\t%s: FAIL\n", test_count, when);
		failure_count++;
	}
}

int
main()
{
#if !defined(NDEBUG)
	printf("This test must be built with NDEBUG: see the variant_ndebug_test rule\n");
	return 2;
#else
	// An unsigned value beyond what its signed twin holds: the Variant must
	// keep the value, in a type that holds it
	{
		Variant	v(4000000000u);
		(void)v.as_int();		// Must return, not die
		printf("UInteger beyond INT_MAX -> %s holding %s\n",
			v.type_name(), v.as_json().asUTF8());	// as_json() reads, as_strval() coerces
		expect("as_int() returned rather than dying", true);
		expect("the value was kept, in a Long", v.as_long() == 4000000000L);
	}

	// A value no signed type holds: it stays where it is, still not lost
	{
		Variant	v(18446744073709551615ull);
		(void)v.as_long();		// Must return, not die
		printf("ULongLong beyond LLONG_MAX -> %s holding %s\n",
			v.type_name(), v.as_json().asUTF8());
		expect("the value stays unsigned",
			v.type() == Variant::ULongLong
			&& v.as_ulonglong() == 18446744073709551615ull);
	}

	// A signed source already holds its value: nothing is changed, and the
	// caller still gets what it asked for
	{
		Variant	v(5000000000LL);
		(void)v.as_int();		// Must return, not die
		printf("LongLong beyond INT_MAX -> %s holding %s\n",
			v.type_name(), v.as_json().asUTF8());
		expect("a signed value is left as it stands",
			v.type() == Variant::LongLong && v.as_longlong() == 5000000000LL);
	}

	// The report is the only thing that tells the caller what happened
	{
		Variant	v(4000000000u);
		(void)v.as_int();

		ErrBuf*	buf = ErrBuffer();
		expect("the refusal was reported", buf && buf->count() > 0);

		if (buf && buf->count() > 0)
		{
			StrVal	said;
			{	// The Message holds a slice of the buffer's parameters, and
				// delivered() refuses while any slice is outstanding
				ErrBuf::Message	msg = buf->message(0);
				said = StrVal::format(msg.default_text, msg.parameters);
			}
			printf("the buffer says: %s\n", said.asUTF8());
			expect("...naming the type and the value",
				said == "Cannot convert to a `Integer` because the value 4000000000 does not fit");
			buf->delivered();
		}
	}

	printf("Completed %d tests with %d failures\n", test_count, failure_count);
	return failure_count != 0;
#endif
}
