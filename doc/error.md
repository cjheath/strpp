## Error numbers: the ErrNum type

`#include	<error.h>`

An ErrNum is a 32-bit value naming a message: a set number, and a number
within that set. It subsumes both errno and Microsoft's HRESULT, so one
value carries a failure from anywhere - ours, the C library's, or Windows'
without the reader needing to know which.

ErrNum is a class rather than a bare integer, so the tests over it are
methods:

	ErrNum	e = ...;
	if (e.is_failure())		// Something went wrong
		...
	if (e.is_info())		// Worth knowing, but not a failure
		...

Its constexpr cast to int32_t allows the values in a switch, which is how a
caller handles the errors it knows and passes on the rest:

	ErrNum e = DoSomeWork();
	switch (e)
	{
	case 0:
		// That seemed to go ok
		break;
	case SubsysErrorOfSomeTypeNum:	// A generated name for a set's message
		// Handle the error
		Complain(e);
		return;
	default:
		return e;		// Not ours to handle
	}

Set and message numbers are generated from a message set description: a
set's messages become `#define`s in a generated header, `<Module>_err.h`,
one per message with the default text in a comment, so that code has names
rather than numbers. There are 1024 messages to a set, and set numbers run
to 262143.

A number, once used, is never re-used, so a number written into a log, a
manual or a customer's report keeps its meaning over the life of the
software.

`is_failure` and `is_info` are the intended API but their names are not yet
settled.

Reporting an error, and reading back what has been reported, is the error
buffer's job: see [errbuf.md](errbuf.md). The `Error` function declared
there is what the generated reporting functions call.
