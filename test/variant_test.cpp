#include	"memory_monitor.h"
#include	<variant.h>

void variant_array_tests();
void variant_tests();

int
main(int argc, const char** argv)
{
	start_recording_allocations();

	printf("sizeof(Variant) == %ld\n", sizeof(Variant));

	variant_array_tests();
	variant_tests();

	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();

	return 0;
}

void variant_array_tests()
{
	VariantArray	va;
	StrArray	sa;

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
