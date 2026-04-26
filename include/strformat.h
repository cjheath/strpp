#if !defined(STRFORMAT_H)
#define STRFORMAT_H

template<typename Index>
StrVal	StrValI<Index>::format(StrVal f, VariantArray args)
{
	StrVal	result;		// Output string
	int	size, prec;	// Size and precision
	bool	leading_zeroes;	// Whether to pad with leading zeroes
	int	d;		// A digit
	int	a = 0;		// Arg index
	int	numbered_arg;

	Index	m = f.length();
	for (Index i = 0; i < m; i++)
	{
		UCS4	c = f[i];
		if (c != '%' || i == m-1)
		{
			result += c;
			continue;
		}
		c = f[++i];	// Skip the %

		if (i+2 < m && (d = Digit(c, 10)) > 0 && f[i+1] == '$')
		{		// Extract the argument sequence digit
			numbered_arg = d;
			i += 2;
			c = f[++i];	// Skip ASD
		}

		// Get the size
		size = 0;
		leading_zeroes = false;
		if (c == '*')
			size = -1;
		else if ((d = Digit(c, 10)) >= 0)
		{
			if (d == 0)
				leading_zeroes = true;
			size = d;
			while (i < m && (d = Digit(c = f[++i], 10)) >= 0)
				size = size*10 + d;
		}

		// Get the precision
		prec = 0;
		if (i < m && f[i] == '.')	// REVISIT: assert(f[i] == c)
		{
			c = f[++i];
			if (c == '*')
				prec = -1;
			else if ((d = Digit(c, 10)) >= 0)
			{
				prec = d;
				while (i < m && (d = Digit(c = f[++i], 10)) >= 0)
					prec = prec*10 + d;
			}
		}

		if (size == -1)
			size = args[a++].as_int();
		if (prec == -1)
			prec = args[a++].as_int();

		// Get the conversion character
		switch (c)
		{
		case 's':
			// REVISIT: Handle size and prec
			result += args[a++].as_strval();
			break;

		case 'd':
		case 'f':
		case 'g':
			// REVISIT: Handle size and prec
			result += args[a++].as_json();
			break;

		default:
			break;
		}
	}

	return result;
}

#endif // STRFORMAT_H
