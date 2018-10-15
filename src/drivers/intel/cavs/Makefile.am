noinst_LTLIBRARIES = libdrivers.la

libdrivers_la_SOURCES = \
	dmic.c \
	hda-dma.c \
	hda.c \
	interrupt.c \
	ssp.c \
	timer.c

libdrivers_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)
