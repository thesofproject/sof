lib_LIBRARIES = libxtos.a

VECTOR_DEFS = \
	-D__SPLIT__vector \
	-D__SPLIT__handler \
	-D__SPLIT__level1int \
	-D__SPLIT__user \
	-D__SPLIT__level1int \
	-D__SPLIT__level2 \
	-D__SPLIT__level3 \
	-D__SPLIT__level4 \
	-D__SPLIT__level5 \
	-D__SPLIT__level6


if HAVE_BAYTRAIL
PLATFORM_DEFS =
else
PLATFORM_DEFS =
endif

libxtos_a_SOURCES = \
	exc-syscall-handler.S \
	exc-syscall-c-handler.c \
	exc-sethandler.c \
	exc-table.S \
	exc-unhandled.S \
	exc-c-wrapper-handler.S \
	exc-return.S \
	user-vector.S \
	int-lowpri-dispatcher.S \
	interrupt-table.S \
	int-sethandler.c \
	ints-on.S \
	ints-off.S

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
