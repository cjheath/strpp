/*
 * Variant data type
 */

#include <strval.h>
#include <array.h>
#include <assert.h>
#include <map>

class	Variant;
typedef Array<StrVal>			StrArray;	// Array of StrVal
typedef Array<Variant>			VariantArray;	// Array of Variant
class	StrVariantMap					// Map from StrVal to Variant
	: public std::map<StrVal, Variant>
{
	using Base = std::map<StrVal, Variant>;
public:
	void	insert(StrVal s, Variant v);
	void	insert(value_type v);			// D'oh, need to re-expose overloads here
};

class	Variant
{
	static const char*	type_names[];

public:
	typedef enum {
		None,
		// , Boolean
		Integer,
		Long,
		LongLong,
		// , BigNum, Float, Double
		String,
		StringArray,
		VarArray,
		StrVarMap,
		//, VarVarMap
		VariantTypeMax = StrVarMap
	} VariantType;

	~Variant()
	{ coerce_none(); }

	// Constructors of various types:
	Variant()
	{ type = None; }
	Variant(int _i)
	{ type = Integer; u.i = _i; }
	Variant(long _l)
	{ type = Long; u.l = _l; }
	Variant(long long _ll)
	{ type = LongLong; u.ll = _ll; }
	Variant(StrVal v)
	{ type = String; new(&u.str) StrVal(v); }
	Variant(StrVal* v, StrArray::Index count)
	{ type = StringArray; new(&u.str_arr) StrArray(v, count); }
	Variant(Variant* v, VariantArray::Index count)
	{ type = VarArray; new(&u.var_arr) VariantArray(v, count); }
	Variant(StrVal* keys, Variant* values, StrArray::Index count)
	{ 	type = StrVarMap;
		new(&u.var_map) StrVariantMap();
		for (StrArray::Index i = 0; i < count; i++)
			u.var_map.insert(StrVariantMap::value_type(keys[i], values[i]));
	}

	// Default-initialise any type
	Variant(VariantType t)
	{
		type = t;
		switch (t)
		{
		default: type = None;	// FALL THROUGH
		case None:		// FALL THROUGH
		case Integer:		// FALL THROUGH
		case Long:		// FALL THROUGH
		case LongLong:
			return;

		case String:		new(&u.str) StrVal(); break;
		case StringArray:	new(&u.str_arr) StrArray(); break;
		case VarArray:		new(&u.var_arr) VariantArray(); break;
		case StrVarMap:		new(&u.var_map) StrVariantMap(); break;
		}
	}

	// Copy constructor: No previous value exists.
	Variant(const Variant& v)
	: type(v.type)
	{
		u.zero();	// Or is this already done?
		switch (v.type)
		{
		default:		break;
		case None:		break;
		case Integer:		u.i = v.as_int(); break;
		case Long:		u.l = v.as_long(); break;
		case LongLong:		u.ll = v.as_longlong(); break;
		case String:		new(&u.str) StrVal(v.as_strval()); break;
		case StringArray:	new(&u.str_arr) StrArray(v.as_string_array()); break;
		case VarArray:		new(&u.var_arr) VariantArray(v.as_variant_array()); break;
		case StrVarMap:		new(&u.var_map) StrVariantMap(v.as_variant_map()); break;
		}
	}

	// Assignment operator. Discards any previous value
	Variant& operator=(const Variant v)
	{
		coerce_none();
		switch (v.type)
		{
		default:		break;
		case None:		break;
		case Integer:		u.i = v.as_int(); break;
		case Long:		u.l = v.as_long(); break;
		case LongLong:		u.ll = v.as_longlong(); break;
		case String:		new(&u.str) StrVal(v.as_strval()); break;
		case StringArray:	new(&u.str_arr) StrArray(v.as_string_array()); break;
		case VarArray:		new(&u.var_arr) VariantArray(v.as_variant_array()); break;
		case StrVarMap:		new(&u.var_map) StrVariantMap(v.as_variant_map()); break;
		}
		return *this;
	}

	// For const references (e.g. when copying) we cannot coerce the type, just assert if it's wrong
	const int&		as_int() const { must_be(Integer); return u.i; }
	const long&		as_long() const { must_be(Long); return u.l; }
	const long long&	as_longlong() const { must_be(LongLong); return u.ll; }
	const StrVal		as_strval() const { must_be(String); return u.str; }
	const StrArray		as_string_array() const { must_be(StringArray); return u.str_arr; }
	const VariantArray	as_variant_array() const { must_be(VarArray); return u.var_arr; }
	const StrVariantMap&	as_variant_map() const { must_be(StrVarMap); return u.var_map; }

	int&			as_int() { coerce(Integer); return u.i; }
	long&			as_long() { coerce(Long); return u.l; }
	long long&		as_longlong() { coerce(LongLong); return u.ll; }
	StrVal			as_strval() { coerce(String); return u.str; }
	StrArray		as_string_array() { coerce(StringArray); return u.str_arr; }
	VariantArray		as_variant_array() { coerce(VarArray); return u.var_arr; }
	StrVariantMap&		as_variant_map() { coerce(StrVarMap); return u.var_map; }

	VariantType		get_type() const { return type; }

	const char*		type_name() const
	{
		if (type >= None && type <= VariantTypeMax)
			return type_names[type];
		return "Corrupt type";
	}

protected:
	void	coerce_none()		// Discard any value and nullify the type
	{
		switch (type)
		{
		default:		break;
		case None:		// FALL THROUGH
		case Integer:		// FALL THROUGH
		case Long:		
		case LongLong:		
			break;		// Nothing to do

		case String:		u.str.~StrVal(); break;
		case StringArray:	u.str_arr.~StrArray(); break;
		case VarArray:		u.var_arr.~VariantArray(); break;
		case StrVarMap:		u.var_map.~StrVariantMap(); break;
		}
		type = None;
	}

	void	coerce(VariantType new_type)
	{
		VariantType	old_type = type;

		// printf("coercing %s to %s\n", type_names[old_type], type_names[new_type]);

		if (old_type == new_type)
			return;		// Nothing to do

		ErrNum		e;
		int32_t		i32;
		switch (new_type)
		{
		default:		// FALL THROUGH
		case None:		// FALL THROUGH
			coerce_none();
			return;

		case Integer:
			switch (old_type)
			{
			case Integer:	return; // Already handled
			case Long:	if (u.l != (int)u.l) break;	// Fail if it would truncate
					u.i = (int)u.l; return;
			case LongLong:	if (u.ll != (int)u.ll) break;	// Fail if it would truncate
					u.i = (int)u.ll; return;
			case String:	i32 = u.str.asInt32(&e, 0);
					if (!e)				// Some error in conversion
						break;
					u.i = i32;
					type = new_type;
					return;
			case None:		// FALL THROUGH
			case StringArray:	// FALL THROUGH
			case VarArray:		// FALL THROUGH
			case StrVarMap:		// FALL THROUGH
					break;	// Cannot coerce
			}
			break;

		case Long:
			switch (old_type)
			{
			case Integer:	u.l = u.i; return;
			case Long:	return; // Already handled
			case LongLong:	if (u.ll != (long)u.ll) break;	// Fail if it would truncate
					u.l = (long)u.ll; return;
			case String:	i32 = u.str.asInt32(&e, 0);	// REVISIT: int32 only, or it fails
					if (!e)				// Some error in conversion
						break;
					u.l = i32;
					type = new_type;
					return;
			case None:		// FALL THROUGH
			case StringArray:	// FALL THROUGH
			case VarArray:		// FALL THROUGH
			case StrVarMap:		// FALL THROUGH
					break;	// Cannot coerce
			}
			break;

		case LongLong:
			switch (old_type)
			{
			case Integer:	u.l = u.i; return;
			case Long:	u.ll = u.l; return;
			case LongLong:	return; // Already handled
			case String:	i32 = u.str.asInt32(&e, 0);	// REVISIT: int32 only, or it fails
					if (e)				// Some error in conversion
						break;
					u.ll = i32;
					type = new_type;
					return;
			case None:		// FALL THROUGH
			case StringArray:	// FALL THROUGH
			case VarArray:		// FALL THROUGH
			case StrVarMap:		// FALL THROUGH
					break;	// Cannot coerce
			}
			break;

		case String:
			switch (old_type)
			{
			char buf[24];	// Big enough for 64-bit integer
			case Integer:	snprintf(buf, sizeof(buf), "%d", u.i);
					*this = StrVal(buf);
					return;
			case Long:	snprintf(buf, sizeof(buf), "%ld", u.l);
					*this = StrVal(buf);
					return;
			case LongLong:	snprintf(buf, sizeof(buf), "%lld", u.ll);
					*this = StrVal(buf);
					return;
			case String:	return; // Already handled
			case None:		// FALL THROUGH
			case StringArray:	// FALL THROUGH
			case VarArray:		// FALL THROUGH
			case StrVarMap:		// FALL THROUGH
					break;	// Cannot coerce
			}
			break;

		case StringArray:	break;	// REVISIT: No coercion implemented
		case VarArray:		break;	// REVISIT: No coercion implemented
		case StrVarMap:		break;	// REVISIT: No coercion implemented
		}
		must_be(new_type);		// Report impossible coercion
	}

	// Type assertion:
	void	must_be(VariantType t) const
	{
		if (type == t)
			return;
		printf("Expected type %d, got type %d\n", t, type);
		assert(!"Mismatched type");
	}

	VariantType		type;
	union u
	{
		int		i;
		long		l;
		long long	ll;
		StrVal		str;
		StrArray	str_arr;
		VariantArray	var_arr;
		StrVariantMap	var_map;

		u()		{ zero(); }
		~u()		{}	// Destruction happens outside here
		void zero()	{ memset(this, 0, sizeof(*this)); }
	} u;
};

void	StrVariantMap::insert(StrVal s, Variant v) { insert(StrVariantMap::value_type(s, v)); }
void	StrVariantMap::insert(value_type v) { Base::insert(v); }

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
	//, "VarVarMap"
};
