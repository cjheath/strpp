#
# msggen: the message catalog generator. What a catalog is and how to use this
# is in doc/messages.md; the ADL schema it reads is mcs.adl, beside this.
#
# Building msggen depends on the neighbouring ADL repository, and adl/cpp in
# turn builds against this library - so neither can be built from a tree that
# is not already set up. That is why `all` does nothing: a user's build of
# strpp must not reach in here. Build it when it is wanted:
#
#	make msggen
#
# A library's own catalog, and the rules that generate its headers from it,
# are that library's business and live in its top Makefile.

CXX     =       g++
CXXFLAGS =      -std=c++11

ADL	=	../../adl/cpp
STRPP	=	..

all:
	@true

msggen:	Makefile msggen.cpp 
	$(CXX) $(CXXFLAGS) -I$(ADL) -I$(STRPP)/include -DHAVE_PTHREADS \
		-o msggen msggen.cpp -L$(ADL) -L$(STRPP) -lstrpp

clean:
	rm -f msggen
