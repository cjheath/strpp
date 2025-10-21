#
# Vanilla makefile for px
#

CXX	=	g++
CXXFLAGS =	-std=c++11

COPT	=	-DHAVE_PTHREADS # -DPEG_TRACE

DEBUG	=	-O2 $(COPT)
# DEBUG	=	-g $(COPT) -DUTF8_ASSERT
# DEBUG	=	-O2 $(COPT) -lprofiler
# DEBUG	=	-g -DTRACK_RESULTS $(COPT)

HDRS	=	\
		px_pegexp.h \
		px_cpp.h

SRCS	=	\
		px_pegexp.cpp \
		px_cpp.cpp \
		px.cpp

OBJS	=	$(patsubst %,%,$(SRCS:.cpp=.o))

LIBS	=	-L.. -lstrpp

# vpath	%.cpp	../src
vpath	%.h	../include
vpath	%.a	..

all:	px

px:	Makefile lib peg_ast.h $(HDRS) $(SRCS) px_parser.cpp
	$(CXX) $(DEBUG) $(CXXFLAGS) -I. -I../include -o $@ $(SRCS) $(LIBS)

# Regenerate px_parser from px.px:
px_parser.cpp: px.px
	px px.px >px_parser.cpp

lib:	
	cd ..; make lib

test:

tests:

clean:
	rm -f px $(OBJS)
	rm -rf *.dSYM

clobber:	clean

.PHONY:	all lib clean test tests clean clobber
