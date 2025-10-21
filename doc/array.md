## Array with slices

`#include	<array.h>`

The Array<T> template creates a slice into an array of type T.
New slices (and copies) onto the same ArrayBody are inexpensive (using atomic reference-counting),
but any attempt to modify a slice first creates a copy of the Body, leaving other slices unaffected.
The ArrayBody itself is only accessible as a constant, and a new Array may be created over a static body.

Read the header file for the API.

The StrVal class uses a specialisation of this template to provide its storage and reference counting.

