/*
 * Variant data type.
 */
#include	<variant.h>

const char*	Variant::type_names[] = {
	"None",
	// , "Boolean"
	"Integer",
	"Long",
	"LongLong",
	// , "BigNum", "Float", "Double"
	"String",
	"StringArray",
	"VarArray",
	"StrVarMap"
};
