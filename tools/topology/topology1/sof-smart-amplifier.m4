#
# Unified topology for smart amplifier implementation.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

ifelse(SDW, `1',
`include(`alh.m4')',
`include(`ssp.m4')')

# Include Token library
include(`sof/tokens.m4')

DEBUG_START

# define the default macros.
# define them in your specific platform .m4 if needed.

# define(`SMART_AMP_CORE', 1) define the DSP core that the DSM pipeline will be run on, if not done yet
ifdef(`SMART_AMP_CORE',`',`define(`SMART_AMP_CORE', 0)')

dnl define smart amplifier ALH index

ifelse(SDW, `1',
`
# ALH related
# define(`SMART_ALH_INDEX', 1) define smart amplifier ALH index
ifdef(`SMART_ALH_INDEX',`',`fatal_error(note: Need to define ALH index for sof-smart-amplifier
)')
ifdef(`SMART_ALH_PLAYBACK_NAME',`',`fatal_error(note: Need to define ALH BE dai_link name for sof-smart-amplifier
)')
ifdef(`SMART_ALH_CAPTURE_NAME',`',`fatal_error(note: Need to define ALH BE dai_link name for sof-smart-amplifier
)')
',
`
# SSP related
# define(`SMART_SSP_INDEX', 1) define smart amplifier SSP index
ifdef(`SMART_SSP_INDEX',`',`fatal_error(note: Need to define SSP index for sof-smart-amplifier
)')
# define(`SMART_SSP_NAME', `SSP1-Codec') define SSP BE dai_link name
ifdef(`SMART_SSP_NAME',`',`fatal_error(note: Need to define SSP BE dai_link name for sof-smart-amplifier
)')
# define(`SMART_SSP_QUIRK', 0) define SSP quirk for special use, e.g. set SSP_QUIRK_LBM to verify
# smart_amp nocodec mode. Set it to 0 by default for normal mode.
ifdef(`SMART_SSP_QUIRK',`',`define(`SMART_SSP_QUIRK', 0)')
# define(`SSP_MCLK', ) define SSP mclk if not done yet
ifdef(`SSP_MCLK',`',`define(`SSP_MCLK', 19200000)')
')

# define(`SMART_BE_ID', 7) define BE dai_link ID
ifdef(`SMART_BE_ID',`',`fatal_error(note: Need to define SSP BE dai_link ID for sof-smart-amplifier
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

#The long process time (aprox. 8ms) and process length (16ms in 16k sample rate) of igo_nr starve
#the scheduler and results in SMART_AMP underflow, ending up with smart_amp component reset and close.
#So increase the buffer size of SMART_AMP is necessary.
ifdef(`IGO', `define(`SMART_AMP_PERIOD', 16000)', `define(`SMART_AMP_PERIOD', 1000)')

ifelse(SDW, `1',
`
#
# Define the pipelines
#
# PCM2 ----> smart_amp ----> ALH(ALH_INDEX)
#             ^
#             |
#             |
# PCM3 <---- demux <----- ALH(ALH_INDEX + 1)
#
'
,`
#
# Define the pipelines
#
# PCM0 ----> smart_amp ----> SSP(SSP_INDEX)
#             ^
#             |
#             |
# PCM0 <---- demux <----- SSP(SSP_INDEX)
#
')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Demux pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-smart-amplifier-playback.m4,
	SMART_PB_PPL_ID, SMART_PCM_ID, SMART_PB_CH_NUM, s32le,
	SMART_AMP_PERIOD, 0, SMART_AMP_CORE,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
ifelse(SDW, `1',
`
PIPELINE_PCM_ADD(sof/pipe-amp-ref-capture.m4,
        SMART_REF_PPL_ID, eval(SMART_PCM_ID + 1), SMART_REF_CH_NUM, s32le,
        SMART_AMP_PERIOD, 0, 0,
        48000, 48000, 48000)
',
`
PIPELINE_PCM_ADD(sof/pipe-amp-ref-capture.m4,
	SMART_REF_PPL_ID, SMART_PCM_ID, SMART_REF_CH_NUM, s32le,
	SMART_AMP_PERIOD, 0, SMART_AMP_CORE,
	48000, 48000, 48000)
')

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

ifelse(SDW, `1',
`
# playback DAI is ALH(ALH_INDEX) using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        SMART_PB_PPL_ID, ALH, SMART_ALH_INDEX, SMART_ALH_PLAYBACK_NAME,
        SMART_PIPE_SOURCE, 2, s24le,
        SMART_AMP_PERIOD, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is ALH(ALH_INDEX) using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        SMART_REF_PPL_ID, ALH, eval(SMART_ALH_INDEX + 1), SMART_ALH_CAPTURE_NAME,
        SMART_PIPE_SINK, 2, s24le,
        SMART_AMP_PERIOD, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)
',
`
# playback DAI is SSP(SPP_INDEX) using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	SMART_PB_PPL_ID, SSP, SMART_SSP_INDEX, SMART_SSP_NAME,
	SMART_PIPE_SOURCE, 2, s32le,
	SMART_AMP_PERIOD, 0, SMART_AMP_CORE, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP(SSP_INDEX) using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	SMART_REF_PPL_ID, SSP, SMART_SSP_INDEX, SMART_SSP_NAME,
	SMART_PIPE_SINK, 2, s32le,
	SMART_AMP_PERIOD, 0, SMART_AMP_CORE, SCHEDULE_TIME_DOMAIN_TIMER)
')

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
ifelse(SDW, `1',
`
PCM_PLAYBACK_ADD(SMART_PCM_NAME, SMART_PCM_ID, SMART_PB_PPL_NAME)
PCM_CAPTURE_ADD(Amplifier Reference, eval(SMART_PCM_ID + 1), SMART_REF_PPL_NAME)
',
`
dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(SMART_PCM_NAME, SMART_PCM_ID, SMART_PB_PPL_NAME, SMART_REF_PPL_NAME)
')

#
# BE configurations - overrides config in ACPI if present
#

ifelse(SDW, `1',
`
#ALH ALH Pin2 (ID: SMART_BE_ID)
DAI_CONFIG(ALH, SMART_ALH_INDEX, SMART_BE_ID, SMART_ALH_PLAYBACK_NAME,
        ALH_CONFIG(ALH_CONFIG_DATA(ALH, SMART_ALH_INDEX, 48000, SMART_TX_CHANNELS)))

#ALH ALH Pin3 (ID: SMART_BE_ID + 1)
DAI_CONFIG(ALH, eval(SMART_ALH_INDEX + 1), eval(SMART_BE_ID + 1), SMART_ALH_CAPTURE_NAME,
        ALH_CONFIG(ALH_CONFIG_DATA(ALH, eval(SMART_ALH_INDEX + 1), 48000, SMART_RX_CHANNELS)))
',
`
#SSP SSP_INDEX (ID: SMART_BE_ID)
DAI_CONFIG(SSP, SMART_SSP_INDEX, SMART_BE_ID, SMART_SSP_NAME,
	SSP_CONFIG(DSP_B, SSP_CLOCK(mclk, SSP_MCLK, codec_mclk_in),
		      SSP_CLOCK(bclk, 12288000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(8, 32, 15, 255),
		      SSP_CONFIG_DATA(SSP, SMART_SSP_INDEX, 32, 0, SMART_SSP_QUIRK)))
')

DEBUG_END
