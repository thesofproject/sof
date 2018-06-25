noinst_LIBRARIES = libcore.a libdma.a

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
	pm_runtime.c

libcore_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

libdma_a_SOURCES = \
	dma.c

libdma_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)
