#include	"memory_monitor.h"
#include	<variant.h>

void tests();

int
main(int argc, const char** argv)
{
        start_recording_allocations();

	printf("sizeof(Variant) == %ld\n", sizeof(Variant));

	tests();

        if (allocation_growth_count() > 0)      // No allocation should remain unfreed
                report_allocation_growth();

	return 0;
}

void tests()
{
	Variant	vi(23);
	Variant	vll(47LL);
	Variant	vstr("foo");
	Variant vmap(Variant::StrVarMap);

	StrVal		s("foo");
	StrVariantMap	v;
	v.insert(s, 23LL);
	v.insert("baz", vll);
	v.insert("bar", vmap);

	StrVariantMap	vm = vmap.as_variant_map();
	vm.insert("foo", vll);

	Variant&	f = vm["foo"];
	printf("Found type %s\n", f.type_name());

	int	fi = f.as_longlong();
	printf("Found foo=%d\n", fi);
}
