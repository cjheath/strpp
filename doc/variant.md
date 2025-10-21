## Variant Data Type

`#include	<variant.h>`

The Variant class offers a type-safe way to manage numeric and StrVal types in a compact union,
along with Array<StrVal>, Array<Variant>, and a StrVal-keyed COWMap to Variant.
This allows building arbitrary data structures, which support asJSON() to render the whole structure as a string.

