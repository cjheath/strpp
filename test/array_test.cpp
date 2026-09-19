/*
 * Simple tests for the Array (slices) template
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */

#include	<array.h>
#include	<strval.h>
#include	<cstdio>
#include	<cstring>

using	CharArray = Array<char>;
using	PtrArray = Array<const char*>;
using	StrArray = StringArray;

static int	fails = 0;

static void
check(const char* what, StrArray& got, unsigned expect_len, const char* expect_text)
{
	StrVal	joined = got.join("");
	bool	ok = got.length() == expect_len && joined == expect_text;
	if (!ok)
		fails++;
	printf("  %-14s %u[%s]  %s\n", what, got.length(), joined.asUTF8(), ok ? "ok" : "FAIL");
}

int
main(int argc, const char** argv)
{
	CharArray	ca;
	printf("CharArray\n");
	ca += 'a';
	printf("ca @%p = '%.*s'\n", ca.asElements(), ca.length(), ca.asElements());
	printf("Copy cb = ca\n");
	CharArray	cb = ca;
	printf("cb @%p = '%.*s'\n", cb.asElements(), cb.length(), cb.asElements());
	printf("Mutate cb\n");
	cb += 'b';
	printf("ca @%p = '%.*s'\n", ca.asElements(), ca.length(), ca.asElements());
	printf("cb @%p = '%.*s'\n", cb.asElements(), cb.length(), cb.asElements());

	PtrArray	pa;
	printf("\nPtrArray\n");
	pa += "a";
	printf("pa @%p = %d[%s]\n", pa.asElements(), pa.length(), pa[0]);
	printf("Copy&mutate pb = pa+\"b\"\n");
	PtrArray pb = pa+"b";
	printf("pa @%p = %d[%s]\n", pa.asElements(), pa.length(), pa[0]);
	printf("pb @%p = %d[%s, %s]\n", pb.asElements(), pb.length(), pb[0], pb[1]);

	StrArray	sa;
	printf("\nStrArray\n");
	sa += "a";
	printf("sa @%p = %d[%s]\n", sa.asElements(), sa.length(), sa[0].asUTF8());
	printf("Copy&mutate sb = sa+\"b\"\n");
	StrArray sb = sa+"b";
	printf("sa @%p = %d[%s]\n", sa.asElements(), sa.length(), sa[0].asUTF8());
	printf("sb @%p = %d[%s, %s]\n", sb.asElements(), sb.length(), sb[0].asUTF8(), sb[1].asUTF8());
	printf("sb.join(+) = %s\n", sb.join("+").asUTF8());
	StrArray sbc = sb+"c";
	printf("sbc @%p = %d[%s, %s, %s]\n", sbc.asElements(), sbc.length(), sbc[0].asUTF8(), sbc[1].asUTF8(), sbc[2].asUTF8());
	StrArray sc = sbc;
	sc.remove(1, 1);
	printf("sc @%p = %d[%s, %s]\n", sc.asElements(), sc.length(), sc[0].asUTF8(), sc[1].asUTF8());

	// Test that compare() uses Element::operator<()
	StrArray sbc2 = sbc;
	printf("sbc2 @%p = %d[%s, %s, %s]\n", sbc2.asElements(), sbc2.length(), sbc2[0].asUTF8(), sbc2[1].asUTF8(), sbc2[2].asUTF8());

	// Mutate sbc2 and return it to the same state
	StrArray sbc3 = sbc2.shorter(1);
	sbc2 += "d";
	printf("sbc3 @%p = %d[%s, %s]\n", sbc3.asElements(), sbc3.length(), sbc3[0].asUTF8(), sbc3[1].asUTF8());
	sbc2.remove(3);

	printf("sbc3 @%p = %d[%s, %s]\n", sbc3.asElements(), sbc3.length(), sbc3[0].asUTF8(), sbc3[1].asUTF8());
	sbc3 += "d";		// Mutate it
	printf("sbc3 @%p = %d[%s, %s, %s]\n", sbc3.asElements(), sbc3.length(), sbc3[0].asUTF8(), sbc3[1].asUTF8(), sbc3[2].asUTF8());
	printf("sbc == sbc2 -> %s\n", (sbc == sbc2) ? "true" : "false");	// Should be true
	// End of compare() test

	// Check that shorter() (which uses slice()) worked correctly:
	printf("sbc3 @%p = %d[%s, %s, %s]\n", sbc3.asElements(), sbc3.length(), sbc3[0].asUTF8(), sbc3[1].asUTF8(), sbc3[2].asUTF8());
	printf("sbc == sbc3 -> %s\n", (sbc == sbc3) ? "true" : "false");	// Should be false

	/*
	 * Slicing to the end of a body. A slice's bounds are inclusive of the end,
	 * so all of these are legal - the whole array, a slice whose length runs to
	 * the end, and tail(). Offsets and lengths that sum to exactly the body's
	 * length used to trip an over-strict assertion in the slice constructor,
	 * which tail() produces as a matter of course.
	 */
	printf("\nSlice bounds\n");
	{
		StrArray	abc;
		abc += "a";
		abc += "b";
		abc += "c";

		StrArray	all = abc.slice(0, abc.length());	// The whole array
		StrArray	rest = abc.slice(0);			// Length defaults to the rest
		StrArray	whole = abc.slice(0, 3);		// Explicitly to the end
		StrArray	last = abc.tail(1);			// Starts one before the end
		StrArray	from1 = abc.slice(1, 2);		// Ending at the end

		check("slice(0,length)", all, 3, "abc");
		check("slice(0)", rest, 3, "abc");
		check("slice(0,3)", whole, 3, "abc");
		check("tail(1)", last, 1, "c");
		check("slice(1,2)", from1, 2, "bc");
	}

	printf("\n%s\n", fails ? "FAILED" : "all slice checks passed");
	return fails != 0;
}
