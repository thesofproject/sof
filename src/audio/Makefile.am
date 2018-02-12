noinst_LIBRARIES = libaudio.a

libaudio_a_SOURCES = \
	eq_iir.c \
	iir.c \
	eq_fir.c \
	fir.c \
	tone.c \
	src.c \
	src_core.c \
	mixer.c \
	mux.c \
	volume.c \
	switch.c \
	dai.c \
	pipeline.c \
	pipeline_static.c \
	component.c \
	buffer.c

if BUILD_BAYTRAIL
libaudio_a_SOURCES += \
	host.c
endif

if BUILD_CHERRYTRAIL
libaudio_a_SOURCES += \
	host.c
endif

if BUILD_APOLLOLAKE
libaudio_a_SOURCES += \
	host_gw.c
endif

if BUILD_BROADWELL
libaudio_a_SOURCES += \
	host.c
endif

if BUILD_HASWELL
libaudio_a_SOURCES += \
	host.c
endif

if BUILD_SUECREEK
libaudio_a_SOURCES += \
	host.c
endif

if BUILD_CANNONLAKE
libaudio_a_SOURCES += \
	host_gw.c
endif

libaudio_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(REEF_INCDIR) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR)
