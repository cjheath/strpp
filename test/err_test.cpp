/*
 * Test code for errors and error numbers
 *
 * A sketch of the two halves of a generated message set, and of what calling
 * one and reading the result back looks like. See doc/errbuf.md.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */
#include	<error.h>
#include	<errbuf.h>
#include	<cstdio>

/*
 * What the catalog compiler puts into a generated public header,
 * <Module>_err.h: a number per message, with its default text in a comment.
 */
#define	ADLERR_SET		1024
#define	ERRNUM_Something	ErrNum(ADLERR_SET, 20)	// Something went wrong with a thing

/*
 * ...and what its companion <Module>_msg.h emits, one reporting function per
 * message, gathering the parameters its default text calls for.
 */
static ThreadLocal<VariantArray>	scratch;

inline ErrNum
ErrorADL_Something(StrVal thing)
{
	VariantArray&	params = *scratch.get();
	params.evacuate();				// Empty, keeping the storage
	params.append(Variant(thing));
	return Error(ERRNUM_Something, "Something went wrong with a thing", params);
}

ErrNum
fail()
{
	return ErrorADL_Something("the thing");
}

int
main(int argc, const char** argv)
{
	ErrNum	e = fail();

	if (e == ERRNUM_Something)
		printf("Integer comparison succeeded\n");
	if (e.is_failure())
		printf("We're returning a fault\n");

	switch (e)
	{
	case 0:
		printf("No error\n");
		break;

	case ERRNUM_Something:
		{
			/*
			 * What a display does: read the message, use it, let it go,
			 * then retire it. Formatting is not designed yet, so this
			 * only shows what the buffer holds.
			 */
			ErrBuf*		buf = error_buffer().get();
			{
				ErrBuf::Message	m = buf->message(0);
				printf("It happened: \"%s\"", m.default_text);
				for (unsigned i = 0; i < m.parameters.length(); i++)
					printf(" [%u] %s", i, m.parameters[i].as_strval().asUTF8());
				printf("\n");
			}
			buf->clear();	// Only once the message has been let go
		}
		break;

	default:
		printf("Something else: 0x%04X\n", (int32_t)e);
		break;
	}
	return 0;
}
