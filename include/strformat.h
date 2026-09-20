#if !defined(STRFORMAT_H)
#define STRFORMAT_H
/*
 * Substituting parameters into a text: StrValI::format, and the format
 * language its markers use.
 *
 * This is included at the foot of variant.h, because rendering a parameter
 * needs Variant to be complete and variant.h is what includes strval.h, not
 * the other way about. The declaration of format() is on StrValI itself.
 *
 * A text names each parameter by its position: {1} is the first of `args`,
 * {2} the second, and so on, because a translation may use them in another
 * order or leave one out. A marker is {, the position, and }, and after a
 * colon it may say how the parameter is to be rendered:
 *
 *	{1}		as it stands
 *	{1:x}		as hexadecimal
 *	{1:8}		in at least eight characters, spaces in front
 *	{1:08}		...zeroes in front instead
 *	{1:2<8}		between two and eight characters, cut if longer
 *	{1:<40...}	at most forty: thirty-seven characters and three dots
 *	{1:<40…}	the same with an ellipsis: thirty-nine characters and …
 *
 * The representation comes first, one character: b binary, o octal, d decimal,
 * x and X hexadecimal in lower and upper case. A base carries no prefix of its
 * own - a text that wants 0x writes it itself, "0x{1:X}", which leaves the
 * prefix where a translator can put it. A non-decimal base renders the bit
 * pattern of the value at the width of its own type, which is what a base is
 * for: an int of -1 is eight f's, with no sign. Decimal renders the sign.
 *
 * Then the minimum, in characters, which is padded for in front with spaces
 * unless its first digit is a 0, which pads with zeroes instead. Then, after a
 * <, the maximum: text longer than that is cut, and what follows the maximum,
 * being three dots or an ellipsis, stands in place of the end that was lost. So
 * {1:<40...} keeps thirty-seven characters and adds three dots, forty in all;
 * a single …, counting as one character, leaves one more. The minimum is
 * applied after the cut, so {1:8<6...} gives two spaces and then abc... The
 * minimum, the maximum and the whole specification are each optional.
 *
 * Sizing counts characters, not terminal columns: a wide glyph is one.
 *
 * A parameter that is an array or a map is expanded rather than named: an array
 * in brackets, its elements rendered the same way, and a map as JSON, which is
 * what a map is for. `depth`, which format() takes and defaults to two levels,
 * bounds how far the arrays go - a composite at the limit answers its type name
 * in angle brackets instead, as does a type with no rendering at all - so that
 * a structure deep enough to run away with the stack cannot, whatever a program
 * passes in. A map goes by way of as_json(), which has no depth of its own and
 * descends as far as the map does. The limit belongs to the call rather than to
 * the text, since it is the programmer who knows what is being passed and a
 * translator who does not.
 *
 * A brace that is meant literally is doubled, as it is in Python and .NET: {{
 * is a {, and }} is a }. Nothing else is escaped, so a text may carry regular
 * expressions, paths and backslashes as they stand.
 *
 * A specification is applied as far as it is understood, and whatever else the
 * marker holds is passed over: a text may be translated, and a translation may
 * come from a catalog that nothing can have checked before the program runs, so
 * a part of a specification that means nothing here leaves the rest of it
 * working rather than striking the message out. A marker naming no parameter of
 * the array is left exactly as it stands, so a text that does not match its
 * parameters shows the reader the marker it could not fill rather than losing
 * the text around it.
 *
 * The specification is parsed into StrFormatSpec, which is broken out so that
 * a type with a formatting of its own - a date or a time, say - can take the
 * same parts and read the rest of the marker itself. strval_render answers the
 * representation, and strval_push_parameter frames it as the specification
 * asks, writing the pad and the tail straight into the answer rather than into
 * strings of their own.
 *
 * The walk hands its pieces to a push, so that a stream can take them as they
 * come rather than a whole string being built first: the counting push sizes
 * the answer, and the writing push fills it, which is why nothing in the
 * formatting path is rendered twice.
 *
 * None of this uses printf: a library that formats its own text must not need
 * it, and the digits of an integer are simple to place.
 */

// A marker's specification, as the text writes it
struct StrFormatSpec
{
	// What stands in place of the end that a cut took off. A tail is only ever
	// one of the two ways of writing an ellipsis, so it is a value here rather
	// than the text it stands for.
	enum Tail : char
	{
		NoTail = 0,		// The value is simply cut short
		Dots,			// ... after the characters that were kept
		Ellipsis		// … after them: one character, three bytes
	};

	char		repr;		// The representation: b, o, d, x or X; 0 is decimal
	bool		zero_pad;	// Pad with zeroes rather than spaces
	Tail		tail;		// What stands in for an end that was cut
	uint16_t	min;		// Least characters, padded for in front; 0 for none
	uint16_t	max;		// Most characters, cut beyond; 0 for no limit

	StrFormatSpec() : repr(0), zero_pad(false), tail(NoTail), min(0), max(0) {}

	int		tailLength() const	// The characters the tail stands for
			{ return tail == Ellipsis ? 1 : tail == Dots ? 3 : 0; }
};

/*
 * The digits of an integer are placed by strval_repr_int in strval.h, since
 * the JSON emitter in variant.h needs them too.
 */

/*
 * The name of a type that has no rendering here, or of a composite that the
 * depth limit stops us expanding: what answers when nothing else can. Built
 * where it is used rather than in front of every parameter, since most
 * parameters answer something else and would only throw it away.
 */
inline StrVal
strval_type_name(Variant v)
{
	return StrVal("<")+v.type_name()+">";
}

/*
 * One parameter in the representation asked for, and nothing else: no minimum,
 * no maximum, no tail. What the specification asks for around it is written
 * straight into the answer by strval_push_parameter below, so that the padding
 * of a value is never a string of its own - the room for all of it was counted
 * before, which is what the counting pass is for.
 *
 * A composite is expanded a level at a time, down to `depth`, and a parameter
 * of a type that has no rendering here - or a composite at that limit - answers
 * its type name in angle brackets, since a text that is being built is no place
 * to stop the program.
 */
inline StrVal
strval_render(Variant v, const StrFormatSpec& spec, int depth)
{
	StrVal	text;

	switch (v.type())
	{
	case Variant::None:	text = strval_type_name(v); break;	// Nothing was set, and it says so

	// The width of the value's own type, so that a base that renders the bit
	// pattern renders it at that width
	case Variant::Integer:	text = strval_repr_int(v.as_int(), spec.repr, (int)sizeof(int)*8); break;
	case Variant::Long:	text = strval_repr_int(v.as_long(), spec.repr, (int)sizeof(long)*8); break;
	case Variant::LongLong:	text = strval_repr_int(v.as_longlong(), spec.repr, (int)sizeof(long long)*8); break;
	case Variant::String:	text = v.as_strval(); break;

	case Variant::StrArray:				// Strings, in brackets
		text = depth > 0 ? StrVal("[")+v.as_string_array().join(", ")+"]"
				 : strval_type_name(v);
		break;

	case Variant::VarArray:				// Their elements, one level down
		if (depth > 0)
		{
			VariantArray	items = v.as_variant_array();
			text = "[";
			for (int i = 0; i < items.length(); i++)
				text += (i > 0 ? StrVal(", ") : StrVal())
					+ strval_render(items[i], StrFormatSpec(), depth-1);
			text += "]";
		}
		else
			text = strval_type_name(v);
		break;

	case Variant::StrVarMap:			// A map says itself in JSON
		text = depth > 0 ? v.as_json() : strval_type_name(v);
		break;

	default:	text = strval_type_name(v); break;	// A type with no rendering here
	}

	return text;
}

/*
 * One parameter written into the answer, framed as its specification asks: cut
 * to the maximum with the tail standing in for the end that was lost, and
 * padded to the minimum. Every piece goes straight into the push it is given,
 * so that nothing here is materialised as a string of its own - the room for
 * all of it was counted before, which is what the counting pass is for.
 *
 * Zeros are a leading pad, so when a value is zero-padded past its sign, the
 * sign is written first and stays outermost.
 */
template<typename Push>
void	strval_push_parameter(Push& out, Variant v, const StrFormatSpec& spec, int depth)
{
	StrVal	text = strval_render(v, spec, depth);
	int	tail_chars = 0;
	bool	cut = spec.max > 0 && (int)text.length() > spec.max;

	if (cut)
	{					// Keep what the tail leaves of the maximum
		int	keep = (int)spec.max - spec.tailLength();
		if (keep < 0)
			keep = 0;
		if (keep > (int)text.length())
			keep = (int)text.length();

		tail_chars = spec.tailLength();
		if (tail_chars > (int)spec.max - keep)
			tail_chars = (int)spec.max - keep;	// A tail longer than the maximum: only the tail
		text = text.head(keep);
	}

	bool	sign_first = spec.zero_pad && text.length() > 0
				&& (text[0] == '-' || text[0] == '+');
	int	written = (int)text.length() + tail_chars;
	int	pad = spec.min > written ? spec.min - written : 0;
	UCS4	pad_char = spec.zero_pad ? '0' : ' ';

	if (pad > 0 && sign_first)
		out.push((UCS4)text[0]);	// The sign stays outermost
	for (int i = 0; i < pad; i++)
		out.push(pad_char);

	out.push(sign_first ? text.substr(1) : text);
	if (spec.tail == StrFormatSpec::Ellipsis)
	{
		if (tail_chars > 0)
			out.push((UCS4)0x2026);
	}
	else
		for (int i = 0; i < tail_chars; i++)
			out.push((UCS4)'.');
}

/*
 * How much room a parameter's rendering will need, without rendering it: the
 * value is not looked at beyond its type, so an integer is charged the most its
 * representation could take - sixty-five characters of binary, twenty-one of
 * decimal, sign included. That is room wasted when the value is small, and a
 * reallocation saved when it is not, which is the cheaper way round.
 */
inline StrValIndex
strval_size(Variant v, const StrFormatSpec& spec)
{
	StrValIndex	room = 16;		// A composite: whatever length it comes to

	switch (v.type())
	{
	case Variant::Integer:
	case Variant::Long:
	case Variant::LongLong:
		switch (spec.repr)
		{
		case 'b':		room = 65; break;	// Sixty-four bits and a sign
		case 'o':		room = 23; break;
		case 'x': case 'X':	room = 17; break;
		default:		room = 21; break;
		}
		break;
	case Variant::String:	room = v.as_strval().length(); break;
	default:		break;		// The guess above: at worst, a reallocation
	}

	if (spec.max > 0 && (int)room > spec.max)
		room = spec.max;		// Cutting, and the tail, fit inside it
	if ((int)room < spec.min)
		room = spec.min;		// And the pad comes after the cut, not before

	return room;
}

/*
 * Parse a specification, from just after its colon, and answer where it ended:
 * the first character that cannot belong to a specification, which is the '}'
 * that closes the marker when the whole of it was understood. What was not
 * understood is passed over by the walk, and the marker is closed at its own }.
 */
inline StrValIndex
strval_parse_format(StrVal f, StrValIndex i, StrFormatSpec& spec)
{
	StrValIndex	m = f.length();

	switch (i < m ? f[i] : 0)
	{
	case 'b': case 'o': case 'd': case 'x': case 'X':
		spec.repr = (char)f[i++];
		break;
	}

	spec.zero_pad = i+1 < m && f[i] == '0' && f[i+1] >= '0' && f[i+1] <= '9';
	if (spec.zero_pad)
		i++;				// The 0 is a flag, not a digit of the minimum
	while (i < m && f[i] >= '0' && f[i] <= '9')
		spec.min = spec.min*10 + (int)(f[i++] - '0');

	if (i < m && f[i] == '<')
	{
		i++;
		while (i < m && f[i] >= '0' && f[i] <= '9')
			spec.max = spec.max*10 + (int)(f[i++] - '0');

		// What follows the maximum is what the text shows in place of the end
		// that was cut: three dots, or an ellipsis. Anything else is left
		// where it stands, and the walk passes over it.
		if (i+2 < m && f[i] == '.' && f[i+1] == '.' && f[i+2] == '.')
		{
			spec.tail = StrFormatSpec::Dots;
			i += 3;
		}
		else if (i < m && f[i] == 0x2026)		// …, one character
		{
			spec.tail = StrFormatSpec::Ellipsis;
			i++;
		}
	}

	return i;
}

/*
 * Where the walked pieces go: anything that can take a run of text, and a
 * parameter together with the specification for rendering it. The push decides
 * what a parameter costs and when to render it, so the walk renders nothing.
 *
 * StrValPush appends to a string, which is what format() below answers; a
 * stream will answer with a push that writes each piece as it arrives, so that
 * a text can be written out without a complete formatted string ever being
 * built. The walk does not know which it is talking to, and nothing else here
 * does either.
 */
struct StrValPush
{
	StrVal&	out;

	StrValPush(StrVal& to) : out(to) {}

	void	push(const StrVal& text)	{ out += text; }
	void	push(UCS4 ch)			{ out += ch; }
	void	push(Variant v, const StrFormatSpec& spec, int depth)
		{ strval_push_parameter(*this, v, spec, depth); }
};

/*
 * A push that only measures, so that the answer can be built with room for all
 * of it and no reallocation: format() walks once into one of these and once
 * into a StrValPush. Measuring a parameter costs its type, not its value, so a
 * value is rendered once in all. What this counts for a parameter has to be
 * what the writing push writes, or the answer is built twice over. A stream
 * will walk it once, and measure nothing, since it is building nothing.
 */
struct StrValMeasure
{
	StrValIndex	room;

	StrValMeasure() : room(0) {}

	void	push(const StrVal& text)	{ room += text.length(); }
	void	push(UCS4)			{ room += 1; }
	void	push(Variant v, const StrFormatSpec& spec, int depth)
		{ room += strval_size(v, spec); }
};

/*
 * The walk that both the string form and, later, a stream form need: emit the
 * text a piece at a time, with each marker replaced by the parameter it names.
 */
template<typename Push>
void	strval_format_into(StrVal f, VariantArray args, Push& out, int depth)
{
	StrValIndex	run = 0;		// Text not yet emitted
	StrValIndex	m = f.length();

	for (StrValIndex i = 0; i < m; i++)
	{
		if ((f[i] == '{' && i+1 < m && f[i+1] == '{')
		 || (f[i] == '}' && i+1 < m && f[i+1] == '}'))
		{				// A doubled brace is that brace, literally
			out.push(f.substr(run, (int)(i-run)));
			out.push((UCS4)(f[i] == '{' ? '{' : '}'));
			run = i+2;
			i++;			// The loop's i++ steps past the second brace
			continue;
		}
		if (f[i] != '{')
			continue;

		StrValIndex	j = i+1;	// A marker is a run of digits,
		int		n = 0;
		while (j < m && f[j] >= '0' && f[j] <= '9')
			n = n*10 + (int)(f[j++] - '0');
		if (j == i+1)
			continue;		// A lone brace, or something else in braces

		StrFormatSpec	spec;
		if (j < m && f[j] == ':')
		{	// The specification is applied as far as it is understood, and
			// whatever else the marker holds is passed over: a text may come
			// from a message catalog that nothing can have checked, and it is
			// better for it to say what it can than to stand there unsaid.
			j = strval_parse_format(f, j+1, spec);
			while (j < m && f[j] != '}' && f[j] != '{')
				j++;		// Not a brace of its own: to the one that closes
		}
		if (j >= m || f[j] != '}')
			continue;		// No closing brace: not a marker

		out.push(f.substr(run, (int)(i-run)));
		if (n >= 1 && n <= args.length())
			out.push(args[n-1], spec, depth);
		else				// No such parameter: leave the marker as it stands
			out.push(f.substr(i, (int)(j+1-i)));
		run = j+1;			// Beyond the '}'
		i = j;				// The loop's i++ steps to the same place
	}

	out.push(f.substr(run, (int)(m-run)));
}

template<typename Index>
StrVal	StrValI<Index>::format(StrVal f, VariantArray args, int depth)
{
	// Walk it once to learn how much room the answer needs, then once more to
	// write it: two passes over the text, but each parameter is rendered once,
	// and the answer is built with one allocation rather than one per piece.
	// A stream, which builds nothing, will walk it only the second way.
	StrValMeasure	measure;
	strval_format_into(f, args, measure, depth);

	StrVal		result("", 0, measure.room+1);	// +1 counts the terminator
	StrValPush	out(result);
	strval_format_into(f, args, out, depth);
	return result;
}

#endif // STRFORMAT_H
