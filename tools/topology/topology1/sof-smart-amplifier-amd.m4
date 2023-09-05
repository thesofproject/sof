#
# Unified topology for smart amplifier implementation.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

include(`acp-hs.m4')

# Include Token library
include(`sof/tokens.m4')

DEBUG_START

# ======================================================================

# define the default macros.
# define them in your specific platform .m4 if needed.

# define(`SMART_AMP_CORE', 1) define the DSP core that the DSM pipeline will be run on, if not done yet
ifdef(`SMART_AMP_CORE',`',`define(`SMART_AMP_CORE', 0)')

dnl define smart amplifier ALH index

# define(`SMART_BE_ID', 7) define BE dai_link ID
ifdef(`SMART_BE_ID',`',`fatal_error(note: Need to define I2SHS BE dai_link ID for sof-smart-amplifier
)')
# Playback related
# define(`SMART_PB_PPL_ID', 1)
ifdef(`SMART_PB_PPL_ID',`',`fatal_error(note: Need to define playback pipeline ID for sof-smart-amplifier
)')
# define(`SMART_PB_CH_NUM', 2)
ifdef(`SMART_PB_CH_NUM',`',`fatal_error(note: Need to define playback channel number for sof-smart-amplifier
)')
define(`SMART_PIPE_SOURCE', concat(`PIPELINE_SOURCE_', SMART_PB_PPL_ID))
# define(`SMART_TX_CHANNELS', 4)
ifdef(`SMART_TX_CHANNELS',`',`fatal_error(note: Need to define DAI TX channel number for sof-smart-amplifier
)')
# define(`SMART_RX_CHANNELS', 8)
ifdef(`SMART_RX_CHANNELS',`',`fatal_error(note: Need to define DAI RX channel number for sof-smart-amplifier
)')
# define(`SMART_FB_CHANNELS', 4)
ifdef(`SMART_FB_CHANNELS',`',`fatal_error(note: Need to define feedback channel number for sof-smart-amplifier
)')
define(`SMART_PB_PPL_NAME', concat(`PIPELINE_PCM_', SMART_PB_PPL_ID))
# Ref capture related
# define(`SMART_REF_PPL_ID', 11)
ifdef(`SMART_REF_PPL_ID',`',`fatal_error(note: Need to define Echo Ref pipeline ID for sof-smart-amplifier
)')
# define(`SMART_REF_CH_NUM', 4)
ifdef(`SMART_REF_CH_NUM',`',`fatal_error(note: Need to define Echo Ref channel number for sof-smart-amplifier
)')
define(`SMART_PIPE_SINK', concat(`PIPELINE_SINK_', SMART_REF_PPL_ID))
# define(`N_SMART_DEMUX', `MUXDEMUX'SMART_REF_PPL_ID`.'$1)
define(`SMART_REF_PPL_NAME', concat(`PIPELINE_PCM_', SMART_REF_PPL_ID))
# PCM related
# define(`SMART_PCM_ID', 0)
ifdef(`SMART_PCM_ID',`',`fatal_error(note: Need to define PCM ID for sof-smart-amplifier
)')
ifdef(`SMART_PCM_NAME',`',`fatal_error(note: Need to define Speaker PCM name for sof-smart-amplifier
)')
# =====================================================================================
define(`SMART_AMP_PERIOD', 2000)

#
# Define the pipelines
#
# PCM1 ----> smart_amp ----> I2SHS
#             ^
#             |
#             |
# PCM1 <---- demux <----- I2SHS
#


dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Demux pipeline 1 on PCM 0 using max 2 channels of s16le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-smart-amplifier-playback-amd.m4,
	SMART_PB_PPL_ID, SMART_PCM_ID, SMART_PB_CH_NUM, s16le,
	SMART_AMP_PERIOD, 0, SMART_AMP_CORE,
	48000, 48000, 48000)
# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s16le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-amp-ref-capture.m4,
	SMART_REF_PPL_ID, SMART_PCM_ID, SMART_REF_CH_NUM, s16le,
	SMART_AMP_PERIOD, 0, SMART_AMP_CORE,
	48000, 48000, 48000)
# ===================================================================
#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is ACPHS(ACPHS_INDEX) using 2 periods
# Buffers use s16le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	SMART_PB_PPL_ID, ACPHS, 1, acp-amp-codec,
	SMART_PIPE_SOURCE, 2, s16le,
	SMART_AMP_PERIOD, 0, SMART_AMP_CORE, SCHEDULE_TIME_DOMAIN_DMA)

# capture DAI is ACPHS(ACPHS_INDEX) using 2 periods
# Buffers use s16le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-capture.m4,
	SMART_REF_PPL_ID, ACPHS, 1, acp-amp-codec,
	SMART_PIPE_SINK, 2, s16le,
	SMART_AMP_PERIOD, 0, SMART_AMP_CORE, SCHEDULE_TIME_DOMAIN_DMA)
# ======================================================================
# Connect demux to smart_amp
ifdef(`N_SMART_REF_BUF',`',`fatal_error(note: Need to define ref buffer name for connection
)')
ifdef(`N_SMART_DEMUX',`',`fatal_error(note: Need to define demux widget name for connection
)')
SectionGraph."PIPE_SMART_AMP" {
	index "0"

	lines [
		# demux to smart_amp
		dapm(N_SMART_REF_BUF, N_SMART_DEMUX)
	]
}

# PCM for SMART_AMP Playback and EchoRef.
dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(SMART_PCM_NAME, SMART_PCM_ID, SMART_PB_PPL_NAME, SMART_REF_PPL_NAME)

#
# BE configurations - overrides config in ACPI if present
#

DAI_CONFIG(ACPHS, 1, SMART_BE_ID, acp-amp-codec,
	ACPHS_CONFIG(I2S, ACP_CLOCK(mclk, 49152000, codec_mclk_in),
		      ACP_CLOCK(bclk, 3072000, codec_slave),
		      ACP_CLOCK(fsync, 48000, codec_slave),
		      ACP_TDM(2, 32, 3, 3),
		      ACPHS_CONFIG_DATA(ACPHS, 1, 48000, 2, 0)))

DEBUG_END
