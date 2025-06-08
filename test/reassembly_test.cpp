/*
 * Unicode Strings
 * Test the re-assembly of contiguous slices
 *
 * (c) Copyright Clifford Heath 2022. See LICENSE file for usage rights.
 */
#include        <cstdio>

#define		protected	public
#define		private		public

#include        <strval.h>

int main()
{
	StrVal  hello_world("Hello, world");
	StrVal	hello = hello_world.substr(0, 5);
	StrVal	comma = hello_world.substr(5, 2);
	StrVal	world = hello_world.substr(7, 5);
	StrVal	reassembled = hello + comma;
	reassembled += world;

	fprintf(stdout, "hello_world\t%p[%d, %d]\n", (StrBody*)hello_world.body, hello_world.offset, hello_world.num_chars);
	fprintf(stdout, "hello\t\t%p[%d, %d]\n", (StrBody*)hello.body, hello.offset, hello.num_chars);
	fprintf(stdout, "comma\t\t%p[%d, %d]\n", (StrBody*)comma.body, comma.offset, comma.num_chars);
	fprintf(stdout, "world\t\t%p[%d, %d]\n", (StrBody*)world.body, world.offset, world.num_chars);
	fprintf(stdout, "reassembled\t%p[%d, %d]\n", (StrBody*)reassembled.body, reassembled.offset, reassembled.num_chars);
}
