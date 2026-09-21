#
# Vanilla makefile for strpp
#

CXX	=	g++
CXXFLAGS =	-std=c++11

# The most levels of array or map that formatting and JSON rendering descend
# to, overridable here: `make DEPTH=8`. It bounds the stack against a
# structure far deeper than any message needs. See RENDER_MAX_DEPTH.
DEPTH	=	16

# The width in bits of the index a string counts with, and of the one an array
# body counts with. Each may be anything from 8 up to the width of a pointer,
# and the narrower it is the less memory a body's own counts take, which is
# what it is for on a small target. The number of characters a string may hold
# follows from the first, and is enforced rather than wrapping round.
# `make STRVALINDEXBITS=16 ARRAYINDEXBITS=16`
STRVALINDEXBITS	=	32
ARRAYINDEXBITS	=	32

COPT	=	-DHAVE_PTHREADS \
		-DRENDER_MAX_DEPTH=$(DEPTH) \
		-DStrValIndexBits=$(STRVALINDEXBITS) \
		-DArrayIndexBits=$(ARRAYINDEXBITS) # -DPEG_TRACE
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
		errbuf.h		\
		cowmap.h		\
		error.h			\
		char_ptr.h		\
		utf8_ptr.h		\
		peg.h			\
		pegexp.h		\
		peg_ast.h		\
		redblack.h		\
		refcount.h		\
		lockfree.h		\
		strassert.h		\
		str_err.h		\
		strformat.h		\
		str_msg.h		\
		threadid.h		\
		strval.h		\
		taggedref.h		\
		thread.h		\
		thread_local.h		\
		variant.h

SRCS	=	\
		char_encoding.cpp	\
		condition.cpp		\
		errbuf.cpp		\
		lockfree.cpp		\
		strassert.cpp		\
		strval.cpp		\
		thread.cpp		\
		variant.cpp

LIB	=	libstrpp.a
TESTS	=	\
		array_test		\
		assert_test		\
		char_encoding_test	\
		cowmap_test		\
		err_test		\
		errbuf_test		\
		greeting_test		\
		medley_test		\
		peg_test		\
		pegexp_test		\
		reassembly_test		\
		redblack_test		\
		strformat_test		\
		strval_test		\
		taggedref_test		\
		thread_test		\
		thread_local_test	\
		utf8pointer_test	\
		variant_test		\
		variant_ndebug_test

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
	run_variant_test run_variant_ndebug_test

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
	peg_test test/fig.px

run_pegexp_size_test:
	@rm pegexp_size_test.o 2>/dev/null || true
	@$(MAKE) pegexp_size_test.o
	@echo Pegexp code size:
	@size pegexp_size_test.o
	@rm pegexp_size_test.o

run_variant_test: variant_test
	variant_test

# The half of a refused coercion that only a build without assertions can show:
# there is no one to stop for, so the value must be kept. It needs NDEBUG all
# through, so the two library sources that report are compiled here rather than
# taken from the archive, whose copies are built with assertions on. Nothing
# else in the archive defines what they do, so the linker pulls only the rest.
variant_ndebug_test: test/variant_ndebug_test.cpp $(HDRS) Makefile
	$(CXX) $(DEBUG) -DNDEBUG $(CXXFLAGS) -Iinclude -Itest -o $@ \
		$< src/variant.cpp src/errbuf.cpp $(LIB)

run_variant_ndebug_test: variant_ndebug_test
	variant_ndebug_test

%:	%.cpp $(LIB) $(MEMCHECK)
	$(CXX) $(DEBUG) $(CXXFLAGS) -Iinclude -Itest -o $@ $< $(MEMCHECK) $(LIB)

# Build the documentation site and commit it to the gh-pages branch, for review
# before pushing. Nothing is pushed. See book.toml.
doc:
	@command -v mdbook >/dev/null || { echo "mdbook is not installed: brew install mdbook"; exit 1; }
	mdbook build
	git worktree prune
	@test -d build/gh-pages || git worktree add -f build/gh-pages gh-pages
	find build/gh-pages -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
	cp -R build/doc/. build/gh-pages/
	cd build/gh-pages && git add -A
	@echo "staged in build/gh-pages, on the gh-pages branch:"
	@cd build/gh-pages && git status --short | head -20
	@echo ""
	@echo "review with:  cd build/gh-pages && git status && git diff --cached"
	@echo "then commit it there, and 'git worktree remove --force build/gh-pages' when done"

# Commit the staged documentation in the gh-pages worktree, opening an editor
# for the message. Run `make doc` first.
commit-docs:
	@test -d build/gh-pages || { echo "nothing staged: run 'make doc' first"; exit 1; }
	cd build/gh-pages && git commit

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
	@$(CXX) $(CXXFLAGS) $(FREERTOS_COPT) -Iinclude -Itest $(FREERTOS_INC) \
		-fsyntax-only test/thread_local_branch_check.cpp
	@echo "FreeRTOS thread-local branch compiles"
	@$(CXX) $(CXXFLAGS) -Iinclude -Itest \
		-fsyntax-only test/thread_local_branch_check.cpp
	@echo "No-threading thread-local branch compiles"

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

.PHONY:	all lib clean test tests doc freertos_check
