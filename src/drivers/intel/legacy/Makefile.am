noinst_LTLIBRARIES = libdrivers.la

libdrivers_la_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)

# This lib just includes lib for specific platform
libdrivers_la_SOURCES =

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
