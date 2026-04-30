#include	"memory_monitor.h"
#include	<variant.h>

void variant_array_tests();
void variant_tests();

int
main(int argc, const char** argv)
{
	start_recording_allocations();

	printf("sizeof(Variant) == %ld\n", sizeof(Variant));
	printf("sizeof(StrRef) == %ld\n", sizeof(StrRef));
	printf("sizeof(StrVal) == %ld\n", sizeof(StrVal));
	printf("sizeof(StringArray) == %ld\n", sizeof(StringArray));
	printf("sizeof(VariantArray) == %ld\n", sizeof(VariantArray));
	printf("sizeof(StrVariantMap) == %ld\n", sizeof(StrVariantMap));

	variant_array_tests();
	variant_tests();

	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();

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
