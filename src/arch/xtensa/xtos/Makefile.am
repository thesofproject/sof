lib_LIBRARIES = \
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
lib_LIBRARIES += libxlevel6.a
VECTOR_DEFS += -D__SPLIT__level6
endif

if BUILD_BROADWELL
PLATFORM_DEFS = $(VECTOR_DEFS)
endif

if BUILD_HASWELL
PLATFORM_DEFS = $(VECTOR_DEFS)
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
	exc-alloca-handler.S \
	exc-c-wrapper-handler.S \
	exc-return.S \
	exc-sethandler.c \
	exc-syscall-c-handler.c \
	exc-syscall-handler.S \
	exc-table.S \
	exc-unhandled.S \
	init.c \
	interrupt-table.S \
	int-sethandler.c \
	intlevel-restore.S \
	intlevel-setmin.S \
	intlevel-set.S \
	int-lowpri-dispatcher.S \
	ints-off.S \
	ints-on.S \
	kernel-vector.S \
	memep-enable.S \
	memep-initrams.S \
	memerror-vector.S \
	nmi-vector.S \
	reloc-vectors.S \
	user-vector.S \
	window-vectors.S

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
