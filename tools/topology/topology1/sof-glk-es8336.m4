#
# Topology for generic Apollolake UP^2 with es8316 codec.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include DSP configuration
include(`platform/intel/'PLATFORM`.m4')

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# define(`SSP_MCLK', )
ifelse(PLATFORM, `cml', `define(`SSP_MCLK', 24000000)', `define(`SSP_MCLK', 19200000)')

# Define pipeline id for intel-generic-dmic.m4
# to generate dmic setting

ifelse(CHANNELS, `0', ,
`

# if XPROC is not defined, define with default pipe
ifdef(`DMICPROC', , `define(DMICPROC, eq-iir-volume)')
ifdef(`DMIC16KPROC', , `define(DMIC16KPROC, eq-iir-volume)')

define(DMIC_PCM_48k_ID, `1')
define(DMIC_PCM_16k_ID, `2')
define(DMIC_DAI_LINK_48k_ID, `1')
define(DMIC_DAI_LINK_16k_ID, `2')
define(DMIC_PIPELINE_48k_ID, `3')
define(DMIC_PIPELINE_16k_ID, `4')

include(`platform/intel/intel-generic-dmic.m4')
'
)

# Add HDMI-SSP Audio Offload pass-through
ifdef(`HDMI_1_SSP_NUM',
`	define(`HDMI_SSP_NUM', HDMI_1_SSP_NUM)
	define(`HDMI_SSP_PIPELINE_CP_ID', `8')
	define(`HDMI_SSP_DAI_LINK_ID', 6)
	define(`HDMI_SSP_PCM_ID', `3') dnl use fixed PCM_ID
	include(`platform/intel/intel-hdmi-ssp.m4')
'
)

ifdef(`HDMI_2_SSP_NUM',
`	define(`HDMI_SSP_NUM', HDMI_2_SSP_NUM)
	define(`HDMI_SSP_PIPELINE_CP_ID', `9')
	define(`HDMI_SSP_DAI_LINK_ID', 7)
	define(`HDMI_SSP_PCM_ID', `4') dnl use fixed PCM_ID
	include(`platform/intel/intel-hdmi-ssp.m4')
'
)

DEBUG_START
#
# Define the pipelines
#
# PCM0 <----> volume <-----> SSP2 (es8316)
#
ifelse(CHANNELS, `0',
`
# PCM2 <-------------------- DMIC01 (dmic0 capture, BE dailink 2)
# PCM3 <-------------------- DMIC16k (dmic16k, BE dailink 3)
')
# PCM5  ----> volume (pipe 5)   -----> iDisp1 (HDMI/DP playback, BE link 5)
# PCM6  ----> Volume (pipe 6)   -----> iDisp2 (HDMI/DP playback, BE link 6)
# PCM7  ----> volume (pipe 7)   -----> iDisp3 (HDMI/DP playback, BE link 7)
ifdef(`HDMI_1_SSP_NUM',
`
# PCM3 <---- volume <----- HDMI_1_SSP_NUM (lt6911)
'
)
ifdef(`HDMI_2_SSP_NUM',
`
# PCM4 <---- volume <----- HDMI_2_SSP_NUM (lt6911)
'
)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-low-latency-capture.m4,
	2, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 5 using max 2 channels of s32le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        5, 5, 2, s32le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 6 on PCM 6 using max 2 channels of s32le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        6, 6, 2, s32le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 7 using max 2 channels of s32le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        7, 7, 2, s32le,
        1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP2 using 2 periods
# Buffers use s24le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, SSP_NUM, `SSP'SSP_NUM`-Codec',
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

# capture DAI is SSP2 using 2 periods
# Buffers use s24le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, SSP_NUM, `SSP'SSP_NUM`-Codec',
	PIPELINE_SINK_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
        5, HDA, 3, iDisp1,
        PIPELINE_SOURCE_5, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
        6, HDA, 4, iDisp2,
        PIPELINE_SOURCE_6, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
        7, HDA, 5, iDisp3,
        PIPELINE_SOURCE_7, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

PCM_DUPLEX_ADD(ES8336, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)

PCM_PLAYBACK_ADD(HDMI 1, 5, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI 2, 6, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI 3, 7, PIPELINE_PCM_7)

# BE configurations - overrides config in ACPI if present

DAI_CONFIG(SSP, SSP_NUM, 0, `SSP'SSP_NUM`-Codec',
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, SSP_MCLK, codec_mclk_in),
		SSP_CLOCK(bclk, 2400000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 25, 3, 3),
		SSP_CONFIG_DATA(SSP, SSP_NUM, 24, 0)))


# 3 HDMI/DP outputs (ID: 3,4,5)
DAI_CONFIG(HDA, 3, 3, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 3, 48000, 2)))
DAI_CONFIG(HDA, 4, 4, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 4, 48000, 2)))
DAI_CONFIG(HDA, 5, 5, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 5, 48000, 2)))

DEBUG_END


