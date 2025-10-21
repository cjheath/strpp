## Error and ErrNum type

`#include <error.h>`

Error is a lightweight type intended to encapsulate success or failure reason from a function.
It includes:
- a 32-bit ErrNum (which subsumes <strong>errno</strong> and <strong>HRESULT</strong>)
- a <strong>const char*</strong> to a default message text format string
- a reference-counted Variant array of typed arguments to be inserted into a format string

Error and the ErrNum class has a constexpr cast to <strong>int32_t</strong>
which allows the values to be used in switch statements.  The actual
number is made up of a 16-bit subsystem identifier (a message set number)
and a 14-bit message number within that set.  Message Numbers in each set
should be defined in a message catalog file, and generated to #defines
in a header file.  The intention is that each message may contain text
in one or more natural languages, allowing a message to be formatted with
parameter values in the user's locale.

The tooling to automate this will be included in this repository later.

Example:

	#include	<subsys_errors.h>
		....
		return SubsysErrorOfSomeType(param1, param2, ...)
		....

		switch (Error e = DoSomeWork())
		{
		case 0:
			// That seemed to go ok
			break;
		case ENOENT:
			// No such file or directory!
			break;

		case SubsysErrorOfSomeTypeNum:
			// Handle the error
			Complain(e);
			return;
		}
