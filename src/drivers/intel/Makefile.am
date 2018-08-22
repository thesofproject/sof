noinst_LTLIBRARIES = libdrivers.la

libdrivers_la_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)

libdrivers_la_SOURCES = dw-dma.c

if BUILD_CAVS
SUBDIRS = cavs
libdrivers_la_LIBADD = cavs/libdrivers.la
else
SUBDIRS = legacy
libdrivers_la_LIBADD = legacy/libdrivers.la
endif
