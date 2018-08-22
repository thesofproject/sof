SUBDIRS = include

EXTRA_DIST = baytrail.x.in

noinst_LTLIBRARIES = libplatform.la

libplatform_la_SOURCES = \
	interrupt.c \
	platform.c \
	dai.c \
	dma.c \
	clk.c \
	timer.c \
	memory.c

libplatform_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)
