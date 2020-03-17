#
# Topology for Tigerlake with rt5682 codec.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include Token library
include(`sof/tokens.m4')

# Include platform specific DSP configuration
include(`platform/intel/tgl.m4')

DEBUG_START

# define the default macros.
# redefine them in your specific platform .m4 if needed.

# SSP related
define(`SMART_SSP_INDEX', 0)
define(`SMART_SSP_NAME', `NoCodec-0')
define(`SMART_BE_ID', 0)
# Playback related
define(`SMART_PB_PPL_ID', 1)
define(`SMART_PB_CH_NUM', 2)
define(`SMART_PIPE_SOURCE', concat(`PIPELINE_SOURCE_', SMART_PB_PPL_ID))
define(`N_SMART_REF_BUF', `BUF'SMART_PB_PPL_ID`.'$1)
define(`SMART_PB_PPL_NAME', concat(`PIPELINE_PCM_', SMART_PB_PPL_ID))
# Ref capture related
define(`SMART_REF_PPL_ID', 2)
define(`SMART_REF_CH_NUM', 4)
define(`SMART_PIPE_SINK', concat(`PIPELINE_SINK_', SMART_REF_PPL_ID))
define(`N_SMART_DEMUX', `MUXDEMUX'SMART_REF_PPL_ID`.'$1)
define(`SMART_REF_PPL_NAME', concat(`PIPELINE_PCM_', SMART_REF_PPL_ID))
# PCM related
define(`SMART_PCM_ID', 0)
define(`SMART_PCM_NAME', `smart_spk')

#
# Define the pipelines
#
# PCM0 ----> dsm ----> SSP(SSP_INDEX)
#             ^
#             |
#             |
# PCM0 <---- demux <----- SSP(SSP_INDEX)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Demux pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-smart-amplifier-playback.m4,
	SMART_PB_PPL_ID, SMART_PCM_ID, SMART_PB_CH_NUM, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-demux-capture.m4,
	SMART_REF_PPL_ID, SMART_PCM_ID, SMART_REF_CH_NUM, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP(SPP_INDEX) using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	SMART_PB_PPL_ID, SSP, SMART_SSP_INDEX, SMART_SSP_NAME,
	SMART_PIPE_SOURCE, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP(SSP_INDEX) using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	SMART_REF_PPL_ID, SSP, SMART_SSP_INDEX, SMART_SSP_NAME,
	SMART_PIPE_SINK, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# Connect demux to dsm
SectionGraph."PIPE_DSM" {
	index "0"

	lines [
		# demux to dsm
		dapm(N_SMART_REF_BUF(2), N_SMART_DEMUX(0))
	]
}

# PCM for DSM Playback and EchoRef.
dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(SMART_PCM_NAME, SMART_PCM_ID, SMART_PB_PPL_NAME, SMART_REF_PPL_NAME)

#
# BE configurations - overrides config in ACPI if present
#

#SSP SSP_INDEX (ID: SMART_BE_ID)
DAI_CONFIG(SSP, SMART_SSP_INDEX, SMART_BE_ID, SMART_SSP_NAME,
	SSP_CONFIG(DSP_B, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 12288000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(8, 32, 15, 255),
		      SSP_CONFIG_DATA(SSP, SMART_SSP_INDEX, 32, 0, SSP_QUIRK_LBM)))

DEBUG_END
