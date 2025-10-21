#if !defined(PX_CPP_H)
#define PX_CPP_H
/*
 * Px PEG parser generator for C++
 *
 * Copyright 2025 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<strval.h>
#include	<variant.h>

void emit_cpp(const char* parser_name, VariantArray rules);

#endif // PX_CPP_H
