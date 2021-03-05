#
# Topology for generic Apollolake UP^2 with pcm512x codec and HDMI.
#

# if XPROC is not defined, define with default pipe
ifdef(`DMICPROC', , `define(DMICPROC, eq-iir-volume)')
ifdef(`DMIC16KPROC', , `define(DMIC16KPROC, eq-iir-volume)')

# if CHANNELS is not defined, define with default 2ch. Note that
# it can be overrode with DMIC_DAI_CHANNELS, DMIC_PCM_CHANNELS
# in intel-generic-dmic.m4. Same macros exist for DMIC16K too.
ifdef(`CHANNELS', , `define(CHANNELS, 2)')

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')

define(`SSP_SCHEDULE_TIME_DOMAIN',
	ifdef(`CODEC_MASTER', SCHEDULE_TIME_DOMAIN_DMA, SCHEDULE_TIME_DOMAIN_TIMER))

DEBUG_START

#
# Define the pipelines
#
# PCM0 <---> volume <----> SSP5 (pcm512x)
# PCM5 ----> volume -----> iDisp1
# PCM6 ----> volume -----> iDisp2
# PCM7 ----> volume -----> iDisp3
# PCM4 ----> volume -----> Media Playback 4
# PCM1 <------------------ DMIC0 (DMIC)
# PCM2 <------------------ DMIC1 (DMIC16kHz)
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback-bt.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	8000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	6, 0, 2, s32le,
	1000, 0, 0,
	FSYNC, FSYNC, FSYNC)

# Low Latency playback pipeline 2 on PCM 5 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	2, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 3 on PCM 6 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 6, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 4 on PCM 7 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	4, 7, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# platform/intel/intel-generic-dmic.m4 uses DAI link IDs for PCM IDs so we have
# to use PCM1 and PCM2 for DMICs.

ifelse(CHANNELS, `0', ,
`
define(DMIC_PCM_48k_ID, `1')
define(DMIC_PCM_16k_ID, `2')
define(DMIC_DAI_LINK_48k_ID, `1')
define(DMIC_DAI_LINK_16k_ID, `2')
define(DMIC_PIPELINE_48k_ID, `7')
define(DMIC_PIPELINE_16k_ID, `8')
include(`platform/intel/intel-generic-dmic.m4')
'
)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP5 using 2 periods
# Buffers use s24le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 5, SSP5-Codec,
	PIPELINE_SOURCE_1, 2, s24le,
	1000, 0, 0, SSP_SCHEDULE_TIME_DOMAIN, 2, 48000, 1)

# capture DAI is SSP5 using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	6, SSP, 5, SSP5-Codec,
	PIPELINE_SINK_6, 2, s24le,
	1000, 0, 0, SSP_SCHEDULE_TIME_DOMAIN, 2, 48000, 1)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	2, HDA, 0, iDisp1,
	PIPELINE_SOURCE_2, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, HDA, 1, iDisp2,
	PIPELINE_SOURCE_3, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	4, HDA, 2, iDisp3,
	PIPELINE_SOURCE_4, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
PCM_DUPLEX_ADD(Port5, 0, PIPELINE_PCM_1, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI1, 5, PIPELINE_PCM_2)
PCM_PLAYBACK_ADD(HDMI2, 6, PIPELINE_PCM_3)
PCM_PLAYBACK_ADD(HDMI3, 7, PIPELINE_PCM_4)

#
# BE configurations - overrides config in ACPI if present
#

define(`hwconfig_names', HW_CONFIG_NAMES(LIST(`     ', "hw_config1", "hw_config2")))
define(`data_names', DAI_DATA_NAMES(LIST(`     ', "ssp_data1", "ssp_data2")))

define(`ssp_config_list_1', LIST(`',
	`MULTI_SSP_CONFIG(hw_config1, 0, I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),'
		`SSP_CLOCK(bclk, 3072000, codec_master),'
		`SSP_CLOCK(fsync, 48000, codec_master),'
		`SSP_TDM(2, 32, 3, 3),'
		`SSP_MULTI_CONFIG_DATA(ssp_data1, 24))',
	`MULTI_SSP_CONFIG(hw_config2, 1, I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),'
		`SSP_CLOCK(bclk, 512000, codec_master),'
		`SSP_CLOCK(fsync, 8000, codec_master),'
		`SSP_TDM(1, 32, 1, 1),'
		`SSP_MULTI_CONFIG_DATA(ssp_data2, 24))'))

define(`ssp_config_list_2', LIST(`',
	`MULTI_SSP_CONFIG(hw_config1, 0, I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),'
		`SSP_CLOCK(bclk, 3072000, codec_slave),'
		`SSP_CLOCK(fsync, 48000, codec_slave),'
		`SSP_TDM(2, 32, 3, 3),'
		`SSP_MULTI_CONFIG_DATA(ssp_data1, 24))',
	`MULTI_SSP_CONFIG(hw_config2, 1, I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),'
		`SSP_CLOCK(bclk, 512000, codec_slave),'
		`SSP_CLOCK(fsync, 8000, codec_slave),'
		`SSP_TDM(1, 32, 1, 1),'
		`SSP_MULTI_CONFIG_DATA(ssp_data2, 24))'))

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
#SSP 5 (ID: 0)
ifdef(`CODEC_MASTER',
`
MULTI_DAI_CONFIG(SSP, 5, 0, SSP5-Codec, ssp_config_list_1, hwconfig_names, data_names)
'
,
`
MULTI_DAI_CONFIG(SSP, 5, 0, SSP5-Codec, ssp_config_list_2, hwconfig_names, data_names)
'
)

# 3 HDMI/DP outputs (ID: 3,4,5)
DAI_CONFIG(HDA, 0, 3, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, 4, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, 5, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))

DEBUG_END
