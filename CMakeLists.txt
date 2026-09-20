# strpp as an ESP-IDF component.
#
# Most of strpp (StrVal, Variant, CowMap, the persistent red-black tree, the
# PEG/Pegexp parsers, TaggedRef, Array, etc.) is header-only template code
# under include/, which any component depending on this one reaches simply
# by #include - no separate compilation needed for those.
#
# What does need compiling: thread.cpp/condition.cpp/lockfree.cpp
# (Thread/Latch/Condition, see include/thread.h, include/lockfree.h,
# include/condition.h), the stand-alone char_encoding.cpp and variant.cpp,
# errbuf.cpp - the error buffer (ErrBuf/Error, see include/errbuf.h) that every
# reported message goes through, Variant's type assertion among them - and
# strassert.cpp, the assertion that dumps that buffer before it dies.
#
# The error buffer holds a thread-local slot, as does anything else wanting one
# per thread, so FreeRTOS's configNUM_THREAD_LOCAL_STORAGE_POINTERS
# (CONFIG_FREERTOS_THREAD_LOCAL_STORAGE_POINTERS) has to be raised from its
# default of 0 to at least THREAD_LOCAL_MAX_SLOTS, which is 16. See
# include/thread_local.h.
#
# HAVE_FREERTOS is always defined for this component, since ESP-IDF always
# runs on FreeRTOS - see the HAVE_FREERTOS branches in thread.h/lockfree.h/
# condition.h (and their .cpp files) for what that selects. It's PUBLIC so
# that other components including these headers see the same macro.
idf_component_register(
	SRCS
		"src/char_encoding.cpp"
		"src/condition.cpp"
		"src/errbuf.cpp"
		"src/lockfree.cpp"
		"src/strassert.cpp"
		"src/thread.cpp"
		"src/variant.cpp"
	INCLUDE_DIRS
		"include"
	PRIV_INCLUDE_DIRS
		"src"		# case_conversions.c is #included by char_encoding.cpp, not built on its own
	REQUIRES
		"freertos"
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC
	HAVE_FREERTOS
	THREAD_DEFAULT_STACK_BYTES=${CONFIG_STRPP_THREAD_DEFAULT_STACK_BYTES}
	THREAD_DEFAULT_PRIORITY=${CONFIG_STRPP_THREAD_DEFAULT_PRIORITY}
)

# MAX_THREAD (see Array<Thread*> Thread::threads in src/thread.cpp) is tested
# with #if defined(MAX_THREAD), not by its value, so it must be left
# completely undefined for the "0 = unbounded" case - not defined as 0.
if(CONFIG_STRPP_MAX_THREAD)
	target_compile_definitions(${COMPONENT_LIB} PUBLIC
		MAX_THREAD=${CONFIG_STRPP_MAX_THREAD})
endif()
