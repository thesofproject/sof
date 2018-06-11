check_PROGRAMS =

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tap-driver.sh

if BUILD_XTENSA
LOG_COMPILER = xt-run
endif

AM_CFLAGS = \
	$(SOF_INCDIR)

AM_LDFLAGS =

if HAVE_CMOCKA_PREFIX
AM_CFLAGS += -I$(CMOCKA_PREFIX)/include
AM_LDFLAGS += -L$(CMOCKA_PREFIX)/lib
endif

CFLAGS := $(filter-out -nostdlib,$(CFLAGS))
LDFLAGS := $(filter-out -nostdlib,$(LDFLAGS))

LDADD = -lcmocka

# math/numbers tests

check_PROGRAMS += gcd
gcd_SOURCES = src/math/numbers/gcd.c
gcd_LDADD = ../../src/math/libsof_math.a $(LDADD)

# all our binaries are test cases
TESTS = $(check_PROGRAMS)
