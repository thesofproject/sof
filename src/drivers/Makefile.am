SUBDIRS = intel

noinst_LTLIBRARIES = libdrivers.la

libdrivers_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)

# This lib is just composition of convenience libraries
libdrivers_la_SOURCES =

libdrivers_la_LIBADD = intel/libdrivers.la
