noinst_HEADERS = \
	xtos-internal.h \
	xtos-params.h \
	xtos-structs.h \
	interrupt-pri.h \
	int-highpri-dispatcher.S \
	int-medpri-dispatcher.S

noinst_LIBRARIES = \
	libxtos.a \
	libxlevel2.a \
	libxlevel3.a \
	libxlevel4.a \
	libxlevel5.a

VECTOR_DEFS = \
	-D__SPLIT__vector \
	-D__SPLIT__handler \
	-D__SPLIT__user \
	-D__SPLIT__level1int \
	-D__SPLIT__level2 \
	-D__SPLIT__level3 \
	-D__SPLIT__level4 \
	-D__SPLIT__level5


if BUILD_BAYTRAIL
PLATFORM_DEFS = $(VECTOR_DEFS)
endif

if BUILD_CHERRYTRAIL
PLATFORM_DEFS = $(VECTOR_DEFS)
endif

if BUILD_APOLLOLAKE
PLATFORM_DEFS = $(VECTOR_DEFS)
noinst_LIBRARIES += libxlevel6.a
VECTOR_DEFS += -D__SPLIT__level6
endif

if BUILD_BROADWELL
PLATFORM_DEFS = $(VECTOR_DEFS)
endif

if BUILD_HASWELL
PLATFORM_DEFS = $(VECTOR_DEFS)
endif

if BUILD_CANNONLAKE
PLATFORM_DEFS = $(VECTOR_DEFS)
noinst_LIBRARIES += libxlevel6.a
VECTOR_DEFS += -D__SPLIT__level6
endif

if BUILD_ICELAKE
PLATFORM_DEFS = $(VECTOR_DEFS)
noinst_LIBRARIES += libxlevel6.a
VECTOR_DEFS += -D__SPLIT__level6
endif

if BUILD_SUECREEK
PLATFORM_DEFS = $(VECTOR_DEFS)
noinst_LIBRARIES += libxlevel6.a
VECTOR_DEFS += -D__SPLIT__level6
endif

LEVEL_SRC = \
	int-handler.S \
	int-vector.S \
	int-initlevel.S

libxlevel2_a_SOURCES = \
	$(LEVEL_SRC)

libxlevel2_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	$(PLATFORM_DEFS) \
	-D_INTERRUPT_LEVEL=2

libxlevel3_a_SOURCES = \
	$(LEVEL_SRC)

libxlevel3_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	$(PLATFORM_DEFS) \
	-D_INTERRUPT_LEVEL=3

libxlevel4_a_SOURCES = \
	$(LEVEL_SRC)

libxlevel4_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	$(PLATFORM_DEFS) \
	-D_INTERRUPT_LEVEL=4

libxlevel5_a_SOURCES = \
	$(LEVEL_SRC)

libxlevel5_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	$(PLATFORM_DEFS) \
	-D_INTERRUPT_LEVEL=5

if BUILD_APOLLOLAKE
libxlevel6_a_SOURCES = \
	$(LEVEL_SRC)

libxlevel6_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	$(PLATFORM_DEFS) \
	-D_INTERRUPT_LEVEL=6
endif

libxtos_a_SOURCES = \
	core-restore.S \
	core-save.S \
	core-shutoff.S \
	double-vector.S \
	xea1/exc-alloca-handler.S \
	xea1/exc-c-wrapper-handler.S \
	xea2/exc-c-wrapper-handler.S \
	xea1/exc-return.S \
	xea2/exc-return.S \
	exc-sethandler.c \
	exc-syscall-handler.S \
	exc-table.S \
	exc-unhandled.S \
	interrupt-table.S \
	int-sethandler.c \
	xea1/intlevel-restore.S \
	xea2/intlevel-restore.S \
	xea1/intlevel-setmin.S \
	xea2/intlevel-setmin.S \
	xea1/intlevel-set.S \
	xea2/intlevel-set.S \
	xea1/int-lowpri-dispatcher.S \
	xea2/int-lowpri-dispatcher.S \
	ints-off.S \
	ints-on.S \
	kernel-vector.S \
	memep-enable.S \
	memep-initrams.S \
	memerror-vector.S \
	nmi-vector.S \
	xea2/reloc-vectors.S \
	user-vector.S \
	xea1/window-vectors.S \
	xea2/window-vectors.S

libxtos_a_CFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(PLATFORM_DEFS)

libxtos_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	$(PLATFORM_DEFS)
