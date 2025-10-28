#if !defined(PX_RAILROAD_H)
#define PX_RAILROAD_H
/*
 * Javascript/SVG generator for railroad diagrams for a parser
 *
 * Copyright 2025 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<strval.h>
#include	<variant.h>

extern	StrArray	omitted_rules;

void emit_railroad(const char* parser_name, VariantArray rules);

#endif // PX_RAILROAD_H
