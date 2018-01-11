noinst_LIBRARIES = libaudio.a

noinst_HEADERS = \
	eq_fir.h \
	eq_iir.h \
	fir.h \
	iir.h \
	src_config.h \
	src_core.h

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
	host.c \
	pipeline.c \
	pipeline_static.c \
	component.c \
	buffer.c

libaudio_a_CFLAGS = \
	$(ARCH_CFLAGS) \
	$(REEF_INCDIR) \
	$(ARCH_INCDIR) \
	$(PLATFORM_INCDIR)
