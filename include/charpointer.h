#if !defined(CHARPOINTER_H)
#define CHARPOINTER_H

#include	<guarded_utf8_pointer.h>
#include	<guarded_char_pointer.h>

#if	defined(UNICODE)

using	CharPointer = GuardedUTF8Ptr;
using	Char = UTF8;

#else

using	CharPointer = GuardedCharPtr;
using	Char = char;

#endif

#endif
