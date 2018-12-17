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
noinst_LTLIBRARIES += libboot_drv.la

libdrivers_la_SOURCES += dw-intc.c dw-uart.c dw-uart-write-word.c sue-gpio.c
libboot_drv_la_SOURCES = dw-uart-ll.c dw-uart-write-word.c

libboot_drv_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)
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
