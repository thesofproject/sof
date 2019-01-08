noinst_LTLIBRARIES = libdrivers.la

libdrivers_la_SOURCES = \
	dmic.c \
	hda-dma.c \
	hda.c \
	interrupt.c \
	ssp.c \
	timer.c

if BUILD_APOLLOLAKE
libdrivers_la_SOURCES += \
	ipc.c
endif

if BUILD_CANNONLAKE
libdrivers_la_SOURCES += \
	ipc.c
endif

if BUILD_ICELAKE
libdrivers_la_SOURCES += \
	ipc.c
endif

if BUILD_SUECREEK
libdrivers_la_SOURCES += \
	sue-ipc.c
endif

libdrivers_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS) \
	$(PLATFORM_INCDIR) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR)
