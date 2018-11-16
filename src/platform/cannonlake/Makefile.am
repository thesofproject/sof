SUBDIRS = include

EXTRA_DIST = \
	cannonlake.x.in \
	boot_ldr.x.in

noinst_LTLIBRARIES = libplatform.la

libplatform_la_LIBADD = ../intel/cavs/libcavsplatform.la

libplatform_la_SOURCES = power_down.S

libplatform_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

libplatform_la_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ASFLAGS) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

noinst_PROGRAMS = module boot_module

module_SOURCES = \
	base_module.c

module_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

boot_module_SOURCES = \
	boot_module.c

boot_module_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)
