#
# Vanilla makefile for strpp
#

CXX	=	g++
CXXFLAGS =	-std=c++11

DEBUG	=	-Os $(COPT)	# -DSTRVAL_65K
# DEBUG	=	-g $(COPT)
# DEBUG	=	-g -DTRACK_RESULTS $(COPT)

HDRS	=	char_encoding.h		\
		refcount.h		\
		strpp.h 		\
		strregex.h
SRCS	=	char_encoding.cpp	\
		strmatch.cpp		\
		strpp.cpp		\
		strrdump.cpp		\
		strregex.cpp
LIB	=	libstrpp.a
TESTS	=	match_test regex_test

_OBJS	=	$(SRCS:.cpp=.o)
OBJS	=	$(patsubst %,build/%,$(_OBJS))

vpath	%.cpp	src:test
vpath	%.h	include

all:	lib

lib:	$(LIB)
$(LIB):	build $(OBJS)
	$(AR) cr $@ $(OBJS)

tests:	$(TESTS)

%:	%.cpp $(LIB)
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Itest -o $@ $< test/memory_monitor.cpp $(LIB)

build/%.o:	%.cpp $(HDRS) Makefile
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Isrc -o $@ -c $<

%.o:	%.cpp $(HDRS) Makefile
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Isrc -o $@ -c $<

build:
	@mkdir build

clean:
	rm -rf $(OBJS) $(TESTS) $(TESTS:=.dSYM)
	@rmdir build 2>/dev/null || true

clobber:	clean
	rm -f $(LIB)

.PHONY:	all lib clean tests clean clobber
