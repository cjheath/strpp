/*
 * The one thing Array's header cannot do for itself: report.
 *
 * array.h sits below the strval.h -> variant.h -> errbuf.h cycle, so it cannot
 * name an ErrorSTR_* function; and ArrayR is a template, so a reporting member
 * could not be defined out of line once for every Element a caller might
 * instantiate it with. So the report is one non-template function, declared in
 * array.h and defined here, where Error() is at hand - one call serves every
 * instantiation.
 *
 * (c) Copyright Clifford Heath 2026. See LICENSE file for usage rights.
 */
#include	<array.h>
#include	<strpp_msg.h>

/*
 * A caller asked an Array for an element it has not got. `index` is the index
 * that is out of range, or the first the request needed and did not have, and
 * `operation` is the verb: "remove", "drop", "take", "slice".
 */
ErrNum
array_index_error(size_t index, size_t length, const char* operation)
{
	return ErrorSTR_IndexOutOfRange(operation, index, length);
}
