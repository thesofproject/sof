noinst_LTLIBRARIES = libdrivers.la

libdrivers_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)

libdrivers_la_SOURCES = dw-dma.c

if BUILD_CAVS

SUBDIRS = cavs
libdrivers_la_LIBADD = cavs/libdrivers.la

if BUILD_SUECREEK
libdrivers_la_SOURCES += sue-gpio.c
endif

else

if BUILD_BAYTRAIL
SUBDIRS = baytrail
libdrivers_la_LIBADD = baytrail/libdrivers.la
endif

if BUILD_CHERRYTRAIL
SUBDIRS = baytrail
libdrivers_la_LIBADD = baytrail/libdrivers.la
endif

if BUILD_HASWELL
SUBDIRS = haswell
libdrivers_la_LIBADD = haswell/libdrivers.la
endif

if BUILD_BROADWELL
SUBDIRS = haswell
libdrivers_la_LIBADD = haswell/libdrivers.la
endif

endif
