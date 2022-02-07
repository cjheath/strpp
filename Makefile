#
# Vanilla makefile for strpp
#

CXX	=	g++

SRCS	=	char_encoding.cpp	\
		strmatch.cpp		\
		strpp.cpp		\
		strrdump.cpp		\
		strregex.cpp
_OBJS	=	$(SRCS:.cpp=.o)
OBJS	=	$(patsubst %,build/%,$(_OBJS))
LIB	=	libstrpp.a
TESTS	=	match_test regex_test

# DEBUG	=	-g -DTRACK_RESULTS
DEBUG	=	-Os	# -DSTRVAL_65K

vpath %.cpp src

all:	lib

lib:	libstrpp.a
$(LIB):	build $(OBJS)
	$(AR) cr $@ $(OBJS)

tests:	$(TESTS)

%_test:	lib test/%_test.cpp
	$(CXX) $(DEBUG) -std=c++11 -Iinclude -Itest -o $@ test/$@.cpp test/memory_monitor.cpp libstrpp.a

build/%.o:	%.cpp
	$(CXX) $(DEBUG) -std=c++11 -Iinclude -Isrc -o $@ -c $<

build:
	@mkdir build

clean:
	rm -f $(OBJS) $(TESTS)

clobber:	clean
	rm -f $(LIB)
	@rmdir build 2>/dev/null || true

.PHONY:	all lib clean tests clean clobber
