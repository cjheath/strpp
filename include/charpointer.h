#if !defined(CHARPOINTER_H)
#define CHARPOINTER_H
/*
 * Based on the #define UNICODE, choose a type for Char and CharPointer.
 *
 * (c) Copyright Clifford Heath 2023. See LICENSE file for usage rights.
 */

#if	defined(UNICODE)

#include	<utf8_ptr.h>

using	CharPointer = GuardedUTF8Ptr;
using	Char = UTF8;

#else

#include	<char_ptr.h>

using	CharPointer = GuardedCharPtr;
using	Char = char;

#endif

#endif
