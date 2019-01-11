noinst_LIBRARIES = libcore.a libdma.a libdai.a libtplg.a

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
	pm_runtime.c \
	clk.c

libcore_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

libtplg_a_SOURCES = \
	topology.c

libtplg_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

libdma_a_SOURCES = \
	dma.c

libdma_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

libdai_a_SOURCES = \
	dai.c

libdai_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)
