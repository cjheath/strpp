/*
 * Variant data type: the two reports it makes, which cannot be made from the
 * header because reporting needs the message set. See src/strval.cpp.
 */
#include	<variant.h>
#include	<strpp_msg.h>			// The functions that report

const char*	Variant::type_names[] = {
	"None",
	// , "Boolean"
	"Integer",
	"Long",
	"LongLong",
	"UInteger",
	"ULong",
	"ULongLong",
	"Interval",
	"DateTime",
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
 * A coercion to a type that coerce() has no case for. Every type Variant knows
 * has one, so this is a type that was added to the enum and not implemented:
 * the value is kept, and the gap is reported rather than passed over. Where
 * assertions are on this does not return, and the coercion that was asked for
 * is plainly the thing to fix.
 */
void
Variant::no_coercion(VariantType t) const
{
	ErrorVAR_NoCoercion(type_name_at(_type), type_name_at(t));
	assert(!"Coercion not implemented");
}

/*
 * A number that the type it was asked for cannot hold: the report names the
 * type and the value, where must_be above can name only two types.
 *
 * Where assertions are on, this does not return, and the caller never sees the
 * wrong number it would otherwise have been given. Where they are off, no data
 * loss is tolerable, so the value is kept instead of being thrown away: the
 * Variant is left of the closest type that holds it, which any later read -
 * as_signed, as_longlong, type, as_json - then returns correctly. The caller
 * asked for a type that cannot hold the value and gets what it asked for; the
 * data is still there, and the buffer says what happened.
 */
void
Variant::cannot_convert(VariantType t)
{
	ErrorVAR_DoesNotFit(value_text(), type_names[t]);
	assert(!"Value does not fit the type it was asked for");

	VariantType	to = fitting_signed();
	if (to != _type && to != None)
		coerce(to);		// It fits, so this cannot fail
}
