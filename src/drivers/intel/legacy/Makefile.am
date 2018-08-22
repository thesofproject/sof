noinst_LTLIBRARIES = libdrivers.la

libdrivers_la_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)

libdrivers_la_SOURCES =

if BUILD_BAYTRAIL
libdrivers_la_SOURCES += baytrail/ssp.c
endif

if BUILD_CHERRYTRAIL
libdrivers_la_SOURCES += baytrail/ssp.c
endif

if BUILD_HASWELL
libdrivers_la_SOURCES += haswell/ssp.c
endif

if BUILD_BROADWELL
libdrivers_la_SOURCES += haswell/ssp.c
endif
