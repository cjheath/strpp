#
# Vanilla makefile for strpp
#

CXX	=	g++
CXXFLAGS =	-std=c++11

COPT	=	-DHAVE_PTHREADS # -DPEG_TRACE
# For a real FreeRTOS build, use -DHAVE_FREERTOS instead of -DHAVE_PTHREADS above,
# add e.g. -DTHREAD_DEFAULT_STACK_BYTES=4096 -DTHREAD_DEFAULT_PRIORITY=1 -DMAX_THREAD=8,
# and point -I at your real FreeRTOS headers instead of test/freertos_stub.
# See `make freertos_check` below for a compile/link check against a stub.

MEMCHECK =
#MEMCHECK =	-DMEMCHECK test/memory_monitor.cpp

DEBUG	=	-O2 $(COPT)
# DEBUG	=	-g $(COPT) # -DUTF8_ASSERT
# DEBUG	=	-O2 $(COPT) -lprofiler
# DEBUG	=	-g -DTRACK_RESULTS $(COPT)

HDRS	=	\
		array.h			\
		char_encoding.h		\
		charpointer.h		\
		condition.h		\
		cowmap.h		\
		error.h			\
		char_ptr.h		\
		utf8_ptr.h		\
		peg.h			\
		pegexp.h		\
		peg_ast.h		\
		redblack.h		\
		refcount.h		\
		strval.h		\
		taggedref.h		\
		thread.h		\
		variant.h

SRCS	=	\
		char_encoding.cpp	\
		condition.cpp		\
		lockfree.cpp		\
		thread.cpp		\
		variant.cpp

LIB	=	libstrpp.a
TESTS	=	\
		array_test		\
		char_encoding_test	\
		cowmap_test		\
		err_test		\
		greeting_test		\
		medley_test		\
		peg_test		\
		pegexp_test		\
		reassembly_test		\
		redblack_test		\
		strval_test		\
		taggedref_test		\
		thread_test		\
		utf8pointer_test	\
		variant_test

SUBDIRS	=	rx

OBJS	=	$(patsubst %,build/%,$(SRCS:.cpp=.o))

vpath	%.c	src:test
vpath	%.cpp	src:test
vpath	%.h	include

all:	lib
	$(foreach subdir,$(SUBDIRS),$(MAKE) -C $(subdir) $@; )

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

$(TESTS): $(HDRS)

run_peg_test: peg_test
	peg_test ../fig/fig.px

run_pegexp_size_test:
	@rm pegexp_size_test.o 2>/dev/null || true
	@$(MAKE) pegexp_size_test.o
	@echo Pegexp code size:
	@size pegexp_size_test.o
	@rm pegexp_size_test.o

run_variant_test: variant_test
	variant_test

%:	%.cpp $(LIB) $(MEMCHECK)
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Itest -o $@ $< $(MEMCHECK) $(LIB)

px:
	cd ../px; $(MAKE)

thread_test:	thread_test.cpp $(LIB)
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Itest -o $@ $< $(LIB)

build/%.o:	%.cpp $(HDRS) Makefile
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Isrc -o $@ -c $<

build/char_encoding.o: case_conversions.c

$(TESTS):	$(HDRS) Makefile

%.o:	%.cpp $(HDRS) Makefile
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Isrc -o $@ -c $<

# --- FreeRTOS compile/link check --------------------------------------------
# test/freertos_stub/ is a MINIMAL, COMPILE-CHECK-ONLY stand-in for the real
# FreeRTOS headers (see the comment in test/freertos_stub/FreeRTOS.h) - there's
# no scheduler behind it, so `make freertos_check` builds thread_test.cpp and
# the library against it purely to catch transcription errors (wrong types,
# wrong constant names, etc) in the HAVE_FREERTOS branches of thread.h/
# thread.cpp/condition.h/condition.cpp/lockfree.h/lockfree.cpp.
#
# DO NOT RUN the resulting binary: xTaskCreate() in the stub never actually
# runs the task function, so every thread stays in the New state forever and
# main() hangs in joinAny(). Real runtime testing needs genuine FreeRTOS
# sources on target hardware or QEMU, which this Makefile doesn't attempt.
FREERTOS_COPT	=	-DHAVE_FREERTOS -DTHREAD_DEFAULT_STACK_BYTES=4096 -DTHREAD_DEFAULT_PRIORITY=1 -DMAX_THREAD=8
FREERTOS_INC	=	-Itest/freertos_stub
FREERTOS_OBJS	=	$(patsubst %,build/freertos/%,$(SRCS:.cpp=.o))

freertos_check:	thread_test_freertos
	@echo "FreeRTOS stub build OK (compile/link check only - do not run thread_test_freertos)"

thread_test_freertos:	thread_test.cpp libstrpp_freertos.a
	$(CXX) $(CXXFLAGS) $(FREERTOS_COPT) -Iinclude -Itest $(FREERTOS_INC) -o $@ $< libstrpp_freertos.a

libstrpp_freertos.a:	build/freertos $(FREERTOS_OBJS)
	$(AR) cr $@ $(FREERTOS_OBJS)

build/freertos/%.o:	%.cpp $(HDRS) Makefile
	$(CXX) $(CXXFLAGS) $(FREERTOS_COPT) -Iinclude -Isrc $(FREERTOS_INC) -o $@ -c $<

build/freertos/char_encoding.o: case_conversions.c

build/freertos:
	@mkdir -p build/freertos

build:
	@mkdir build

clean:
	rm -f $(OBJS) $(TESTS)
	rm -f $(FREERTOS_OBJS) thread_test_freertos
	rm -rf *.dSYM
	@rmdir build/freertos 2>/dev/null || true
	@rmdir build 2>/dev/null || true
	$(foreach subdir,$(SUBDIRS),$(MAKE) -C $(subdir) $@;)

clobber:	clean
	rm -f $(LIB) libstrpp_freertos.a
	$(foreach subdir,$(SUBDIRS),$(MAKE) -C $(subdir) $@;)

.PHONY:	all lib clean test tests clean clobber px freertos_check
