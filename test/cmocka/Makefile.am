check_PROGRAMS =

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tap-driver.sh

if BUILD_XTENSA
LOG_COMPILER = xt-run
endif

override AM_CFLAGS := \
	$(filter-out -nostdlib,$(AM_CFLAGS)) \
	$(SOF_INCDIR)

override AM_LDFLAGS := \
	$(filter-out -nostdlib,$(AM_LDFLAGS))

if HAVE_CMOCKA_PREFIX
AM_CFLAGS += -I$(CMOCKA_PREFIX)/include
AM_LDFLAGS += -L$(CMOCKA_PREFIX)/lib
endif

if BUILD_XTENSA
AM_CFLAGS += -I../../src/arch/xtensa/include
AM_CFLAGS += -I../../src/platform/apollolake/include
AM_CFLAGS += -I../../src/audio
endif

if BUILD_HOST
AM_CFLAGS += -I../../src/arch/host/include
endif

LDADD = -lcmocka

# memory allocator test

if BUILD_XTENSA
check_PROGRAMS += alloc
alloc_SOURCES = src/lib/alloc/alloc.c src/lib/alloc/mock.c ../../src/lib/alloc.c ../../src/platform/apollolake/memory.c
endif

# volume tests

check_PROGRAMS += volume_process
volume_process_SOURCES = src/audio/volume/volume_process.c
volume_process_LDADD =  ../../src/audio/libaudio.a $(LDADD)

# buffer tests

check_PROGRAMS += buffer_new
buffer_new_SOURCES = src/audio/buffer/buffer_new.c src/audio/buffer/mock.c
buffer_new_LDADD =  ../../src/audio/libaudio.a $(LDADD)

check_PROGRAMS += buffer_write
buffer_write_SOURCES = src/audio/buffer/buffer_write.c src/audio/buffer/mock.c
buffer_write_LDADD =  ../../src/audio/libaudio.a $(LDADD)

check_PROGRAMS += buffer_wrap
buffer_wrap_SOURCES = src/audio/buffer/buffer_wrap.c src/audio/buffer/mock.c
buffer_wrap_LDADD =  ../../src/audio/libaudio.a $(LDADD)

check_PROGRAMS += buffer_copy
buffer_copy_SOURCES = src/audio/buffer/buffer_copy.c src/audio/buffer/mock.c
buffer_copy_LDADD =  ../../src/audio/libaudio.a $(LDADD)

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

check_PROGRAMS += ceil_divide
ceil_divide_SOURCES = src/math/numbers/ceil_divide.c
ceil_divide_LDADD = ../../src/math/libsof_math.a -lm $(LDADD)

check_PROGRAMS += find_equal_int16
find_equal_int16_SOURCES = src/math/numbers/find_equal_int16.c
find_equal_int16_LDADD = ../../src/math/libsof_math.a $(LDADD)

check_PROGRAMS += find_min_int16
find_min_int16_SOURCES = src/math/numbers/find_min_int16.c
find_min_int16_LDADD = ../../src/math/libsof_math.a $(LDADD)

check_PROGRAMS += find_max_abs_int32
find_max_abs_int32_SOURCES = src/math/numbers/find_max_abs_int32.c
find_max_abs_int32_LDADD = ../../src/math/libsof_math.a $(LDADD)

check_PROGRAMS += norm_int32
norm_int32_SOURCES = src/math/numbers/norm_int32.c
norm_int32_LDADD = ../../src/math/libsof_math.a $(LDADD)

check_PROGRAMS += sin_fixed
sin_fixed_SOURCES = src/math/trig/sin_fixed.c
sin_fixed_LDADD = ../../src/math/libsof_math.a $(LDADD)

# all our binaries are test cases
TESTS = $(check_PROGRAMS)