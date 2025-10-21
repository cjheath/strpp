#
# Vanilla makefile for rx, a Thompson regular expression engine
#

CXX	=	g++
CXXFLAGS =	-std=c++11

COPT	=	-DHAVE_PTHREADS # -DPEG_TRACE

DEBUG	=	-O2 $(COPT)
# DEBUG	=	-g $(COPT) -DUTF8_ASSERT
# DEBUG	=	-O2 $(COPT) -lprofiler
# DEBUG	=	-g -DTRACK_RESULTS $(COPT)

HDRS	=	\
		strregex.h

SRCS	=	\
		rxcompile.cpp \
		rxdump.cpp \
		rxmatch.cpp

TESTS	=	\
		rxcompile_test		\
		rxmatch_test

OBJS	=	$(patsubst %,build/%,$(SRCS:.cpp=.o))

LIB	=	librx.a

LIBS	=	-L.. -lstrpp

vpath	%.cpp	.:../test
vpath	%.h	../include
vpath	%.a	..

all:	$(LIB)

lib:	strpp $(LIB)

strpp:
	cd ..; make lib

$(LIB):	build $(OBJS)
	$(AR) cr $@ $(OBJS)

test:	tests
	rxcompile_test
	rxmatch_test

tests:	$(TESTS)

$(TESTS):	$(HDRS) Makefile

rxcompile_test: rxcompile_test.cpp strpp $(LIB) $(OBJS)
	$(CXX) $(DEBUG) $(CXXFLAGS) -I. -I../include -I../test -o $@ $< $(OBJS) ../test/memory_monitor.cpp $(LIB) $(LIBS)

rxmatch_test: rxmatch_test.cpp strpp $(LIB) $(OBJS)
	$(CXX) $(DEBUG) $(CXXFLAGS) -I. -I../include -I../test -o $@ $< $(OBJS) ../test/memory_monitor.cpp $(LIB) $(LIBS)

%:	%.cpp $(LIB) test/memory_monitor.cpp
	$(CXX) $(DEBUG) $(CXXFLAGS) -I. -I../include -Itest -o $@ $< test/memory_monitor.cpp $(LIB)

build/%.o:	%.cpp $(HDRS) Makefile
	$(CXX) $(DEBUG) $(CXXFLAGS) -I. -I../include -Isrc -o $@ -c $<

build:
	@mkdir build

clean:
	rm -f $(OBJS) $(TESTS)
	rm -rf build *.dSYM

clobber:	clean
	rm -f $(LIB) 

.PHONY:	all lib clean test tests clean clobber
