/*
 * Variant data type.
 */
#include	<variant.h>
#include	<errbuf.h>			// For the type assertion below

const char*	Variant::type_names[] = {
	"None",
	// , "Boolean"
	"Integer",
	"Long",
	"LongLong",
	"UInteger",
	"ULong",
	"ULongLong",
	// , "BigNum", "Float", "Double"
	"String",
	"StrArray",
	"VarArray",
	"StrVarMap"
};

/*
 * The type assertion. It both reports and asserts: a program built with
 * assertions left out would otherwise carry on with the wrong type, so the
 * report is made whether or not the assertion is.
 */
void
Variant::must_be(VariantType t) const
{
	if (_type == t)
		return;

	Error(VARERR_WRONG_TYPE, "A `{1}` was expected, but this Variant is a `{2}`",
		VariantArray() << type_names[t] << type_names[_type]);
	assert(!"Mismatched type");
}
