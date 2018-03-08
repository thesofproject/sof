if BUILD_LIB
lib_LTLIBRARIES = libsof_math.la

libsof_math_la_SOURCES = \
	trig.c \
	numbers.c

libsof_math_la_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(COMMON_INCDIR)
else
noinst_LIBRARIES = libsof_math.a

libsof_math_a_SOURCES = \
	trig.c \
	numbers.c

libsof_math_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(COMMON_INCDIR)
endif
