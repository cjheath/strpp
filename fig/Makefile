#
# Vanilla makefile for FIG: Fact Interchange Grammar
#

CXX	=	g++
CXXFLAGS =	-std=c++11

COPT	=	-DHAVE_PTHREADS # -DPEG_TRACE

DEBUG	=	-O2 $(COPT)
# DEBUG	=	-g $(COPT) -DUTF8_ASSERT
# DEBUG	=	-O2 $(COPT) -lprofiler
# DEBUG	=	-g -DTRACK_RESULTS $(COPT)

HDRS	=	\

SRCS	=	\
		fig.cpp

OBJS	=	$(patsubst %,%,$(SRCS:.cpp=.o))

LIBS	=	-L.. -lstrpp

# vpath	%.cpp	../src
vpath	%.h	../include
vpath	%.a	..

all:	strpp fig

fig:	Makefile peg_ast.h $(HDRS) $(SRCS) fig_parser.cpp
	$(CXX) $(DEBUG) $(CXXFLAGS) -I. -I../include -I../test -o $@ $(SRCS) ../test/memory_monitor.cpp $(LIBS)

# Regenerate fig_parser from fig.px:
fig_parser.cpp: ../px/px fig.px
	../px/px fig.px >fig_parser.cpp

strpp:	
	cd ..; $(MAKE) lib

../px/px:
	cd ../px; $(MAKE)

test:

tests:

clean:
	rm -f fig $(OBJS)
	rm -rf *.dSYM

clobber:	clean

.PHONY:	all strpp clean test tests clean clobber
