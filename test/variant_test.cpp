#include	"memory_monitor.h"
#include	<variant.h>

#include	<cassert>

void variant_array_tests();
void variant_tests();
void unsigned_tests();

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
	// does, only read differently afterwards: the mutable accessors coerce
	Variant	back(u);
	assert(back.as_longlong() == 4000000000LL);
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
