noinst_LIBRARIES = libdrivers.a

libdrivers_a_SOURCES = \
	dw-dma.c

if BUILD_APL_SSP
libdrivers_a_SOURCES += \
	apl-ssp.c
else
libdrivers_a_SOURCES += \
	byt-ssp.c
endif

if BUILD_APOLLOLAKE
libdrivers_a_SOURCES += \
	hda-dma.c
endif

if BUILD_CANNONLAKE
libdrivers_a_SOURCES += \
	hda-dma.c
endif

libdrivers_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(SOF_INCDIR) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR)
