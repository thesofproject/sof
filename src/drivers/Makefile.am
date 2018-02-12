noinst_LIBRARIES = libdrivers.a

libdrivers_a_SOURCES = \
	dw-dma.c
if BUILD_APL_SSP
libdrivers_a_SOURCES += \
	apl-ssp.c
else
libdrivers_a_SOURCES += \
	ssp.c
endif

if BUILD_SUECREEK
libdrivers_a_SOURCES += \
	dw-uart.c \
	spi.c
endif

libdrivers_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(REEF_INCDIR) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR)
