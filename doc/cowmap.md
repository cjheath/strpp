## COWMap

`#include	<cowmap.h>`

The Copy-on-write Map template uses a C++ STL map template class (sorted red-black trees)
under a reference-counted zero side-effect implementation. Any modification to a
map via a reference will first copy the map if there are any other references.
Looking up an entry in a map returns a *copy* of the entry, so it is necessary
to explicitly put a modified entry back into the map.

The COWMap template is functional but rudimentary, still under development.
