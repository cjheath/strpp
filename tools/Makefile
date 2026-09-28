#
# The message catalog tools.
#
# mcs.adl is the schema; ../messages/strpp.mcs is strpp's own real catalog
# written against it. `make check` parses the schema with the ADL
# implementation to be sure it is still well formed.
#
# LANGUAGES is the neighbouring adl repo's own IETF language enumeration,
# which mcs.adl refers to and does not declare - so it is loaded between
# adl.adl and mcs.adl, and mcs.adl on its own no longer parses.
#
# msggen.cpp is the generator, and it IS a dependency on the neighbouring
# ADL repository - the one place in strpp/tools allowed to be. `all` still
# does nothing, deliberately: msggen is not part of strpp's normal build,
# and `regenerate` is never run by it either. There is a real cyclic
# dependency here - adl/cpp cannot build without strpp's own library, so
# strpp's build cannot depend on adl/cpp being built first - which is why
# msggen is run by hand, and its output (../include/strpp_err.h,
# strpp_msg.h) is committed rather than built on demand.
#
# LANGS is each language's own wordings, one file per IETF subtag (es, de,
# fr, it, zh, ja). They are not read by msggen - what it generates refers to
# the base message sets alone - but `check` parses them after the catalog.
# The order matters, and each file also anchors itself: it re-opens the
# catalog BY NAME, because a language file's top-level statements otherwise
# land in whatever object the file before it left behind, and that is a Set,
# which `Is Complete` closes to new content. Add a file here as a language
# appears, and wrap it the same way.

ADL	=	../../adl/cpp
ADLMEM	=	$(ADL)/adlmem
STRPP	=	..
LANGUAGES =	$(ADL)/ietf_languages.adl
CATALOG	=	$(STRPP)/messages/strpp.mcs
LANGS	=	$(STRPP)/messages/strpp.es.mcs \
		$(STRPP)/messages/strpp.de.mcs \
		$(STRPP)/messages/strpp.fr.mcs \
		$(STRPP)/messages/strpp.it.mcs \
		$(STRPP)/messages/strpp.zh.mcs \
		$(STRPP)/messages/strpp.ja.mcs

all:
	@true

# The order below is the same one msggen loads them in, and for the same
# reason - a language file's bare names resolve against the context the
# catalog leaves behind, so the catalog has to have been read first.
check:
	@test -x $(ADLMEM) || { \
		echo "build it first: cd $(ADL) && make"; exit 1; }
	cd $(ADL) && ./adlmem -a adl.adl -a ietf_languages.adl $(CURDIR)/mcs.adl >/dev/null \
		&& echo "mcs.adl parses, with the language enumeration it refers to"
	cd $(ADL) && ./adlmem -a adl.adl -a ietf_languages.adl -a $(CURDIR)/mcs.adl \
		-a $(CURDIR)/$(CATALOG) $(patsubst %,-a $(CURDIR)/%,$(LANGS)) >/dev/null \
		&& echo "the catalog and its languages parse"

msggen:	msggen.cpp
	g++ -std=c++17 -I$(ADL) -I$(STRPP)/include -DHAVE_PTHREADS \
		-o msggen msggen.cpp -L$(ADL) -L$(STRPP) -lstrpp

# Manual only - never run by `all` or by anything else. Regenerates and
# OVERWRITES ../include/strpp_err.h and strpp_msg.h; review the diff before
# committing, the same as any other hand-reviewed change.
regenerate:	msggen
	./msggen $(ADL)/adl.adl $(LANGUAGES) mcs.adl $(CATALOG) $(STRPP)/include

clean:
	rm -f msggen
