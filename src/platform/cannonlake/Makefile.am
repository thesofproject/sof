SUBDIRS = include

EXTRA_DIST = \
	cannonlake.x.in \
	boot_ldr.x.in

noinst_LIBRARIES = libplatform.a

libplatform_a_SOURCES = \
	platform.c \
	dai.c \
	dma.c \
	clk.c \
	timer.c \
	interrupt.c \
	memory.c \
	pm_runtime.c

libplatform_a_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(ARCH_INCDIR) \
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