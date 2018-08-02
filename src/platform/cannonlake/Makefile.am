SUBDIRS = include

EXTRA_DIST = \
	cannonlake.x.in \
	boot_ldr.x.in

noinst_LIBRARIES = libplatform.a

libplatform_a_SOURCES = \
	../intel/cavs/platform.c \
	../intel/cavs/dai.c \
	../intel/cavs/dma.c \
	../intel/cavs/clk.c \
	../intel/cavs/timer.c \
	../intel/cavs/interrupt.c \
	../intel/cavs/memory.c \
	../intel/cavs/pm_runtime.c

libplatform_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

noinst_PROGRAMS = module boot_module

module_SOURCES = \
	base_module.c

module_CFLAGS = \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)

boot_module_SOURCES = \
	boot_module.c

boot_module_CFLAGS = \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR)