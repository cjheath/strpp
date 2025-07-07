#
# Vanilla makefile for strpp
#

CXX	=	g++
CXXFLAGS =	-std=c++11

DEBUG	=	-O2 $(COPT)
# DEBUG	=	-g $(COPT) # -DPEG_TRACE
# DEBUG	=	-O2 $(COPT) -lprofiler
# DEBUG	=	-g -DTRACK_RESULTS $(COPT)

HDRS	=	\
		array.h			\
		char_encoding.h		\
		charpointer.h		\
		cowmap.h		\
		error.h			\
		char_ptr.h		\
		utf8_ptr.h		\
		peg.h			\
		pegexp.h		\
		refcount.h		\
		strval.h		\
		strregex.h		\
		variant.h

SRCS	=	\
		char_encoding.cpp	

RX_SRCS	=	\
		rxcompile.cpp		\
		rxdump.cpp		\
		rxmatch.cpp

LIB	=	libstrpp.a
TESTS	=	\
		array_test		\
		err_test		\
		greeting_test		\
		medley_test		\
		peg_test		\
		pegexp_test		\
		px			\
		reassembly_test		\
		rxcompile_test		\
		rxmatch_test		\
		strval_test		\
		utf8pointer_test	\
		variant_test

OBJS	=	$(patsubst %,build/%,$(SRCS:.cpp=.o))
RX_OBJS	=	$(patsubst %,build/%,$(RX_SRCS:.cpp=.o))

vpath	%.cpp	src:test
vpath	%.h	include

all:	lib $(RX_OBJS)

lib:	$(LIB)
$(LIB):	build $(OBJS)
	$(AR) cr $@ $(OBJS)

tests:	$(TESTS)

test:	run_pegexp_test run_pegexp_size_test \
	run_peg_test run_peg_size_test \
	run_variant_test

run_pegexp_test: pegexp_test
	test/run_pegexp_test < test/pegexp_test.cases

run_peg_size_test:
	@rm peg_size_test.o 2>/dev/null || true
	@$(MAKE) peg_size_test.o
	@echo PEG code size:
	@size peg_size_test.o
	@rm peg_size_test.o

run_peg_test: peg_test
	peg_test grammars/px.px

run_pegexp_size_test:
	@rm pegexp_size_test.o 2>/dev/null || true
	@$(MAKE) pegexp_size_test.o
	@echo Pegexp code size:
	@size pegexp_size_test.o
	@rm pegexp_size_test.o

run_variant_test: variant_test
	variant_test

%:	%.cpp $(LIB) test/memory_monitor.cpp
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Itest -o $@ $< test/memory_monitor.cpp $(LIB)

build/%.o:	%.cpp $(HDRS) Makefile
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Isrc -o $@ -c $<

$(TESTS):	$(HDRS) Makefile

%.o:	%.cpp $(HDRS) Makefile
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Isrc -o $@ -c $<

build:
	@mkdir build

clean:
	rm -rf $(OBJS) $(RX_OBJS) $(TESTS) $(TESTS:=.dSYM)
	@rmdir build 2>/dev/null || true

clobber:	clean
	rm -f $(LIB)

.PHONY:	all lib clean test tests clean clobber
