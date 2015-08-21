if HAVE_BAYTRAIL
PLATFORM_SRC = 
else
PLATFORM_SRC =
endif


lib_LIBRARIES = libxtos.a

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
	interrupt-table.S

libxtos_a_CFLAGS = \
	$(XTENSA_CFLAGS) \
	$(XTENSA_INCDIR)

libxtos_a_CCASFLAGS = \
	$(XTENSA_INCDIR) \
	$(XTENSA_ASFLAGS)
