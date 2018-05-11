noinst_LIBRARIES = libdrivers.a

libdrivers_a_SOURCES = \
	dw-dma.c

if BUILD_BAYTRAIL
libdrivers_a_SOURCES += \
	byt-ssp.c
endif

if BUILD_CHERRYTRAIL
libdrivers_a_SOURCES += \
	byt-ssp.c
endif

if BUILD_HASWELL
libdrivers_a_SOURCES += \
	hsw-ssp.c
endif

if BUILD_BROADWELL
libdrivers_a_SOURCES += \
	hsw-ssp.c
endif

if BUILD_APOLLOLAKE
libdrivers_a_SOURCES += \
	apl-ssp.c \
	hda-dma.c \
	dmic.c
endif

if BUILD_CANNONLAKE
libdrivers_a_SOURCES += \
	apl-ssp.c \
	hda-dma.c \
	dmic.c
endif

libdrivers_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR)
