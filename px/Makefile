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

SRCS	=	\
		px.cpp

LIBS	=	-L.. -lstrpp

OBJS	=	$(patsubst %,%,$(SRCS:.cpp=.o))

# vpath	%.cpp	../src
vpath	%.h	../include
vpath	%.a	..

all:	px

px:	Makefile lib peg_ast.h px_pegexp.h px_cpp.h px_pegexp.cpp px_cpp.cpp px_parser.cpp px.cpp
	$(CXX) $(DEBUG) $(CXXFLAGS) -I. -I../include -o $@ px_pegexp.cpp px_cpp.cpp px.cpp $(LIBS)

px_parser.cpp: px.px
	px px.px >px_parser.cpp

lib:	
	cd ..; make lib

#%.o:	%.cpp $(HDRS) Makefile
#	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Isrc -o $@ -c $<

clean:
	rm -rf px $(OBJS) *.dSYM

clobber:	clean
	rm -f $(LIB)

.PHONY:	all lib clean test tests clean clobber
