noinst_LTLIBRARIES = libdrivers.la

libdrivers_la_SOURCES = \
	clk.c \
	dmic.c \
	hda-dma.c \
	hda.c \
	interrupt.c \
	pm_runtime.c \
	ssp.c \
	timer.c

libdrivers_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)

if BUILD_APOLLOLAKE

libdrivers_la_CCASFLAGS = \
	$(ARCH_INCDIR) \
	$(ASFLAGS) \
	$(ARCH_ASFLAGS) \
	$(PLATFORM_INCDIR)

noinst_HEADERS = apollolake/power_down.h

libdrivers_la_SOURCES += apollolake/power_down.S

endif