#
# Topology for Tigerlake RVP board CI testing, with covering as more
# features as possible running with no codec machine.
#
# TGL Host GW DMAC support max 6 playback and max 6 capture channels so some
# pipelines/PCMs/DAIs are commented out to keep within HW bounds. If these
# are needed then they can be used provided other PCMs/pipelines/SSPs are
# commented out in their place.

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`ssp.m4')
include(`muxdemux.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Tigerlake DSP configuration
include(`platform/intel/tgl.m4')
include(`platform/intel/dmic.m4')

# Define pipeline id for intel-generic-dmic-kwd.m4
# to generate dmic setting with kwd when we have dmic
# define channel
define(CHANNELS, `2')
# define kfbm with volume
define(KFBM_TYPE, `vol-kfbm')
# define pcm, pipeline and dai id
define(DMIC_PCM_48k_ID, `99')
define(DMIC_PIPELINE_48k_ID, `10')
define(DMIC_DAI_LINK_48k_ID, `6')
define(DMIC_DAI_LINK_48k_NAME, `NoCodec-6')
define(DMIC_PCM_16k_ID, `100')
define(DMIC_PIPELINE_16k_ID, `11')
define(DMIC_PIPELINE_KWD_ID, `12')
define(DMIC_DAI_LINK_16k_ID, `7')
define(DMIC_DAI_LINK_16k_NAME, `NoCodec-7')
# define pcm, pipeline and dai id
define(KWD_PIPE_SCH_DEADLINE_US, 20000)
# include the generic dmic with kwd
include(`platform/intel/intel-generic-dmic-kwd.m4')

#
# Define the demux configure
#
dnl Configure demux
dnl name, pipeline_id, routing_matrix_rows
dnl Diagonal 1's in routing matrix mean that every input channel is
dnl copied to corresponding output channels in all output streams.
dnl I.e. row index is the input channel, 1 means it is copied to
dnl corresponding output channel (column index), 0 means it is discarded.
dnl There's a separate matrix for all outputs.
define(matrix1, `ROUTE_MATRIX(1,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

define(matrix2, `ROUTE_MATRIX(7,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

dnl name, num_streams, route_matrix list
MUXDEMUX_CONFIG(demux_priv_1, 2, LIST(`	', `matrix1,', `matrix2'))

#
# Define the pipelines
#
# PCM0 --> volume --> demux --> SSP0
#                       |        |
# PCM3 <----------------+        |
# PCM0 <-------------------------+
# PCM1 <---> Volume <----> SSP1
# PCM2 ----> smart_amp ----> SSP(SSP_INDEX)
#             ^
#             |
#             |
# PCM2 <---- demux <----- SSP(SSP_INDEX)
# PCM99 <---- volume <---- DMIC01 (dmic 48k capture)
# PCM100 <---- kpb <---- DMIC16K (dmic 16k capture)
#

# Smart amplifier related
# SSP related
#define smart amplifier SSP index
define(`SMART_SSP_INDEX', 2)
#define SSP BE dai_link name
define(`SMART_SSP_NAME', `NoCodec-2')
#define BE dai_link ID
define(`SMART_BE_ID', 2)
#define SSP QUIRK
define(`SMART_SSP_QUIRK', `SSP_QUIRK_LBM')

# Playback related
define(`SMART_PB_PPL_ID', 5)
define(`SMART_PB_CH_NUM', 2)
define(`SMART_TX_CHANNELS', 4)
define(`SMART_RX_CHANNELS', 8)
define(`SMART_FB_CHANNELS', 8)
# Ref capture related
define(`SMART_REF_PPL_ID', 6)
define(`SMART_REF_CH_NUM', 4)
# PCM related
define(`SMART_PCM_ID', 2)
define(`SMART_PCM_NAME', `smart-nocodec')

# Include Smart Amplifier support
include(`sof-smart-amplifier.m4')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-demux-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Passthrough capture pipeline 2 on PCM 0 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	2, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 3 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
#PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
PIPELINE_PCM_ADD(sof/pipe-processing-playback.m4,
	3, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 4 on PCM 1 using max 2 channels of s16le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	4, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 0, NoCodec-0,
	PIPELINE_SINK_2, 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# Capture pipeline 7 from demux on PCM 6 using max 2 channels of s32le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture-sched.m4,
	7, 3, 2, s32le,
	1000, 1, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Connect demux to capture
SectionGraph."PIPE_CAP" {
	index "0"

	lines [
		# mux to capture
		dapm(PIPELINE_SINK_7, PIPELINE_DEMUX_1)
	]
}

# Connect virtual capture to dai
SectionGraph."PIPE_CAP_VIRT" {
	index "7"

	lines [
		# mux to capture
		dapm(ECHO REF 7, SSP0.IN)
	]
}

# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, SSP, 1, NoCodec-1,
	PIPELINE_SOURCE_3, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, SSP, 1, NoCodec-1,
	PIPELINE_SINK_4, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port0, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(Port1, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)
PCM_CAPTURE_ADD(FWEchoRef, 3, PIPELINE_PCM_7)

#
# BE configurations - overrides config in ACPI if present
#

DAI_CONFIG(SSP, 0, 0, NoCodec-0,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, 0, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, 1, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 32, 3, 3),
		      SSP_CONFIG_DATA(SSP, 1, 32, 0, SSP_QUIRK_LBM)))
