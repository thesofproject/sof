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

# list tests

check_PROGRAMS += list_init
list_init_SOURCES = src/list/list_init.c

check_PROGRAMS += list_is_empty
list_is_empty_SOURCES = src/list/list_is_empty.c

check_PROGRAMS += list_item_append
list_item_append_SOURCES = src/list/list_item_append.c

check_PROGRAMS += list_item_del
list_item_del_SOURCES = src/list/list_item_del.c

check_PROGRAMS += list_item_is_last
list_item_is_last_SOURCES = src/list/list_item_is_last.c

check_PROGRAMS += list_item_prepend
list_item_prepend_SOURCES = src/list/list_item_prepend.c

check_PROGRAMS += list_item
list_item_SOURCES = src/list/list_item.c

# math/numbers tests

check_PROGRAMS += gcd
gcd_SOURCES = src/math/numbers/gcd.c
gcd_LDADD = ../../src/math/libsof_math.a $(LDADD)

# all our binaries are test cases
TESTS = $(check_PROGRAMS)
