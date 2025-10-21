#if !defined(PX_PEGEXP_H)
#define PX_PEGEXP_H
/*
 * Px PEG parser generator. Functions to make Px to Pegexp
 *
 * Copyright 2025 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<strval.h>
#include	<variant.h>

StrVal	generate_pegexp(Variant re);
StrVal	generate_parameters(Variant parameters);
StrVal	generate_parameter(Variant parameter_map);

#endif // PX_PEGEXP_H
