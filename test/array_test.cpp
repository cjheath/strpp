/*
 * Simple tests for the Array (slices) template
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */

#include	<array.h>
#include	<strval.h>
#include	<errbuf.h>
#include	<str_err.h>
#include	<cstdio>
#include	<cstring>

using	CharArray = Array<char>;
using	PtrArray = Array<const char*>;
using	StrArray = StringArray;

static int	fails = 0;

static void
check(const char* what, const StrArray& got, unsigned expect_len, const char* expect_text)
{
	StrVal	joined = got.join("");
	bool	ok = got.length() == expect_len && joined == expect_text;
	if (!ok)
		fails++;
	printf("  %-14s %u[%s]  %s\n", what, got.length(), joined.asUTF8(), ok ? "ok" : "FAIL");
}

static void
expect(const char* what, bool ok)
{
	if (!ok)
		fails++;
	printf("  %-44s %s\n", what, ok ? "ok" : "FAIL");
}

static void
expect_int(const char* what, long got, long want)
{
	expect(what, got == want);
	if (got != want)
		printf("      wanted %ld, got %ld\n", want, got);
}

static StrArray
strs(const char* s1, const char* s2 = 0, const char* s3 = 0)
{
	StrArray	a;
	a += s1;
	if (s2)
		a += s2;
	if (s3)
		a += s3;
	return a;
}

/*
 * A report is read back and formatted, which is the only way to see it: the
 * buffer holds the number, the default text and the parameters, and formats
 * nothing. It is emptied here, so that every case starts clean.
 */
static StrVal
reported(ErrNum& number)
{
	ErrBuf*	buf = ErrBuffer();
	if (!buf || buf->count() == 0)
	{
		number = 0;
		return "<nothing was reported>";
	}

	StrVal	said;
	{
		// Scoped: a Message holds a slice of the buffer's parameters, and
		// clear() refuses while any slice is outstanding
		ErrBuf::Message	msg = buf->message(0);
		number = msg.error;
		said = StrVal::format(msg.default_text, msg.parameters);
	}
	buf->clear();			// Leaves nothing for the next case
	return said;
}

static void
expect_report(const char* what, ErrNum want_err, const char* want_text)
{
	ErrNum	number = 0;
	StrVal	said = reported(number);
	bool	ok = number == want_err && said == want_text;
	expect(what, ok);
	if (!ok)
		printf("      wanted %08X \"%s\", got %08X \"%s\"\n",
			(unsigned)want_err, want_text, (unsigned)number, said.asUTF8());
}

static void
expect_no_report(const char* what)
{
	ErrNum	number = 0;
	StrVal	said = reported(number);
	expect(what, number == 0);
	if (number)
		printf("      but it reported %08X \"%s\"\n", (unsigned)number, said.asUTF8());
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

	/*
	 * Comparison. A longer array whose prefix equals the comparand used to
	 * compare equal to it, because the loop could not tell "ran out of
	 * comparand" from "ran out of self".
	 */
	printf("\nComparison\n");
	{
		StrArray	a = strs("a");
		StrArray	ab = strs("a", "b");
		StrArray	abc = strs("a", "b", "c");

		expect_int("a shorter array sorts first", a.compare(ab), -1);
		expect_int("a longer array sorts last", abc.compare(ab), 1);
		expect_int("equal arrays compare equal", ab.compare(strs("a", "b")), 0);
		expect_int("a later element sorts last", strs("b").compare(strs("a")), 1);
		expect_int("an earlier element sorts first", strs("a").compare(strs("b")), -1);
	}

	/*
	 * Asking for more elements than there are is a clamp, not a loss: the class
	 * has always said so, and the slices that wrap around used to answer by
	 * accident instead. An *index* past the end is the caller's error, and is
	 * reported.
	 */
	printf("\nSlicing, and requests past the end\n");
	{
		StrArray	abc = strs("a", "b", "c");

		check("head(10)", abc.head(10), 3, "abc");	// Clamped
		check("tail(10)", abc.tail(10), 3, "abc");	// Clamped; used to answer empty
		check("shorter(3)", abc.shorter(3), 0, "");	// All of it shaved off
		check("shorter(10)", abc.shorter(10), 0, "");	// Used to answer the whole array
		expect_no_report("...and none of those reported");

		check("slice(3)", abc.slice(3), 0, "");		// The empty slice at the end
		expect_no_report("...and slice(length) did not report");

		check("slice(4)", abc.slice(4), 0, "");
		expect_report("slice(4) reports the index",
			STRERR_INDEX_OUT_OF_RANGE,
			"An index of 4 is outside an array of 3 elements, so there is nothing to slice");
	}

	// An empty array, and one with no body at all: the slice constructors
	// dereference the body, so a clamp must not try to build a slice of it
	printf("\nAn empty array\n");
	{
		StrArray	empty;
		check("head(10)", empty.head(10), 0, "");
		check("tail(10)", empty.tail(10), 0, "");
		check("shorter(10)", empty.shorter(10), 0, "");
		check("slice(0)", empty.slice(0), 0, "");

		CharArray	chars;			// An Array<T>, whose empty body is a null pointer
		expect("head(10) of an empty Array<char> is empty", chars.head(10).length() == 0);
		expect("tail(10) of an empty Array<char> is empty", chars.tail(10).length() == 0);
		expect("slice(0) of an empty Array<char> is empty", chars.slice(0).length() == 0);
		expect_no_report("...and none of those reported");
	}

	/*
	 * Removing past the end is refused, and changes nothing at all: the caller
	 * has misjudged the array, and no answer here is the one they meant. It
	 * used to write past the allocation under NDEBUG.
	 */
	printf("\nRemoving, and requests past the end\n");
	{
		StrArray	abc = strs("a", "b", "c");

		StrArray	too_long = abc;
		too_long.remove(0, 9);
		check("remove(0,9)", too_long, 3, "abc");
		expect_report("remove(0,9) reports the first index it needed",
			STRERR_INDEX_OUT_OF_RANGE,
			"An index of 3 is outside an array of 3 elements, so there is nothing to remove");

		StrArray	past_end = abc;
		past_end.remove(4);
		check("remove(4)", past_end, 3, "abc");
		expect_report("remove(4) reports the index",
			STRERR_INDEX_OUT_OF_RANGE,
			"An index of 4 is outside an array of 3 elements, so there is nothing to remove");

		StrArray	dropped = abc;
		dropped.drop(9);
		check("drop(9)", dropped, 3, "abc");
		expect_report("drop(9) reports the index",
			STRERR_INDEX_OUT_OF_RANGE,
			"An index of 9 is outside an array of 3 elements, so there is nothing to drop");

		StrArray	taken = abc;
		StrRef		element = taken.delete_at(3);
		check("delete_at(3)", taken, 3, "abc");
		expect("delete_at(3) answers a default element", element.length() == 0);
		expect_report("delete_at(3) reports the index",
			STRERR_INDEX_OUT_OF_RANGE,
			"An index of 3 is outside an array of 3 elements, so there is nothing to take");

		StrArray	at_end = abc;
		at_end.remove(3);				// Removing nothing, at the end, is not an error
		check("remove(3)", at_end, 3, "abc");
		expect_no_report("...and it did not report");

		StrArray	ac = abc;
		ac.remove(1, 1);				// The in-range case still removes
		check("remove(1,1)", ac, 2, "ac");
		expect_no_report("...and it did not report");
	}

	/*
	 * delete_if used to remove nothing at all: the output pointer advanced with
	 * the input and the match branch's decrement cancelled it, so nothing was
	 * ever written through and the length never changed.
	 */
	printf("\ndelete_if\n");
	{
		StrArray	abc = strs("a", "b", "c");

		StrRef		b("b");
		StrArray	without_b = abc;
		without_b.delete_if([&b](const StrRef& e) { return e == b; });
		check("delete_if(b)", without_b, 2, "ac");

		StrArray	unaffected = abc;		// Shared with `abc` until the copy-on-write
		check("...the array it was copied from", unaffected, 3, "abc");
		expect_no_report("...and nothing was reported");

		StrArray	none = abc;
		none.delete_if([](const StrRef& e) { return false; });
		check("delete_if(none)", none, 3, "abc");

		StrArray	all = abc;
		all.delete_if([](const StrRef& e) { return true; });
		check("delete_if(all)", all, 0, "");

		StrArray	only_b = abc;
		only_b.delete_if([&b](const StrRef& e) { return e != b; });
		check("delete_if(not b)", only_b, 1, "b");

		StrArray	sliced = strs("a", "b", "c");
		StrArray	tail = sliced.tail(2);		// A slice with an offset into its body
		tail.delete_if([&b](const StrRef& e) { return e == b; });
		check("delete_if on a slice", tail, 1, "c");
		check("...leaves the array it was sliced from", sliced, 3, "abc");
	}

	// A non-trivial Element, so the elements removed are destroyed and the slots
	// they vacated are default-constructed, which delete_if must go through
	printf("\ndelete_if, on a non-trivial Element\n");
	{
		Array<StrVal>	words;
		words += StrVal("alpha");
		words += StrVal("beta");
		words += StrVal("gamma");

		words.delete_if([](const StrVal& e) { return e == StrVal("beta"); });

		StrVal	joined;
		for (ArrayIndex i = 0; i < words.length(); i++)
			joined = joined + words[i] + ",";
		expect("delete_if on Array<StrVal> keeps the survivors, in order",
			words.length() == 2 && joined == "alpha,gamma,");
	}

	printf("\n%s\n", fails ? "FAILED" : "all array checks passed");
	return fails != 0;
}
