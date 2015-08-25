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
	$(ARCH_INCDIR) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR)

libxtos_a_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR)
