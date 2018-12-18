check_PROGRAMS =

LOG_DRIVER = env AM_TAP_AWK='$(AWK)' $(SHELL) $(top_srcdir)/tap-driver.sh

if BUILD_XTENSA
LOG_COMPILER = xt-run
endif

# cmocka needs stdlib
override LDFLAGS := $(filter-out -nostdlib,$(LDFLAGS))

# turn off optimization if You need to debug UT using Xplorer
#override CFLAGS := $(filter-out -O2,$(CFLAGS))

override AM_CFLAGS := \
	$(filter-out -nostdlib,$(AM_CFLAGS)) \
	$(SOF_INCDIR) \
	$(PLATFORM_INCDIR) \
	-DCHECK

override AM_LDFLAGS := \
	$(filter-out -nostdlib,$(AM_LDFLAGS))

AM_CFLAGS += -I./include

if HAVE_CMOCKA_PREFIX
AM_CFLAGS += -I$(CMOCKA_PREFIX)/include
AM_LDFLAGS += -L$(CMOCKA_PREFIX)/lib
endif

if BUILD_XTENSA
AM_CFLAGS += -I../../src/arch/xtensa/include
AM_CFLAGS += $(ARCH_INCDIR)
AM_CFLAGS += $(PLATFORM_INCDIR)
AM_CFLAGS += -I../../src/audio
endif

if BUILD_HOST
AM_CFLAGS += -I../../src/arch/host/include
endif

LDADD = -lcmocka

# generate linker script
LINK_SCRIPT = memory_mock.x

BUILT_SOURCES = $(LINK_SCRIPT)
CLEANFILES = $(LINK_SCRIPT)
$(LINK_SCRIPT): Makefile $(LINK_SCRIPT).in
	cat $(LINK_SCRIPT).in | $(CPP) -P $(PLATFORM_INCDIR) $(SOF_INCDIR) - >$@

if BUILD_XTENSA
AM_LDFLAGS += -T $(LINK_SCRIPT)
endif

# mixer tests
check_PROGRAMS += mixer
mixer_SOURCES = src/audio/mixer/mixer_test.c \
				src/audio/mixer/mock.c \
				src/audio/mixer/comp_mock.c \
				../../src/audio/buffer.c \
				../../src/audio/mixer.c

if BUILD_HOST
mixer_SOURCES += ../../src/host/trace.c
endif

mixer_LDADD = -lm $(LDADD)

# memory allocator test

if BUILD_XTENSA
check_PROGRAMS += alloc
alloc_SOURCES = src/lib/alloc/alloc.c src/lib/alloc/mock.c ../../src/lib/alloc.c ../../src/platform/intel/cavs/memory.c
endif

# pipeline tests

if BUILD_XTENSA

check_PROGRAMS += pipeline_new
pipeline_new_SOURCES = ../../src/audio/pipeline.c  src/audio/pipeline/pipeline_new.c src/audio/pipeline/pipeline_mocks.c src/audio/pipeline/pipeline_mocks_rzalloc.c

check_PROGRAMS += pipeline_new_allocation
pipeline_new_allocation_SOURCES = ../../src/audio/pipeline.c  src/audio/pipeline/pipeline_new_allocation.c src/audio/pipeline/pipeline_mocks.c src/audio/pipeline/pipeline_new_allocation_mocks.c

check_PROGRAMS += pipeline_connect_upstream
pipeline_connect_upstream_SOURCES = ../../src/audio/pipeline.c src/audio/pipeline/pipeline_mocks.c src/audio/pipeline/pipeline_connect_upstream.c src/audio/pipeline/pipeline_mocks_rzalloc.c src/audio/pipeline/pipeline_connection_mocks.c

check_PROGRAMS += pipeline_free
pipeline_free_SOURCES = ../../src/audio/pipeline.c src/audio/pipeline/pipeline_mocks.c src/audio/pipeline/pipeline_free.c src/audio/pipeline/pipeline_mocks_rzalloc.c src/audio/pipeline/pipeline_connection_mocks.c

endif

# lib/preproc tests

check_PROGRAMS += preproc_varargs_count
preproc_varargs_count_SOURCES =\
	src/lib/preproc/varargs_count.c

check_PROGRAMS += preproc_concat
preproc_concat_SOURCES =\
	src/lib/preproc/concat.c

check_PROGRAMS += preproc_seq
preproc_seq_SOURCES =\
	src/lib/preproc/seq.c

check_PROGRAMS += preproc_defer
preproc_defer_SOURCES =\
	src/lib/preproc/defer.c

check_PROGRAMS += preproc_get_arg
preproc_get_arg_SOURCES =\
	src/lib/preproc/get_arg.c

# debugability tests

check_PROGRAMS += debugability_macros
debugability_macros_SOURCES = src/debugability/macros.c

# lib/lib tests

check_PROGRAMS += rstrcmp
rstrcmp_SOURCES = src/lib/lib/rstrcmp.c
if BUILD_HOST
rstrcmp_SOURCES += ../../src/lib/lib.c
rstrcmp_LDADD = $(LDADD)
else
rstrcmp_LDADD = ../../src/lib/libcore.a $(LDADD)
endif

check_PROGRAMS += rstrlen
rstrlen_SOURCES = src/lib/lib/rstrlen.c
if BUILD_HOST
rstrlen_SOURCES += ../../src/lib/lib.c
rstrlen_LDADD = $(LDADD)
else
rstrlen_LDADD = ../../src/lib/libcore.a $(LDADD)
endif

check_PROGRAMS += strcheck
strcheck_SOURCES = src/lib/lib/strcheck.c
if BUILD_HOST
strcheck_SOURCES += ../../src/lib/lib.c
strcheck_LDADD = $(LDADD)
else
strcheck_LDADD = ../../src/lib/libcore.a $(LDADD)
endif

# volume tests

check_PROGRAMS += volume_process
volume_process_SOURCES = src/audio/volume/volume_process.c
volume_process_CFLAGS = -I../../src/audio $(AM_CFLAGS)
if BUILD_HOST
volume_process_SOURCES += ../../src/audio/volume.c \
			../../src/audio/volume_generic.c \
			../../src/audio/component.c \
			../../src/audio/buffer.c \
			../../src/audio/pipeline.c \
			../../src/ipc/ipc.c
volume_process_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
volume_process_LDADD =  ../../src/audio/libaudio.a $(LDADD)
endif

# buffer tests

check_PROGRAMS += buffer_new
buffer_new_SOURCES = src/audio/buffer/buffer_new.c src/audio/buffer/mock.c
if BUILD_HOST
buffer_new_SOURCES += 	../../src/audio/component.c \
			../../src/audio/buffer.c \
			../../src/audio/pipeline.c \
			../../src/ipc/ipc.c
buffer_new_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
buffer_new_LDADD =  ../../src/audio/libaudio.a $(LDADD)
endif

check_PROGRAMS += buffer_write
buffer_write_SOURCES = src/audio/buffer/buffer_write.c src/audio/buffer/mock.c
if BUILD_HOST
buffer_write_SOURCES += ../../src/audio/component.c \
			../../src/audio/buffer.c \
			../../src/audio/pipeline.c \
			../../src/ipc/ipc.c
buffer_write_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
buffer_write_LDADD =  ../../src/audio/libaudio.a $(LDADD)
endif

check_PROGRAMS += buffer_wrap
buffer_wrap_SOURCES = src/audio/buffer/buffer_wrap.c src/audio/buffer/mock.c
if BUILD_HOST
buffer_wrap_SOURCES += 	../../src/audio/component.c \
			../../src/audio/buffer.c \
			../../src/audio/pipeline.c \
			../../src/ipc/ipc.c
buffer_wrap_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
buffer_wrap_LDADD =  ../../src/audio/libaudio.a $(LDADD)
endif

check_PROGRAMS += buffer_copy
buffer_copy_SOURCES = src/audio/buffer/buffer_copy.c src/audio/buffer/mock.c
if BUILD_HOST
buffer_copy_SOURCES += 	../../src/audio/component.c \
			../../src/audio/buffer.c \
			../../src/audio/pipeline.c \
			../../src/ipc/ipc.c
buffer_copy_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
buffer_copy_LDADD =  ../../src/audio/libaudio.a $(LDADD)
endif

# component tests

check_PROGRAMS += comp_set_state
comp_set_state_SOURCES = src/audio/component/comp_set_state.c src/audio/component/mock.c
if BUILD_HOST
comp_set_state_SOURCES += 	../../src/audio/component.c \
			../../src/audio/buffer.c \
			../../src/audio/pipeline.c \
			../../src/ipc/ipc.c
comp_set_state_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
comp_set_state_LDADD = ../../src/audio/libaudio.a $(LDADD)
endif

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
if BUILD_HOST
gcd_SOURCES += 	../../src/math/numbers.c
gcd_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
gcd_LDADD = ../../src/math/libsof_math.a $(LDADD)
endif

check_PROGRAMS += ceil_divide
ceil_divide_SOURCES = src/math/numbers/ceil_divide.c
if BUILD_HOST
ceil_divide_SOURCES += 	../../src/math/numbers.c
ceil_divide_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
ceil_divide_LDADD = ../../src/math/libsof_math.a -lm $(LDADD)
endif

check_PROGRAMS += find_equal_int16
find_equal_int16_SOURCES = src/math/numbers/find_equal_int16.c
if BUILD_HOST
find_equal_int16_SOURCES += 	../../src/math/numbers.c
find_equal_int16_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
find_equal_int16_LDADD = ../../src/math/libsof_math.a $(LDADD)
endif

check_PROGRAMS += find_min_int16
find_min_int16_SOURCES = src/math/numbers/find_min_int16.c
if BUILD_HOST
find_min_int16_SOURCES += 	../../src/math/numbers.c
find_min_int16_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
find_min_int16_LDADD = ../../src/math/libsof_math.a $(LDADD)
endif

check_PROGRAMS += find_max_abs_int32
find_max_abs_int32_SOURCES = src/math/numbers/find_max_abs_int32.c
if BUILD_HOST
find_max_abs_int32_SOURCES += 	../../src/math/numbers.c
find_max_abs_int32_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
find_max_abs_int32_LDADD = ../../src/math/libsof_math.a $(LDADD)
endif

check_PROGRAMS += norm_int32
norm_int32_SOURCES = src/math/numbers/norm_int32.c
if BUILD_HOST
norm_int32_SOURCES += 	../../src/math/numbers.c
norm_int32_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
norm_int32_LDADD = ../../src/math/libsof_math.a $(LDADD)
endif

check_PROGRAMS += sin_fixed
sin_fixed_SOURCES = src/math/trig/sin_fixed.c
if BUILD_HOST
sin_fixed_SOURCES += 	../../src/math/numbers.c \
			../../src/math/trig.c
sin_fixed_LDADD =  ../../src/host/libtb_common.a $(LDADD) -ldl
else
sin_fixed_LDADD = ../../src/math/libsof_math.a $(LDADD)
endif

# all our binaries are test cases
TESTS = $(check_PROGRAMS)
