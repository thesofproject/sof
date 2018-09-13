noinst_LTLIBRARIES = libcavsplatform.la

libcavsplatform_la_SOURCES = \
	platform.c \
	dai.c \
	dma.c \
	memory.c

libcavsplatform_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)
