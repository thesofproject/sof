noinst_LTLIBRARIES = libcavsplatform.la

libcavsplatform_la_SOURCES = \
	platform.c \
	dai.c \
	dma.c \
	timer.c \
	memory.c \
	pm_runtime.c

libcavsplatform_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)
