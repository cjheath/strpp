#
# The message catalog tools.
#
# mcs.adl is the schema; ../messages/strpp.mcs is strpp's own real catalog
# written against it. `make check` parses the schema with the ADL
# implementation to be sure it is still well formed.
#
# msggen.cpp is the generator, and it IS a dependency on the neighbouring
# ADL repository - the one place in strpp/tools allowed to be. `all` still
# does nothing, deliberately: msggen is not part of strpp's normal build,
# and `regenerate` is never run by it either. There is a real cyclic
# dependency here - adl/cpp cannot build without strpp's own library, so
# strpp's build cannot depend on adl/cpp being built first - which is why
# msggen is run by hand, and its output (../include/strpp_err.h,
# strpp_msg.h) is committed rather than built on demand.

ADL	=	../../adl/cpp
ADLMEM	=	$(ADL)/adlmem
STRPP	=	..

all:
	@true

check:
	@test -x $(ADLMEM) || { \
		echo "build it first: cd $(ADL) && make"; exit 1; }
	cd $(ADL) && ./adlmem -a adl.adl $(CURDIR)/mcs.adl >/dev/null \
		&& echo "mcs.adl parses"

msggen:	msggen.cpp
	g++ -std=c++17 -I$(ADL) -I$(STRPP)/include -DHAVE_PTHREADS \
		-o msggen msggen.cpp -L$(ADL) -L$(STRPP) -lstrpp

# Manual only - never run by `all` or by anything else. Regenerates and
# OVERWRITES ../include/strpp_err.h and strpp_msg.h; review the diff before
# committing, the same as any other hand-reviewed change.
regenerate:	msggen
	./msggen $(ADL)/adl.adl mcs.adl $(STRPP)/messages/strpp.mcs $(STRPP)/include

clean:
	rm -f msggen
