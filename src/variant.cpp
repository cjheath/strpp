/*
 * Variant data type: the two reports it makes, which cannot be made from the
 * header because reporting needs the message set. See src/strval.cpp.
 */
#include	<variant.h>
#include	<str_msg.h>			// The functions that report

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

	ErrorVAR_WrongType(type_names[t], type_names[_type]);
	assert(!"Mismatched type");
}

/*
 * A number that the type it was asked for cannot hold: the report names the
 * type and the value, where must_be above can name only two types.
 *
 * Where assertions are on, this does not return, and the caller never sees the
 * wrong number it would otherwise have been given. Where they are off, no data
 * loss is tolerable, so the value is kept instead of being thrown away: the
 * Variant is left of the closest type that holds it, which any later read -
 * as_signed, as_longlong, type, as_json - then answers correctly. The caller
 * asked for a type that cannot hold the value and gets what it asked for; the
 * data is still there, and the buffer says what happened.
 */
void
Variant::cannot_convert(VariantType t)
{
	ErrorVAR_DoesNotFit(type_names[t], value_text());
	assert(!"Value does not fit the type it was asked for");

	VariantType	to = fitting_signed();
	if (to != _type && to != None)
		coerce(to);		// It fits, so this cannot fail
}
