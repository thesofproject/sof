noinst_LIBRARIES = libcore.a

libcore_a_SOURCES = \
	lib.c \
	alloc.c \
	work.c \
	notifier.c \
	trace.c \
	schedule.c \
	agent.c \
	interrupt.c \
	dma-trace.c \
	module.c

libcore_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(SOF_INCDIR) \
	$(PLATFORM_INCDIR)
