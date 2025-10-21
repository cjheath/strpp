#if !defined(PX_PARSER_H)
#define PX_PARSER_H

#include	<strval.h>
#include	<variant.h>
#include	<peg.h>
#include	<peg_ast.h>

typedef	Peg<PegMemorySource, PegMatch, PegContext>	PxParser;

#endif // PX_PARSER_H
