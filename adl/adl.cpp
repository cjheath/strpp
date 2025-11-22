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

/*
 * If Syntax lookup is required, you need to save enough data to implement it.
 */
class ADLDebugSink
{
	int		path_ascent;
	StringArray	path;
	StrVal		path_sep;

	// A completed name (with ascent)
	int		last_ascent;
	StringArray	last_path;

	int		object_ascent;
	StringArray	object_path;

	int		supertype_ascent;
	StringArray	supertype_path;

	bool		obj_array;
	enum ValueType {
		VT_None,
		VT_Number,
		VT_String,
		VT_Reference,
		VT_Object,
		VT_Pegexp,
		VT_Match
	}		value_type;
	StrVal		value;

public:
	using	Source = ADLSource;

	void clear()
	{
		path_ascent = 0;
		path.clear();
		path_sep = "";

		last_ascent = 0;
		last_path.clear();

		object_ascent = 0;
		object_path.clear();

		supertype_ascent = 0;
		supertype_path.clear();

		obj_array = false;
		value_type = VT_None;
		value = "";
	}

	ADLDebugSink()
	{
		clear();
	}

	void	error(const char* why, const char* what, const Source& where)
	{
		printf("At line %d:%d, %s MISSING %s: ", where.line_number(), where.column(), why, what);
		where.print_ahead();
	}

	void	definition_ends()
	{
		printf("Definition Ends\n");
		clear();
	}

	void	ascend()				// Go up one scope level to look for a name
	{
		path_ascent++;
	}

	void	name(Source start, Source end)		// A name exists between start and end
	{
		StrVal	n(start.peek(), (int)(end-start));

		if (path_sep != " ")			// "" or ".", start new name in pathname
			path.push(n);
		else if (path_sep != ".")
			path.push(path.pull() + path_sep + n);	// Append the partial name
		path_sep = " ";
	}

	void	descend()				// Go down one level from the last name
	{
		path_sep = ".";	// Start new multi-word name
	}

	void	pathname(bool ok)			// The sequence *ascend name *(descend name) is complete
	{
		last_ascent = path_ascent;
		last_path = path;

		// Prepare to start a new pathname
		path_ascent = 0;
		path.clear();
		path_sep = "";
	}

	void	object_name()				// The last name was for a new object
	{
		// Save the path:
		object_ascent = last_ascent;
		object_path = last_path;
		printf("Object PathName (%d names): .%d '%s'\n", object_path.length(), object_ascent, object_path.join(".").asUTF8());
	}

	void	supertype()				// Last pathname was a supertype
	{
		supertype_ascent = last_ascent;
		supertype_path = last_path;

		printf("Supertype pathname (%d names): .%d '%s'\n", supertype_path.length(), supertype_ascent, supertype_path.join(".").asUTF8());
	}

	void	reference_type(bool is_multi)		// Last pathname was a reference
	{
		printf("Reference (%s) to .%d '%s'\n",
			is_multi ? "multiple" : "single",
			last_ascent, last_path.join(".").asUTF8());
	}

	void	reference_done(bool ok)			// Reference completed
	{
		// printf("Reference finished\n");
	}

	void	alias()					// Last pathname is an alias
	{
		// printf("Alias finished\n");
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
		obj_array = true;
		printf("Object was an array\n");
	}

	void	assignment(bool is_final)		// The value(s) are assigned to the current definition
	{
		printf("%s Assignment\n", is_final ? "Final" : "Tentative");
	}

	void	string_literal(Source start, Source end)	// Contents of a string between start and end
	{
		StrVal	string(start.peek(), (int)(end-start));
		value_type = VT_String;
		value = string;
		printf("String value: '%s'\n", string.asUTF8());
	}

	void	numeric_literal(Source start, Source end)	// Contents of a number between start and end
	{
		StrVal	number(start.peek(), (int)(end-start));
		value_type = VT_Number;
		value = number;
		printf("Numeric value: %s\n", number.asUTF8());
	}

	void	pegexp_match(Source start, Source end)	// Contents of a matched value between start and end
	{
		StrVal	match(start.peek(), (int)(end-start));
		value_type = VT_Match;
		value = match;
	}

	void	object_literal()			// An object_literal (supertype, block, assignment) was pushed
	{
		value_type = VT_Object;
		value = "<object>";
		printf("Object was an object_literal\n");
	}

	void	reference_literal()			// The last pathname is a value to assign to a reference variable
	{
		value_type = VT_Reference;
		value = last_path.join(".");
		printf("Reference value .%d '%s'\n",
			last_ascent, value.asUTF8());
	}

	void	pegexp_literal(Source start, Source end)	// Contents of a pegexp between start and end
	{
		StrVal	pegexp(start.peek(), (int)(end-start));
		value_type = VT_Pegexp;
		value = pegexp;
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
	ADLSource		source(text);

	bool			ok = adl.parse(source);
	off_t			bytes_parsed = source - text;

	printf("%s, parsed %lld of %lld bytes\n", ok ? "Success" : "Failed", bytes_parsed, file_size);
	exit(ok ? 0 : 1);
}
