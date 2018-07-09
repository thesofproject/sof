includedir = $(prefix)/include/sof/audio

include_HEADERS = \
	eq_iir.h \
	iir.h \
	fir.h \
	src_config.h \
	src.h \
	eq_fir.h \
	volume.h

COMP_SRC = \
	eq_iir.c \
	iir.c \
	eq_fir.c \
	fir.c \
	tone.c \
	src.c \
	src_generic.c \
	mixer.c \
	mux.c \
	volume.c \
	volume_generic.c \
	switch.c \
	dai.c \
	host.c \
	pipeline.c \
	component.c \
	buffer.c

SOF_SRC = \
	dai.c \
	host.c \
	pipeline.c \
	component.c \
	buffer.c

SRC_SRC = \
	src.c \
	src_generic.c

EQ_FIR_SRC = \
	eq_fir.c \
	fir.c

EQ_IIR_SRC = \
	eq_iir.c \
	iir.c

VOLUME_SRC = \
	volume.c \
	volume_generic.c

# common compiler flags for libs
lib_cflags = \
	$(AM_CFLAGS) \
	$(ARCH_CFLAGS)

if BUILD_LIB

# only host builds shared libraries, the rest are static
if BUILD_HOST

# common linker flags for host libs
host_lib_ldflags = \
	$(AM_LDFLAGS) \
	-version-info `echo $(VERSION) | cut -d '.' -f 1` \
	-no-undefined \
	-export-dynamic

# libsof
lib_LTLIBRARIES  = libsof.la

libsof_la_SOURCES = $(SOF_SRC)

libsof_la_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

libsof_la_LDFLAGS = $(host_lib_ldflags)

# libsof_src
lib_LTLIBRARIES  += libsof_src.la

libsof_src_la_SOURCES = $(SRC_SRC)

libsof_src_la_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

libsof_src_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_fir
lib_LTLIBRARIES  += libsof_eq_fir.la

libsof_eq_fir_la_SOURCES = $(EQ_FIR_SRC)

libsof_eq_fir_la_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

libsof_eq_fir_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_iir
lib_LTLIBRARIES  += libsof_eq_iir.la

libsof_eq_iir_la_SOURCES = $(EQ_IIR_SRC)

libsof_eq_iir_la_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

libsof_eq_iir_la_LDFLAGS = $(host_lib_ldflags)

# libsof_volume
lib_LTLIBRARIES  += libsof_volume.la

libsof_volume_la_SOURCES = $(VOLUME_SRC)

libsof_volume_la_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

libsof_volume_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mux
lib_LTLIBRARIES  += libsof_mux.la

libsof_mux_la_SOURCES = mux.c

libsof_mux_la_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

libsof_mux_la_LDFLAGS = $(host_lib_ldflags)

# libsof_switch
lib_LTLIBRARIES  += libsof_switch.la

libsof_switch_la_SOURCES = switch.c

libsof_switch_la_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

libsof_switch_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mixer
lib_LTLIBRARIES  += libsof_mixer.la

libsof_mixer_la_SOURCES = mixer.c

libsof_mixer_la_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

libsof_mixer_la_LDFLAGS = $(host_lib_ldflags)

# libsof_tone
lib_LTLIBRARIES  += libsof_tone.la

libsof_tone_la_SOURCES = tone.c

libsof_tone_la_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

libsof_tone_la_LDFLAGS = $(host_lib_ldflags)

if HAVE_SSE42
# libsof
lib_LTLIBRARIES  += libsof_sse42.la

libsof_sse42_la_SOURCES = $(SOF_SRC)

libsof_sse42_la_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

libsof_sse42_la_LDFLAGS = $(host_lib_ldflags)

# libsof_src
lib_LTLIBRARIES  += libsof_src_sse42.la

libsof_src_sse42_la_SOURCES = $(SRC_SRC)

libsof_src_sse42_la_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

libsof_src_sse42_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_fir
lib_LTLIBRARIES  += libsof_eq_fir_sse42.la

libsof_eq_fir_sse42_la_SOURCES = $(EQ_FIR_SRC)

libsof_eq_fir_sse42_la_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

libsof_eq_fir_sse42_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_iir
lib_LTLIBRARIES  += libsof_eq_iir_sse42.la

libsof_eq_iir_sse42_la_SOURCES = $(EQ_IIR_SRC)

libsof_eq_iir_sse42_la_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

libsof_eq_iir_sse42_la_LDFLAGS = $(host_lib_ldflags)

# libsof_volume
lib_LTLIBRARIES  += libsof_volume_sse42.la

libsof_volume_sse42_la_SOURCES = $(VOLUME_SRC)

libsof_volume_sse42_la_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

libsof_volume_sse42_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mux
lib_LTLIBRARIES  += libsof_mux_sse42.la

libsof_mux_sse42_la_SOURCES = mux.c

libsof_mux_sse42_la_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

libsof_mux_sse42_la_LDFLAGS = $(host_lib_ldflags)

# libsof_switch
lib_LTLIBRARIES  += libsof_switch_sse42.la

libsof_switch_sse42_la_SOURCES = switch.c

libsof_switch_sse42_la_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

libsof_switch_sse42_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mixer
lib_LTLIBRARIES  += libsof_mixer_sse42.la

libsof_mixer_sse42_la_SOURCES = mixer.c

libsof_mixer_sse42_la_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

libsof_mixer_sse42_la_LDFLAGS = $(host_lib_ldflags)

# libsof_tone
lib_LTLIBRARIES  += libsof_tone_sse42.la

libsof_tone_sse42_la_SOURCES = tone.c

libsof_tone_sse42_la_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

libsof_tone_sse42_la_LDFLAGS = $(host_lib_ldflags)
endif

if HAVE_AVX
# libsof
lib_LTLIBRARIES  += libsof_avx.la

libsof_avx_la_SOURCES = $(SOF_SRC)

libsof_avx_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX_CFLAGS) \
	$(COMMON_INCDIR)

libsof_avx_la_LDFLAGS = $(host_lib_ldflags)

# libsof_src
lib_LTLIBRARIES  += libsof_src_avx.la

libsof_src_avx_la_SOURCES = $(SRC_SRC)

libsof_src_avx_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX_CFLAGS) \
	$(COMMON_INCDIR)

libsof_src_avx_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_fir
lib_LTLIBRARIES  += libsof_eq_fir_avx.la

libsof_eq_fir_avx_la_SOURCES = $(EQ_FIR_SRC)

libsof_eq_fir_avx_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX_CFLAGS) \
	$(COMMON_INCDIR)

libsof_eq_fir_avx_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_iir
lib_LTLIBRARIES  += libsof_eq_iir_avx.la

libsof_eq_iir_avx_la_SOURCES = $(EQ_IIR_SRC)

libsof_eq_iir_avx_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX_CFLAGS) \
	$(COMMON_INCDIR)

libsof_eq_iir_avx_la_LDFLAGS = $(host_lib_ldflags)

# libsof_volume
lib_LTLIBRARIES  += libsof_volume_avx.la

libsof_volume_avx_la_SOURCES = $(VOLUME_SRC)

libsof_volume_avx_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX_CFLAGS) \
	$(COMMON_INCDIR)

libsof_volume_avx_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mux
lib_LTLIBRARIES  += libsof_mux_avx.la

libsof_mux_avx_la_SOURCES = mux.c

libsof_mux_avx_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX_CFLAGS) \
	$(COMMON_INCDIR)

libsof_mux_avx_la_LDFLAGS = $(host_lib_ldflags)

# libsof_switch
lib_LTLIBRARIES  += libsof_switch_avx.la

libsof_switch_avx_la_SOURCES = switch.c

libsof_switch_avx_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX_CFLAGS) \
	$(COMMON_INCDIR)

libsof_switch_avx_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mixer
lib_LTLIBRARIES  += libsof_mixer_avx.la

libsof_mixer_avx_la_SOURCES = mixer.c

libsof_mixer_avx_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX_CFLAGS) \
	$(COMMON_INCDIR)

libsof_mixer_avx_la_LDFLAGS = $(host_lib_ldflags)

# libsof_tone
lib_LTLIBRARIES  += libsof_tone_avx.la

libsof_tone_avx_la_SOURCES = tone.c

libsof_tone_avx_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX_CFLAGS) \
	$(COMMON_INCDIR)

libsof_tone_avx_la_LDFLAGS = $(host_lib_ldflags)
endif

if HAVE_AVX2
# libsof
lib_LTLIBRARIES  += libsof_avx2.la

libsof_avx2_la_SOURCES = $(SOF_SRC)

libsof_avx2_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX2_CFLAGS) \
	$(COMMON_INCDIR)

libsof_avx2_la_LDFLAGS = $(host_lib_ldflags)

# libsof_src
lib_LTLIBRARIES  += libsof_src_avx2.la

libsof_src_avx2_la_SOURCES = $(SRC_SRC)

libsof_src_avx2_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX2_CFLAGS) \
	$(COMMON_INCDIR)

libsof_src_avx2_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_fir
lib_LTLIBRARIES  += libsof_eq_fir_avx2.la

libsof_eq_fir_avx2_la_SOURCES = $(EQ_FIR_SRC)

libsof_eq_fir_avx2_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX2_CFLAGS) \
	$(COMMON_INCDIR)

libsof_eq_fir_avx2_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_iir
lib_LTLIBRARIES  += libsof_eq_iir_avx2.la

libsof_eq_iir_avx2_la_SOURCES = $(EQ_IIR_SRC)

libsof_eq_iir_avx2_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX2_CFLAGS) \
	$(COMMON_INCDIR)

libsof_eq_iir_avx2_la_LDFLAGS = $(host_lib_ldflags)

# libsof_volume
lib_LTLIBRARIES  += libsof_volume_avx2.la

libsof_volume_avx2_la_SOURCES = $(VOLUME_SRC)

libsof_volume_avx2_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX2_CFLAGS) \
	$(COMMON_INCDIR)

libsof_volume_avx2_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mux
lib_LTLIBRARIES  += libsof_mux_avx2.la

libsof_mux_avx2_la_SOURCES = mux.c

libsof_mux_avx2_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX2_CFLAGS) \
	$(COMMON_INCDIR)

libsof_mux_avx2_la_LDFLAGS = $(host_lib_ldflags)

# libsof_switch
lib_LTLIBRARIES  += libsof_switch_avx2.la

libsof_switch_avx2_la_SOURCES = switch.c

libsof_switch_avx2_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX2_CFLAGS) \
	$(COMMON_INCDIR)

libsof_switch_avx2_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mixer
lib_LTLIBRARIES  += libsof_mixer_avx2.la

libsof_mixer_avx2_la_SOURCES = mixer.c

libsof_mixer_avx2_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX2_CFLAGS) \
	$(COMMON_INCDIR)

libsof_mixer_avx2_la_LDFLAGS = $(host_lib_ldflags)

# libsof_tone
lib_LTLIBRARIES  += libsof_tone_avx2.la

libsof_tone_avx2_la_SOURCES = tone.c

libsof_tone_avx2_la_CFLAGS = \
	$(lib_cflags) \
	$(AVX2_CFLAGS) \
	$(COMMON_INCDIR)

libsof_tone_avx2_la_LDFLAGS = $(host_lib_ldflags)

endif

if HAVE_FMA
# libsof
lib_LTLIBRARIES  += libsof_fma.la

libsof_fma_la_SOURCES = $(SOF_SRC)

libsof_fma_la_CFLAGS = \
	$(lib_cflags) \
	$(FMA_CFLAGS) \
	$(COMMON_INCDIR)

libsof_fma_la_LDFLAGS = $(host_lib_ldflags)

# libsof_src
lib_LTLIBRARIES  += libsof_src_fma.la

libsof_src_fma_la_SOURCES = $(SRC_SRC)

libsof_src_fma_la_CFLAGS = \
	$(lib_cflags) \
	$(FMA_CFLAGS) \
	$(COMMON_INCDIR)

libsof_src_fma_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_fir
lib_LTLIBRARIES  += libsof_eq_fir_fma.la

libsof_eq_fir_fma_la_SOURCES = $(EQ_FIR_SRC)

libsof_eq_fir_fma_la_CFLAGS = \
	$(lib_cflags) \
	$(FMA_CFLAGS) \
	$(COMMON_INCDIR)

libsof_eq_fir_fma_la_LDFLAGS = $(host_lib_ldflags)

# libsof_eq_iir
lib_LTLIBRARIES  += libsof_eq_iir_fma.la

libsof_eq_iir_fma_la_SOURCES = $(EQ_IIR_SRC)

libsof_eq_iir_fma_la_CFLAGS = \
	$(lib_cflags) \
	$(FMA_CFLAGS) \
	$(COMMON_INCDIR)

libsof_eq_iir_fma_la_LDFLAGS = $(host_lib_ldflags)

# libsof_volume
lib_LTLIBRARIES  += libsof_volume_fma.la

libsof_volume_fma_la_SOURCES = $(VOLUME_SRC)

libsof_volume_fma_la_CFLAGS = \
	$(lib_cflags) \
	$(FMA_CFLAGS) \
	$(COMMON_INCDIR)

libsof_volume_fma_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mux
lib_LTLIBRARIES  += libsof_mux_fma.la

libsof_mux_fma_la_SOURCES = mux.c

libsof_mux_fma_la_CFLAGS = \
	$(lib_cflags) \
	$(FMA_CFLAGS) \
	$(COMMON_INCDIR)

libsof_mux_fma_la_LDFLAGS = $(host_lib_ldflags)

# libsof_switch
lib_LTLIBRARIES  += libsof_switch_fma.la

libsof_switch_fma_la_SOURCES = switch.c

libsof_switch_fma_la_CFLAGS = \
	$(lib_cflags) \
	$(FMA_CFLAGS) \
	$(COMMON_INCDIR)

libsof_switch_fma_la_LDFLAGS = $(host_lib_ldflags)

# libsof_mixer
lib_LTLIBRARIES  += libsof_mixer_fma.la

libsof_mixer_fma_la_SOURCES = mixer.c

libsof_mixer_fma_la_CFLAGS = \
	$(lib_cflags) \
	$(FMA_CFLAGS) \
	$(COMMON_INCDIR)

libsof_mixer_fma_la_LDFLAGS = $(host_lib_ldflags)

# libsof_tone
lib_LTLIBRARIES  += libsof_tone_fma.la

libsof_tone_fma_la_SOURCES = tone.c

libsof_tone_fma_la_CFLAGS = \
	$(lib_cflags) \
	$(FMA_CFLAGS) \
	$(COMMON_INCDIR)

libsof_tone_fma_la_LDFLAGS = $(host_lib_ldflags)
endif

else

# Build for non host targets

# libsof
lib_LIBRARIES  = libsof.a

libsof_a_SOURCES = $(SOF_SRC)

libsof_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

# libsof_src
lib_LIBRARIES  += libsof_src.a

libsof_src_a_SOURCES = $(SRC_SRC)

libsof_src_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

# libsof_eq_fir
lib_LIBRARIES  += libsof_eq_fir.a

libsof_eq_fir_a_SOURCES = $(EQ_FIR_SRC)

libsof_eq_fir_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

# libsof_eq_iir
lib_LIBRARIES  += libsof_eq_iir.a

libsof_eq_iir_a_SOURCES = $(EQ_IIR_SRC)

libsof_eq_iir_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

# libsof_volume
lib_LIBRARIES  += libsof_volume.a

libsof_volume_a_SOURCES = $(VOLUME_SRC)

libsof_volume_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

# libsof_mux
lib_LIBRARIES  += libsof_mux.a

libsof_mux_a_SOURCES = mux.c

libsof_mux_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

# libsof_switch
lib_LIBRARIES  += libsof_switch.a

libsof_switch_a_SOURCES = switch.c

libsof_switch_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

# libsof_mixer
lib_LIBRARIES  += libsof_mixer.a

libsof_mixer_a_SOURCES = mixer.c

libsof_mixer_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

# libsof_tone
lib_LIBRARIES  += libsof_tone.a

libsof_tone_a_SOURCES = tone.c

libsof_tone_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)


if HAVE_HIFI2EP
# libsof
lib_LIBRARIES  += libsof_hifi2ep.a

libsof_hifi2ep_a_SOURCES = $(SOF_SRC)

libsof_hifi2ep_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_src
lib_LIBRARIES  += libsof_src_hifi2ep.a

libsof_src_hifi2ep_a_SOURCES = $(SRC_SRC)

libsof_src_hifi2ep_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_eq_fir
lib_LIBRARIES  += libsof_eq_fir_hifi2ep.a

libsof_eq_fir_hifi2ep_a_SOURCES = $(EQ_FIR_SRC)

libsof_eq_fir_hifi2ep_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_eq_iir
lib_LIBRARIES  += libsof_eq_iir_hifi2ep.a

libsof_eq_iir_hifi2ep_a_SOURCES = $(EQ_IIR_SRC)

libsof_eq_iir_hifi2ep_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_volume
lib_LIBRARIES  += libsof_volume_hifi2ep.a

libsof_volume_hifi2ep_a_SOURCES = $(VOLUME_SRC)

libsof_volume_hifi2ep_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_mux
lib_LIBRARIES  += libsof_mux_hifi2ep.a

libsof_mux_hifi2ep_a_SOURCES = mux.c

libsof_mux_hifi2ep_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_switch
lib_LIBRARIES  += libsof_switch_hifi2ep.a

libsof_switch_hifi2ep_a_SOURCES = switch.c

libsof_switch_hifi2ep_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_mixer
lib_LIBRARIES  += libsof_mixer_hifi2ep.a

libsof_mixer_hifi2ep_a_SOURCES = mixer.c

libsof_mixer_hifi2ep_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_tone
lib_LIBRARIES  += libsof_tone_hifi2ep.a

libsof_tone_hifi2ep_a_SOURCES = tone.c

libsof_tone_hifi2ep_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)
endif

if HAVE_HIFI3
# libsof
lib_LIBRARIES  += libsof_hifi3.a

libsof_hifi3_a_SOURCES = $(COMP_SRC)

libsof_hifi3_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_src
lib_LIBRARIES  += libsof_src_hifi3.a

libsof_src_hifi3_a_SOURCES = $(SRC_SRC)

libsof_src_hifi3_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_eq_fir
lib_LIBRARIES  += libsof_eq_fir_hifi3.a

libsof_eq_fir_hifi3_a_SOURCES = $(EQ_FIR_SRC)

libsof_eq_fir_hifi3_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_eq_iir
lib_LIBRARIES  += libsof_eq_iir_hifi3.a

libsof_eq_iir_hifi3_a_SOURCES = $(EQ_IIR_SRC)

libsof_eq_iir_hifi3_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_volume
lib_LIBRARIES  += libsof_volume_hifi3.a

libsof_volume_hifi3_a_SOURCES = $(VOLUME_SRC)

libsof_volume_hifi3_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_mux
lib_LIBRARIES  += libsof_mux_hifi3.a

libsof_mux_hifi3_a_SOURCES = mux.c

libsof_mux_hifi3_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_switch
lib_LIBRARIES  += libsof_switch_hifi3.a

libsof_switch_hifi3_a_SOURCES = switch.c

libsof_switch_hifi3_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_mixer
lib_LIBRARIES  += libsof_mixer_hifi3.a

libsof_mixer_hifi3_a_SOURCES = mixer.c

libsof_mixer_hifi3_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)

# libsof_tone
lib_LIBRARIES  += libsof_tone_hifi3.a

libsof_tone_hifi3_a_SOURCES = tone.c

libsof_tone_hifi3_a_CFLAGS = \
	$(lib_cflags) \
	$(SSE42_CFLAGS) \
	$(COMMON_INCDIR)
endif

endif

else

# build for firmware image

noinst_LIBRARIES = libaudio.a

libaudio_a_SOURCES = \
	eq_iir.c \
	iir.c \
	eq_fir.c \
	fir.c \
	tone.c \
	src.c \
	src_generic.c \
	src_hifi2ep.c \
	src_hifi3.c \
	mixer.c \
	mux.c \
	volume.c \
	volume_generic.c \
	volume_hifi3.c \
	switch.c \
	dai.c \
	host.c \
	pipeline.c \
	pipeline_static.c \
	component.c \
	buffer.c

libaudio_a_CFLAGS = \
	$(lib_cflags) \
	$(COMMON_INCDIR)

endif
