/*
 * Test program for the UTF8 pointer class.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */

#include	<stdio.h>
#include	<utf8_ptr.h>

UTF8	source[] = "Hello, world";

int
main(int argc, const char** argv)
{
	GuardedUTF8Ptr	hello(source);
	GuardedUTF8Ptr	world = hello;

	printf("--- incr/decr ---\n");
	// postincr
	UCS4	h = *hello++;
	printf("h=%c\n", (char)h);
	printf("ello, world=%s\n", static_cast<const char*>(hello));

	// Assignment
	hello = world;
	printf("hello, world=%s\n", static_cast<const char*>(hello));

	// preincr
	UCS4	e = *++hello;
	printf("e=%c\n", (char)e);
	printf("ello, world=%s\n", static_cast<const char*>(hello));

	// predecr
	h = *--hello;
	printf("h=%c\n", (char)h);
	printf("hello, world=%s\n", static_cast<const char*>(hello));

	// postdecr
	hello++;
	e = *hello--;
	printf("e=%c\n", (char)e);
	printf("Hello, world=%s\n", static_cast<const char*>(hello));

	printf("\n--- add and subtract ---\n");
	printf("hello+1=%s\n", static_cast<const char*>(hello+1));		// Add integer
	hello += 1;						// Assign-add an integer
	printf("hello+=1=%s\n", static_cast<const char*>(hello));
	hello -= -1;						// Assign-subtract negative integer with 2-byte
	printf("hello+=1-=-1=%s\n", static_cast<const char*>(hello));
	hello += -1;

	printf("(hello+=1)-1=%s\n", static_cast<const char*>(hello-1));		// Subtract an integer
	hello -= 1;
	printf("(hello+=1)-=1=%s\n", static_cast<const char*>(hello));		// Assign-subtract an integer

	printf("\n--- 2-byte ---\n");
	// Put a 2-byte character and squint at the result, incrementing, adding then backing up over it
	UTF8*	hack = source;
	UTF8Put(hack, 0xE8);

	printf("\u00E8llo, world=%s\n", static_cast<const char*>(hello));

	// Check the 2-byte
	printf("len(\u00E8) = %d\n", hello.len());
	printf("is1st(hello) = %d\n", hello.is1st());
	printf("is1st((char*)hello+1) = %s\n", (hello+1).is1st() ? "true" : "false");
	printf("is1st(hello+1) = %s\n", GuardedUTF8Ptr((static_cast<const char*>(hello))+1).is1st() ? "true" : "false");	// Construct a string pointing at the 2nd byte

	printf("\n--- 2-byte add and subtract ---\n");
	printf("hello+1=%s\n", static_cast<const char*>(hello+1));		// Add integer with 2-byte

	// assign-add integer with 2-byte:
	hello += 1;						// Assign-add integer with 2-byte
	printf("hello+=1=%s\n", static_cast<const char*>(hello));
	hello -= -1;						// Assign-add integer with 2-byte
	printf("hello+=1-=-1=%s\n", static_cast<const char*>(hello));
	hello -= 1;

	// subtract integer with 2-byte:
	printf("hello+=1-1=%s\n", static_cast<const char*>(hello+ (-1)));	// Subtract integer with 2-byte

	// assign-subtract integer with 2-byte:
	hello -= 1;						// Assign-subtract integer with 2-byte
	printf("hello+=1 +=-1 =%s\n", static_cast<const char*>(hello));
}
