#if !defined(VARIANT_H)
#define VARIANT_H
/*
 * Variant data type.
 */
#include	<assert.h>
#include	<functional>

#include	<strval.h>
#include	<str_err.h>			// The error numbers reported from src/variant.cpp
#include	<array.h>
#include	<cowmap.h>

/*
 * The most levels of array or map that rendering descends to, whether by
 * format() or by as_json(). A structure deeper than any text needs cannot then
 * run away with the stack, whatever a program passes in. A build may set it:
 * see the DEPTH option in the Makefile.
 */
#if	!defined(RENDER_MAX_DEPTH)
#define	RENDER_MAX_DEPTH	16
#endif

class	Variant;

// Complex reference-counted types we can use in a Variant:
typedef	Array<Variant>	VariantArray;

class	StrVariantMap			// Map from StrVal to Variant
: public CowMap<Variant, StrVal>
{
public:
	StrVariantMap() {}
	StrVariantMap(const StrVal* keys, const Variant* values, int size)
	: CowMap<Variant, StrVal>(keys, values, size)
	{ }
};

class	Variant
{
public:
	typedef enum {
		None,
		// , Boolean
		Integer,
		Long,
		LongLong,
		// The same three, but unsigned. Stored in the same union word as its signed twin
		UInteger,
		ULong,
		ULongLong,
		// , BigNum, Float, Double
		String,
		StrArray,
		VarArray,
		StrVarMap,
		VariantTypeMax = StrVarMap
	} VariantType;

	VariantType		type() const { return _type; }
	bool			is_null() const { return _type == None; }
	static const char*	type_names[];
	const char*		type_name() const
	{
		if (_type >= None && _type <= VariantTypeMax)
			return type_names[_type];
		return "Corrupt type";
	}

	~Variant()
	{ coerce_none(); }

	// Constructors of various types:
	Variant()							// None
	{ _type = None; }
	Variant(int _i)							// Integer
	{ _type = Integer; u.i = _i; }
	Variant(long _l)						// Long
	{ _type = Long; u.l = _l; }
	Variant(long long _ll)						// LongLong
	{ _type = LongLong; u.ll = _ll; }
	Variant(unsigned _u)						// UInteger
	{ _type = UInteger; u.i = (int)_u; }
	Variant(unsigned long _ul)					// ULong
	{ _type = ULong; u.l = (long)_ul; }
	Variant(unsigned long long _ull)				// ULongLong
	{ _type = ULongLong; u.ll = (long long)_ull; }
	Variant(StrVal v)						// StrRef
	{ _type = String; new(&u.str) StrRef(v); }
	Variant(const char* s)						// StrRef
	{ _type = String; new(&u.str) StrRef(s); }
	Variant(StringArray a)						// StrArray
	{ _type = StrArray; new(&u.str_arr) StringArray(a); }
	Variant(StrVal* v, StringArray::Index count)
	{ _type = StrArray; new(&u.str_arr) StringArray(v, count); }
	Variant(VariantArray a)						// VarArray
	{ _type = VarArray; new(&u.str_arr) VariantArray(a); }
	Variant(Variant* v, VariantArray::Index count)
	{ _type = VarArray; new(&u.var_arr) VariantArray(v, count); }
	Variant(StrVal* keys, Variant* values, StringArray::Index count)	// StrVarMap
	{ 	_type = StrVarMap;
		new(&u.var_map) StrVariantMap();
		for (StringArray::Index i = 0; i < count; i++)
			u.var_map.insert(keys[i], values[i]);
	}
	Variant(StrVariantMap map)					// StrVarMap
	{ _type = StrVarMap; u.var_map = map; }

	// Default-initialise any _type
	Variant(VariantType t)
	{
		_type = t;
		switch (t)
		{
		default: _type = None;	// FALL THROUGH
		case None:		// FALL THROUGH
		case Integer:		// FALL THROUGH
		case Long:		// FALL THROUGH
		case LongLong:
			return;		// The union u has been zeroed already

		case String:		new(&u.str) StrRef(); break;
		case StrArray:		new(&u.str_arr) StringArray(); break;
		case VarArray:		new(&u.var_arr) VariantArray(); break;
		case StrVarMap:		new(&u.var_map) StrVariantMap(); break;
		}
	}

	// Copy constructor
	Variant(const Variant& v)
	: _type(v._type)
	{
		switch (v._type)
		{
		default:		break;
		case None:		break;
		case Integer:		u.i = v.as_int(); break;
		case Long:		u.l = v.as_long(); break;
		case LongLong:		u.ll = v.as_longlong(); break;
		case UInteger:		u.i = (int)v.as_uint(); break;
		case ULong:		u.l = (long)v.as_ulong(); break;
		case ULongLong:		u.ll = (long long)v.as_ulonglong(); break;
		case String:		new(&u.str) StrRef(v.as_strval()); break;
		case StrArray:		new(&u.str_arr) StringArray(v.as_string_array()); break;
		case VarArray:		new(&u.var_arr) VariantArray(v.as_variant_array()); break;
		case StrVarMap:		new(&u.var_map) StrVariantMap(v.as_variant_map()); break;
		}
	}

	// Assignment operator. Discards any previous value
	Variant& operator=(const Variant v)
	{
		coerce_none();		// Discard previous value and zero the union
		switch (v._type)
		{
		default:		break;
		case None:		break;
		case Integer:		u.i = v.as_int(); break;
		case Long:		u.l = v.as_long(); break;
		case LongLong:		u.ll = v.as_longlong(); break;
		case UInteger:		u.i = (int)v.as_uint(); break;
		case ULong:		u.l = (long)v.as_ulong(); break;
		case ULongLong:		u.ll = (long long)v.as_ulonglong(); break;
		case String:		new(&u.str) StrRef(v.as_strval()); break;
		case StrArray:		new(&u.str_arr) StringArray(v.as_string_array()); break;
		case VarArray:		new(&u.var_arr) VariantArray(v.as_variant_array()); break;
		case StrVarMap:		new(&u.var_map) StrVariantMap(v.as_variant_map()); break;
		}
		_type = v._type;
		return *this;
	}

	// Type coercion is never lossy. It just fails with an assertion if loss would occur. So don't do that.
	// For const references (e.g. when copying) we cannot coerce the _type, just assert if it's wrong
	const int&		as_int() const { must_be(Integer); return u.i; }
	const long&		as_long() const { must_be(Long); return u.l; }
	const long long&	as_longlong() const { must_be(LongLong); return u.ll; }

	unsigned		as_uint() const { must_be(UInteger); return (unsigned)u.i; }
	unsigned long		as_ulong() const { must_be(ULong); return (unsigned long)u.l; }
	unsigned long long	as_ulonglong() const { must_be(ULongLong); return (unsigned long long)u.ll; }

	/*
	 * The number held as a signed one, in the closest signed type that holds
	 * it: an Integer, or a Long, or a LongLong. This is the read for a caller
	 * who has a number of unknown origin, and it is the answer to an unsigned
	 * value that its own signed twin is too narrow for - one beyond INT_MAX
	 * becomes a Long, and one beyond LONG_MAX a LongLong, where asking for the
	 * twin leaves nowhere to put it.
	 *
	 * A value already signed is answered as it stands, and not narrowed to the
	 * smallest type that would hold it: it is already signed, and its width is
	 * what its holder chose.
	 *
	 * It answers a value rather than a reference, because the width of a
	 * reference would be whatever the value turned out to need - so this is a
	 * read, and not a way to write into the Variant.
	 *
	 * An unsigned value no signed type can hold, which is a ULongLong beyond
	 * LLONG_MAX, is refused and reported like any other lossy coercion.
	 */
	long long	as_signed()
	{
		switch (_type)
		{
		case Integer:	return u.i;
		case Long:	return u.l;
		case LongLong:	return u.ll;

		case String:	coerce(Integer);	// asInt32 answers an int32
				return u.i;

		case UInteger:	// FALL THROUGH
		case ULong:	// FALL THROUGH
		case ULongLong:
			{
				VariantType	to = fitting_signed();
				if (to == None)		// No signed type holds it
				{
					cannot_convert(LongLong);
					return u.ll;	// Where that returns: the bits, at their width
				}
				coerce(to);		// It fits, so this cannot fail
				return to == Integer ? (long long)u.i
				     : to == Long ? (long long)u.l : u.ll;
			}

		default:	break;			// Nothing numeric is held
		}
		must_be(LongLong);			// Reports and asserts: not a number
		return 0;				// Not reached
	}
	const StrVal		as_strval() const { must_be(String); return u.str; }
	const StringArray		as_string_array() const { must_be(StrArray); return u.str_arr; }
	const VariantArray	as_variant_array() const { must_be(VarArray); return u.var_arr; }
	const StrVariantMap	as_variant_map() const { must_be(StrVarMap); return u.var_map; }

	int&			as_int() { coerce(Integer); return u.i; }
	long&			as_long() { coerce(Long); return u.l; }
	long long&		as_longlong() { coerce(LongLong); return u.ll; }
	StrVal			as_strval() { coerce(String); return u.str; }
	StringArray		as_string_array() { coerce(StrArray); return u.str_arr; }
	VariantArray		as_variant_array() { coerce(VarArray); return u.var_arr; }
	StrVariantMap		as_variant_map() { coerce(StrVarMap); return u.var_map; }

	// as_json(-1) emits single-line JSON with single spaces added for readability.
	// as_json(-2) emits maximally compact JSON.
	// as_json(n) emits formatted/indented json (two spaces per level) starting with indent n.
	// A structure nested deeper than RENDER_MAX_DEPTH answers its type name, as a
	// JSON string, rather than being descended into.
	StrVal			as_json(int indent = -1) const
	{
		return as_json_at(indent, RENDER_MAX_DEPTH);
	}

	// The same, descending at most `depth` levels. Public so that a caller who
	// wants more or less of a structure than RENDER_MAX_DEPTH can say so. See
	// as_json() above
	StrVal			as_json_at(int indent, int depth) const
	{
		// A composite we were told not to descend into says what it is
		if (depth <= 0 && (_type == StrArray || _type == VarArray || _type == StrVarMap))
			return StrVal("\"<")+type_name()+">\"";	// A JSON string, so the answer stays JSON

		int		next_indent = indent;
		StrVal		sep;			// Separator string between array or map items
		StrVal		close;			// What stands before the closing bracket
		switch (indent)
		{
		case -2:	sep = ","; break;	// Tight
		case -1:	sep = ", "; break;	// Compact
		default:	next_indent = indent+1;	// Indented
				sep = StrVal(",\n")+StrVal("  ")*next_indent;
				break;
		}
		// The closing bracket stands at the *parent's* indent, which is the
		// opening's indent less one level - two characters, whatever the
		// depth. Removing next_indent levels instead took the parent's indent
		// away as well, which is only invisible at the outermost level.
		close = sep.substr(1);
		if (indent >= 0)
			close = close.shorter(2);

		switch (_type)
		{
		default:                
			return "REVISIT: Data corruption (Variant::_type)";

		case None:		// FALL THROUGH
			return "null";

		case Integer:		// FALL THROUGH
			return StrVal::fromInt32(u.i, 0);

		case Long:		
			return StrVal::fromLong(u.l, 0);

		case LongLong:		
			return StrVal::fromInt64(u.ll, 0);

		case UInteger:		return StrVal::fromUInt32((unsigned)u.i, 0);
		case ULong:		return StrVal::fromULong((unsigned long)u.l, 0);
		case ULongLong:		return StrVal::fromUInt64((unsigned long long)u.ll, 0);

		case String:
			return StrVal("\"")+StrVal(u.str).asJSON()+"\"";

		case StrArray:
			{
			StrVal		str(StrVal("[")+sep.substr(1));
			for (int i = 0; i < u.str_arr.length(); i++)
				str += (i > 0 ? sep : StrVal())
				    + Variant(u.str_arr[i]).as_json_at(next_indent, depth-1);
			return str+close+"]";
			}

		case VarArray:
			{
			StrVal		str(StrVal("[")+sep.substr(1));
			for (int i = 0; i < u.var_arr.length(); i++)
				str += (i > 0 ? sep : StrVal())
				    + u.var_arr[i].as_json_at(next_indent, depth-1);
			return str+close+"]";
			}

		case StrVarMap:
			{
			StrVal		str(StrVal("{")+sep.substr(1));
			for (auto iter = u.var_map.begin(); iter != u.var_map.end(); iter++)
			{
				str += (iter != u.var_map.begin() ? sep : StrVal())
				    + Variant((*iter).first).as_json_at(-1, depth-1)
				    + (indent==-2 ? ":" : ": ")
				    + (*iter).second.as_json_at(next_indent, depth-1);
			}
			return str+close+"}";
			}
		}
	}

	VariantArray operator <<(const Variant& n) const
	{
		VariantArray	a;
		a.append(*this);
		a.append(n);
		return a;
	}

protected:
	// Discard any value and nullify the type
	void	coerce_none()
	{
		switch (_type)
		{
		default:		break;
		case None:		// FALL THROUGH
		case Integer:		// FALL THROUGH
		case Long:		
		case LongLong:		
		case UInteger:		// FALL THROUGH
		case ULong:		// FALL THROUGH
		case ULongLong:		// FALL THROUGH
			break;		// Nothing to do

		case String:		u.str.~StrRef(); break;
		case StrArray:		u.str_arr.~StringArray(); break;
		case VarArray:		u.var_arr.~VariantArray(); break;
		case StrVarMap:		u.var_map.~StrVariantMap(); break;
		}
		_type = None;
		u.zero();
	}

	// Reading an unsigned value of this Variant, whatever word it is in: used
	// where a coercion widens, and the value rather than the bits is wanted
	unsigned		v_unsigned() const		{ return (unsigned)u.i; }
	unsigned long		v_unsigned_long() const		{ return (unsigned long)u.l; }
	unsigned long long	v_unsigned_longlong() const	{ return (unsigned long long)u.ll; }

	// The signed type sharing storage with this one, which coerces the same
	static VariantType	signed_twin(VariantType t)
	{
		switch (t)
		{
		case UInteger:		return Integer;
		case ULong:		return Long;
		case ULongLong:		return LongLong;
		default:		return t;
		}
	}

	static bool	is_number(VariantType t)	{ return t >= Integer && t <= ULongLong; }

	// The number held, written out as it stands, for a message about it
	StrVal		value_text() const
	{
		switch (_type)
		{
		case Integer:	return StrVal::fromInt32(u.i, 0);
		case Long:	return StrVal::fromLong(u.l, 0);
		case LongLong:	return StrVal::fromInt64(u.ll, 0);
		case UInteger:	return StrVal::fromUInt32((unsigned)u.i, 0);
		case ULong:	return StrVal::fromULong((unsigned long)u.l, 0);
		case ULongLong: return StrVal::fromUInt64((unsigned long long)u.ll, 0);
		default:	break;
		}
		return StrVal("<")+type_name()+">";
	}

	/*
	 * The closest signed type that holds the value held now, or None where no
	 * signed type does - which is a value of the widest width with its top bit
	 * set, and so a value that must stay unsigned. A value already signed is
	 * answered as the type it is in, that type holding it by definition.
	 */
	VariantType	fitting_signed() const
	{
		switch (_type)
		{
		case UInteger:	return u.i >= 0 ? Integer
				     : (sizeof(long) > sizeof(int) ? Long : LongLong);
		case ULong:	return u.l >= 0 ? Long
				     : (sizeof(long long) > sizeof(long) ? LongLong : None);
		case ULongLong:	return u.ll >= 0 ? LongLong : None;
		default:	return _type;
		}
	}

	/*
	 * A number that the type it was asked for cannot hold. Where assertions
	 * are on this does not return; where they are off, the value must not be
	 * lost, so the Variant is left of a type that holds it - see src/variant.cpp.
	 */
	void	cannot_convert(VariantType t);

	void	coerce(VariantType new_type)
	{
		VariantType	old_type = _type;

		if (old_type == new_type)
			return;		// Nothing to do

		// An unsigned type coerces exactly as its signed twin does.
		// To a String is the exception: the digits of 4000000000 are not what that reads as signed.
		VariantType	was = old_type;
		old_type = signed_twin(old_type);
		new_type = signed_twin(new_type);

		// printf("coercing %s to %s\n", type_names[old_type], type_names[new_type]);

		ErrNum		e;
		int32_t		i32;
		switch (new_type)
		{
		default:		// FALL THROUGH
		case None:		// FALL THROUGH
			coerce_none();
			return;

		/*
		 * The three numeric targets read the same way, so they are written the
		 * same way. An unsigned source is dealt with first and on its own: it
		 * either fits the target or it is refused, and a refusal must not reach
		 * the switch below, whose tests read the value as signed - which it is
		 * not, so `u.l != (int)u.l` calls ULONG_MAX an int.
		 *
		 * A value fits a signed type when it is no greater than that type's
		 * maximum. At the source's own width that is the sign bit, and one
		 * width up it is the same test at the wider width.
		 */
		case Integer:
			if (was != old_type)
			{			// An unsigned source
				if (was == UInteger)
				{		// The same width: the word already holds the value
					if ((unsigned)u.i > (unsigned)INT32_MAX)
						break;
				}
				else if (was == ULong)
				{
					if ((unsigned long)u.l > (unsigned long)INT32_MAX)
						break;
					u.i = (int)u.l;
				}
				else
				{
					if ((unsigned long long)u.ll > (unsigned long long)INT32_MAX)
						break;
					u.i = (int)u.ll;
				}
				_type = new_type;
				return;
			}
			switch (old_type)
			{
			case Long:	if (u.l != (int)u.l) break;	// Fail if it would truncate
					u.i = (int)u.l;
					_type = new_type;
					return;
			case LongLong:	if (u.ll != (int)u.ll) break;	// Fail if it would truncate
					u.i = (int)u.ll;
					_type = new_type;
					return;
			case String:	i32 = StrVal(u.str).asInt32(&e, 0);
					if (e)				// Some error in conversion
						break;
					u.i = i32;
					_type = new_type;
					return;
			default:		// An Integer source cannot arrive here
					break;	// Cannot coerce
			}
			break;

		case Long:
			if (was != old_type)
			{			// An unsigned source
				if (was == UInteger)
				{		// A long holds any unsigned int where it is wider
					if (sizeof(unsigned long) > sizeof(unsigned))
						u.l = (long)(unsigned)u.i;
					else if ((unsigned)u.i > (unsigned)LONG_MAX)
						break;
				}
				else if (was == ULong)
				{
					if ((unsigned long)u.l > (unsigned long)LONG_MAX)
						break;
					// The word already holds the value
				}
				else
				{
					if ((unsigned long long)u.ll > (unsigned long long)LONG_MAX)
						break;
					u.l = (long)(unsigned long long)u.ll;
				}
				_type = new_type;
				return;
			}
			switch (old_type)
			{
			case Integer:	u.l = u.i;
					_type = new_type;
					return;
			case LongLong:	if (u.ll != (long)u.ll) break;	// Fail if it would truncate
					u.l = (long)u.ll;
					_type = new_type;
					return;
			case String:	i32 = StrVal(u.str).asInt32(&e, 0);	// REVISIT: int32 only, or it fails
					if (e)				// Some error in conversion
						break;
					u.l = i32;
					_type = new_type;
					return;
			default:		// A Long source cannot arrive here
					break;	// Cannot coerce
			}
			break;

		case LongLong:
			if (was != old_type)
			{			// An unsigned source
				if (was == UInteger)
				{		// A long long is wider than an int on every target
					u.ll = (long long)(unsigned)u.i;
				}
				else if (was == ULong)
				{
					if (sizeof(unsigned long long) > sizeof(unsigned long))
						u.ll = (long long)(unsigned long)u.l;
					else if ((unsigned long)u.l > (unsigned long)LLONG_MAX)
						break;	// No wider than the value needs
				}
				else
				{
					if ((unsigned long long)u.ll > (unsigned long long)LLONG_MAX)
						break;
					// The word already holds the value
				}
				_type = new_type;
				return;
			}
			switch (old_type)
			{
			case Integer:	u.ll = u.i;	// The long long word, not the long one
					_type = new_type;
					return;
			case Long:	u.ll = u.l;
					_type = new_type;
					return;
			case String:	i32 = StrVal(u.str).asInt32(&e, 0);	// REVISIT: int32 only, or it fails
					if (e)				// Some error in conversion
						break;
					u.ll = i32;
					_type = new_type;
					return;
			default:		// A LongLong source cannot arrive here
					break;	// Cannot coerce
			}
			break;

		case String:
			switch (was)
			{			// An unsigned value is rendered unsigned
			case UInteger:	*this = StrVal::fromUInt32((unsigned)u.i, 0);
					return;
			case ULong:	*this = StrVal::fromULong((unsigned long)u.l, 0);
					return;
			case ULongLong:	*this = StrVal::fromUInt64((unsigned long long)u.ll, 0);
					return;
			default:	break;	// Every other type is rendered below
			}
			switch (old_type)
			{
			case Integer:	*this = StrVal::fromInt32(u.i, 0);
					return;
			case Long:	*this = StrVal::fromLong(u.l, 0);
					return;
			case LongLong:	*this = StrVal::fromInt64(u.ll, 0);
					return;
			case String:	return; // Already handled
			case None:		// FALL THROUGH
			case StrArray:		// FALL THROUGH
			case VarArray:		// FALL THROUGH
			case StrVarMap:		// FALL THROUGH
			default:		// The unsigned types were mapped to their twins
					break;	// Cannot coerce
			}
			break;

		case StrArray:		break;	// REVISIT: No coercion implemented
		case VarArray:		break;	// REVISIT: No coercion implemented
		case StrVarMap:		break;	// REVISIT: No coercion implemented
		}

		/*
		 * Every refusal arrives here. A number that the target type cannot
		 * hold is a different complaint from two types that do not convert at
		 * all, and it names the value, which the second cannot.
		 */
		if (is_number(was) && is_number(new_type))
		{
			cannot_convert(new_type);
			return;
		}
		must_be(new_type);		// Report impossible coercion
	}

	// Type assertion. Reported as a message rather than printed, since the
	// default text is in the message set, so a translation can carry it and an
	// enclosing error handler can deal with it. Defined in src/variant.cpp,
	// where Error() is at hand.
	void	must_be(VariantType t) const;

	VariantType		_type;
	union u
	{
		int		i;
		long		l;
		long long	ll;
		StrRef		str;
		StringArray	str_arr;
		VariantArray	var_arr;
		StrVariantMap	var_map;

		u()		{ zero(); }
		~u()		{}	// Destruction happens outside here
		void zero()	{ memset((void*)this, 0, sizeof(*this)); }
	} u;
};

inline VariantArray operator<<(const char* cp, const Variant& v)
{
	return Variant(StrVal(cp)) << v;
}

inline VariantArray operator<<(StrVal s, const Variant& v)
{
	return Variant(s) << v;
}

#include	<strformat.h>

#endif // VARIANT_H
