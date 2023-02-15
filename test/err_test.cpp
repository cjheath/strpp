/*
 * Test code for errors and error numbers
 */
#include	"error.h"
#include	<stdio.h>

/* Example of what the error catalog compiler might put into a generated include file */
#define	ERRNUM_Something	ERRNUM(2,20)
#define	ERRPARMS_Something	ERRNUM_Something, "Bugger: %s"	// No parentheses!
#define	ERR_Something(...)	Error(ERRNUM_Something, "Bugger: %s", ##__VA_ARGS__)

Error
fail()
{
	Error	e;
	// return Error();
	// return 0;
	// e = Error(ERRPARMS_Something, "boo!");
	// e = ERR_Something();
	e = ERR_Something("boo!");
	if (e)
		printf("We're returning a fault\n");
	return e;
}

int
main(int argc, const char** argv)
{
	ErrNum	s(ERRNUM_Something);

	Error	e = fail();
	if (e == ERRNUM_Something)
		printf("Integer comparison succeeded\n");
	switch (e)
	{
	case 0:
		printf("No error\n");
		break;

	case ERRNUM_Something:
		printf("It happened: ");
		printf(e.default_text(), (const char*)e.parameters());
		printf("\n");
		break;

	default:
		printf("Something else: 0x%04X\n", (int32_t)e);
		break;

	}
	return 0;
}
