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
		datetime.h		\
		error.h			\
		gregorian.h		\
		char_ptr.h		\
		utf8_ptr.h		\
		peg.h			\
		pegexp.h		\
		peg_ast.h		\
		msgqueue.h		\
		redblack.h		\
		refcount.h		\
		lock.h			\
		lockfree.h		\
		strassert.h		\
		strpp_err.h		\
		strformat.h		\
		strpp_msg.h		\
		threadid.h		\
		strval.h		\
		taggedref.h		\
		thread.h		\
		thread_local.h		\
		variant.h		\
		window.h

SRCS	=	\
		array.cpp		\
		char_encoding.cpp	\
		condition.cpp		\
		datetime.cpp		\
		errbuf.cpp		\
		gregorian.cpp		\
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
		datetime_test		\
		err_test		\
		errbuf_test		\
		greeting_test		\
		gregorian_test		\
		lock_test		\
		medley_test		\
		peg_test		\
		pegexp_test		\
		msgqueue_test		\
		reassembly_test		\
		redblack_test		\
		refcount_test		\
		strformat_test		\
		strval_test		\
		taggedref_test		\
		thread_test		\
		thread_local_test	\
		utf8pointer_test	\
		variant_test		\
		variant_ndebug_test	\
		window_test

SUBDIRS	=	rx tools

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

# The tests that give a verdict on their own: they take no arguments and exit
# non-zero if any of their checks failed, or if they die. peg_test and
# pegexp_test are not among them - they take a grammar or a case file on the
# command line - so each has a run_* target of its own below, and so do the two
# size reports, which measure rather than check.
RUNTESTS	=	$(filter-out peg_test pegexp_test,$(TESTS))

# Run every one of them, saying which is running, and stop at the first that
# fails so that make returns non-zero. `make tests` only builds them.
test:	$(RUNTESTS) run_peg_test run_pegexp_test \
	run_pegexp_size_test run_peg_size_test
	@for t in $(RUNTESTS); do \
		echo "--- $$t"; \
		./$$t || exit 1; \
	done
	@echo "All $(words $(RUNTESTS)) test programs passed"

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

# Build the documentation site and stage it on the gh-pages branch, in a
# worktree under build/. Nothing is committed here: see `publish`, which does
# this, opens an editor for a commit message, and pushes. See book.toml.
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
	@echo "then 'make publish' to commit it and push, or 'make commit-docs' to"
	@echo "commit without pushing"

# Commit the staged documentation, opening an editor for the message, and push
# it to gh-pages. The worktree is removed once the push has gone, so the next
# `make publish` starts it again; a push that fails leaves it in place to retry.
commit-docs: doc
	cd build/gh-pages && git commit

publish: commit-docs
	cd build/gh-pages && git push
	git worktree remove --force build/gh-pages

px:
	cd ../px; $(MAKE)

# ---- The message catalog ----
#
# The schema is tools/mcs.adl, which a catalog is written against; strpp's own
# catalog and its translations are in messages/; and tools/msggen is the
# generator, built from its own directory when it is wanted. Both targets below
# need the neighbouring ADL repository built - adlmem to parse the catalogs,
# msggen to generate from them - and adl/cpp builds against this library, so
# neither runs as part of the normal build.
MCS_ADL		=	../adl/cpp
MCS_SCHEMA	=	tools/mcs.adl
MCS_CATALOG	=	messages/strpp.mcs
MCS_TRANSLATIONS = \
			messages/strpp.es.mcs \
			messages/strpp.de.mcs \
			messages/strpp.fr.mcs \
			messages/strpp.it.mcs \
			messages/strpp.zh.mcs \
			messages/strpp.ja.mcs

# Parsed in the order the generator loads them: the language enumeration, then
# the schema that refers to it, then the catalog, then each translation - a
# translation's bare names resolve against the context the catalog leaves
# behind, and each ends with a bare `Strpp;` to leave that context where the
# next one wants it. Add a translation here as one appears.
check:	$(MCS_SCHEMA) $(MCS_CATALOG) $(MCS_TRANSLATIONS)
	@test -x $(MCS_ADL)/adlmem || { \
		echo "build it first: cd $(MCS_ADL) && make"; exit 1; }
	cd $(MCS_ADL) && ./adlmem -a adl.adl -a ietf_languages.adl $(CURDIR)/$(MCS_SCHEMA) >/dev/null \
		&& echo "tools/mcs.adl parses, with the language enumeration it refers to"
	cd $(MCS_ADL) && ./adlmem -a adl.adl -a ietf_languages.adl -a $(CURDIR)/$(MCS_SCHEMA) \
		-a $(CURDIR)/$(MCS_CATALOG) \
		$(patsubst %,-a $(CURDIR)/%,$(MCS_TRANSLATIONS)) >/dev/null \
		&& echo "the catalog and its translations parse"

# Manual only, and never run by `all`: this REWRITES include/strpp_err.h and
# include/strpp_msg.h from the catalog, so read the diff before committing it.
# The generated pair is committed rather than built on demand because of the
# cycle above - adl/cpp cannot be built before this library exists.
regenerate:	tools/msggen
	tools/msggen -d include $(MCS_ADL)/adl.adl $(MCS_ADL)/ietf_languages.adl \
		$(MCS_SCHEMA) $(MCS_CATALOG)

tools/msggen:	tools/msggen.cpp
	$(MAKE) -C tools msggen

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
	@$(CXX) $(CXXFLAGS) $(FREERTOS_COPT) -Iinclude -Itest $(FREERTOS_INC) \
		-fsyntax-only test/msgqueue_freertos_branch_check.cpp
	@echo "FreeRTOS queue branch compiles"
	@$(CXX) $(CXXFLAGS) -Iinclude -Itest \
		-fsyntax-only test/msgqueue_freertos_branch_check.cpp
	@echo "No-threading queue branch compiles"

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

.PHONY:	all lib clean test tests doc commit-docs publish freertos_check check regenerate
