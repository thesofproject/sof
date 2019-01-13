noinst_LTLIBRARIES = libdrivers.la

libdrivers_la_SOURCES = \
	interrupt.c \
	ssp.c \
	ipc.c \
	timer.c

libdrivers_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)