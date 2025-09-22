/*
 * Test program using the Px PEG parser to parse Fig files
 *
 * Copyright 2025 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<fig_parser.h>
#include	<fig_parser.cpp>

#include	<char_encoding.h>
#include	<refcount.h>
#include	<strval.h>
#include	<variant.h>

#include	<peg.h>
#include	<peg_ast.h>
#include	<utf8_ptr.h>

#include	<cstdio>
#include	<cctype>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<fcntl.h>

#include	"memory_monitor.h"

typedef	Peg<PegMemorySource, PegMatch, PegContext>	FigParser;

void usage()
{
	fprintf(stderr, "Usage: peg_test peg.px\n");
	exit(1);
}

char* slurp_file(const char* filename, off_t* size_p)
{
	// Open the file and get its size
	int		fd;
	struct	stat	stat;
	char*		px;
	if ((fd = open(filename, O_RDONLY)) < 0		// Can't open
	 || fstat(fd, &stat) < 0			// Can't fstat
	 || (stat.st_mode&S_IFMT) != S_IFREG		// Not a regular file
	 || (px = new char[stat.st_size+1]) == 0	// Can't get memory
	 || read(fd, px, stat.st_size) < stat.st_size)	// Can't read entire file
	{
		perror(filename);
		usage();
	}
	if (size_p)
		*size_p = stat.st_size;
	px[stat.st_size] = '\0';

	return px;
}

FigParser::Match
parse_file(char* text)
{
	FigParser		peg;

	FigParser::Source	source(text);
	return peg.parse(source);
}

int
parse_and_report(const char* filename)
{
	off_t	file_size;
	char*	text = slurp_file(filename, &file_size);

	FigParser::Match match = parse_file(text);

	int		bytes_parsed;

	bytes_parsed = match.is_failure() ? 0 : match.furthermost_success.peek() - text;

	if (bytes_parsed < file_size)
	{
		printf("Parse %s at line %lld column %lld (byte %lld of %d). Next tokens anticipated were:\n",
			bytes_parsed > 0 ? "finished early" : "failed",
			match.furthermost_success.current_line(),
			match.furthermost_success.current_column(),
			match.furthermost_success.current_byte(),
			(int)file_size
		);

		for (int i = 0; i < match.failures.length(); i++)
		{
			PegFailure	f = match.failures[i];
			printf("\t%.*s\n", f.atom_len, f.atom);
		}
	}
	else
		printf("Parsed %d bytes of %d\n", bytes_parsed, (int)file_size);

	if (bytes_parsed > 0)
		printf("Parse Tree:\n%s\n", match.var.as_json(0).asUTF8());

	delete text;

	return bytes_parsed == file_size ? 0 : 1;
}

int
main(int argc, const char** argv)
{
	if (argc < 2)
		usage();

	start_recording_allocations();

	int code = parse_and_report(argv[1]);

	if (allocation_growth_count() > 0)	// No allocation should remain unfreed
		report_allocation_growth();

	return code;
}
