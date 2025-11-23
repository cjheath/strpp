/*
 * ADL Parser, with DebugSink to show progress
 */
#include	<cstdio>
#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>

#include	<array.h>
#include	<strval.h>
#include	<adl.h>

struct	ADLPathName
{
	ADLPathName()
			{ clear(); }

	int		ascent;
	StringArray	path;
	StrVal		sep;	// Next separator to use while building ("", " " or ".")

	void		clear()
			{ ascent = 0; path.clear(); sep = ""; }
	void		consume(ADLPathName& target)
			{ target = *this; clear(); }
};

struct	ADLFrame
{
	enum ADLValueType {
		None,
		Number,
		String,
		Reference,
		Object,
		Pegexp,
		Match
	};

	ADLFrame()
	: obj_array(false)
	, value_type(None)
	{}

	// path name and ascent for current object:
	ADLPathName	object_path;

	// path name and ascent for current object's supertype:
	ADLPathName	supertype_path;

	bool		obj_array;	// This object accepts an array value
	ADLValueType	value_type;	// Type of value assigned
	StrVal		value;		// Value assigned
};

class	ADLStack
: public Array<ADLFrame>
{
};

/*
 * If Syntax lookup is required, you need to save enough data to implement it.
 */
class ADLDebugSink
{
	// Current path name being built (with ascent - outer scope levels to rise before searching)
	ADLPathName	current_path;

	ADLStack	stack;

	// Access the current ADL Frame:
	ADLFrame&	frame() { return stack.last_mut(); }

	// Read/write access to members of the current frame:
	ADLPathName&	object_path() { return frame().object_path; }
	ADLPathName&	supertype_path() { return frame().supertype_path; }
	bool&		obj_array() { return frame().obj_array; }
	ADLFrame::ADLValueType&	value_type() { return frame().value_type; }
	StrVal&		value() { return frame().value; }

public:
	using	Source = ADLSourcePtr;

	ADLDebugSink()
	{
	}

	void	error(const char* why, const char* what, const Source& where)
	{
		printf("At line %d:%d, %s MISSING %s: ", where.line_number(), where.column(), why, what);
		where.print_ahead();
	}

	void	definition_starts()			// A declaration just started
	{
		stack.push(ADLFrame());
	}

	void	definition_ends()
	{
		printf("Definition Ends\n");
		(void)stack.pull();
		current_path.clear();
	}

	void	ascend()				// Go up one scope level to look for a name
	{
		current_path.ascent++;
	}

	void	name(Source start, Source end)		// A name exists between start and end
	{
		StrVal	n(start.peek(), (int)(end-start));

		if (current_path.sep != " ")			// "" or ".", start new name in pathname
			current_path.path.push(n);
		else if (current_path.sep != ".")
			current_path.path.push(current_path.path.pull() + current_path.sep + n);	// Append the partial name
		current_path.sep = " ";
	}

	void	descend()				// Go down one level from the last name
	{
		current_path.sep = ".";	// Start new multi-word name
	}

	void	pathname(bool ok)			// The sequence *ascend name *(descend name) is complete
	{
		if (!ok)
			current_path.clear();
	}

	void	object_name()				// The last name was for a new object
	{
		// Save the path:
		current_path.consume(object_path());
		printf("Object PathName (%d names): .%d '%s'\n", object_path().path.length(), object_path().ascent, object_path().path.join(".").asUTF8());
	}

	void	supertype()				// Last pathname was a supertype
	{
		current_path.consume(supertype_path());

		printf("Supertype pathname (%d names): .%d '%s'\n", supertype_path().path.length(), supertype_path().ascent, supertype_path().path.join(".").asUTF8());
	}

	void	reference_type(bool is_multi)		// Last pathname was a reference
	{
		printf("Reference (%s) to .%d '%s'\n",
			is_multi ? "multiple" : "single",
			current_path.ascent, current_path.path.join(".").asUTF8());
		ADLPathName	reference_path;
		current_path.consume(reference_path);
	}

	void	reference_done(bool ok)			// Reference completed
	{
		// printf("Reference finished\n");
	}

	void	alias()					// Last pathname is an alias
	{
		// printf("Alias finished\n");
		ADLPathName	alias_path;
		current_path.consume(alias_path);
	}

	void	block_start()				// enter the block given by the pathname and supertype
	{
		printf("Enter block\n");
	}

	void	block_end()				// exit the block given by the pathname and supertype
	{
		printf("Exit block\n");
	}

	void	is_array()				// This definition is an array
	{
		obj_array() = true;
		printf("Object was an array\n");
	}

	void	assignment(bool is_final)		// The value(s) are assigned to the current definition
	{
		printf("%s Assignment\n", is_final ? "Final" : "Tentative");
	}

	void	string_literal(Source start, Source end)	// Contents of a string between start and end
	{
		StrVal	string(start.peek(), (int)(end-start));
		value_type() = ADLFrame::String;
		value() = string;
		printf("String value: '%s'\n", string.asUTF8());
	}

	void	numeric_literal(Source start, Source end)	// Contents of a number between start and end
	{
		StrVal	number(start.peek(), (int)(end-start));
		value_type() = ADLFrame::Number;
		value() = number;
		printf("Numeric value: %s\n", number.asUTF8());
	}

	void	pegexp_match(Source start, Source end)	// Contents of a matched value between start and end
	{
		StrVal	match(start.peek(), (int)(end-start));
		value_type() = ADLFrame::Match;
		value() = match;
	}

	void	object_literal()			// An object_literal (supertype, block, assignment) was pushed
	{
		value_type() = ADLFrame::Object;
		value() = "<object>";
		printf("Object was an object_literal\n");
	}

	void	reference_literal()			// The last pathname is a value to assign to a reference variable
	{
		value_type() = ADLFrame::Reference;
		value() = current_path.path.join(".");
		printf("Reference value .%d '%s'\n",
			current_path.ascent, value().asUTF8());
		ADLPathName	reference_path;
		current_path.consume(reference_path);
	}

	void	pegexp_literal(Source start, Source end)	// Contents of a pegexp between start and end
	{
		StrVal	pegexp(start.peek(), (int)(end-start));
		value_type() = ADLFrame::Pegexp;
		value() = pegexp;
		printf("Pegexp value: /%s/\n", pegexp.asUTF8());
	}

	Source	lookup_syntax(Source type)		// Return Source of a Pegexp string to use in matching
	{ return Source(""); }
};

char* slurp_file(const char* filename, off_t* size_p)
{
	// Open the file and get its size
	int		fd;
	struct	stat	stat;
	char*		text;
	if ((fd = open(filename, O_RDONLY)) < 0		// Can't open
	 || fstat(fd, &stat) < 0			// Can't fstat
	 || (stat.st_mode&S_IFMT) != S_IFREG		// Not a regular file
	 || (text = new char[stat.st_size+1]) == 0	// Can't get memory
	 || read(fd, text, stat.st_size) < stat.st_size)	// Can't read entire file
	{
		perror(filename);
		exit(1);
	}
	if (size_p)
		*size_p = stat.st_size;
	text[stat.st_size] = '\0';

	return text;
}

int main(int argc, const char** argv)
{
	const char*		filename = argv[1];
	off_t			file_size;
	char*			text = slurp_file(filename, &file_size);
	ADLParser<ADLDebugSink>	adl;
	ADLSourcePtr		source(text);

	bool			ok = adl.parse(source);
	off_t			bytes_parsed = source - text;

	printf("%s, parsed %lld of %lld bytes\n", ok ? "Success" : "Failed", bytes_parsed, file_size);
	exit(ok ? 0 : 1);
}
