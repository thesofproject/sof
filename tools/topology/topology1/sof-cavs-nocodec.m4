#
# Topology for generic Cannonlake board with no codec and digital mic array.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`ssp.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include DSP configuration
include(`platform/intel/'PLATFORM`.m4')

# bxt has 2 cores but currently only one is enabled in the build
ifelse(PLATFORM, `bxt', `define(NCORES, 1)')
ifelse(PLATFORM, `cnl', `define(NCORES, 4)')
ifelse(PLATFORM, `cml', `define(NCORES, 4)')
ifelse(PLATFORM, `icl', `define(NCORES, 4)')
ifelse(PLATFORM, `jsl', `define(NCORES, 2)')
ifelse(PLATFORM, `tgl', `define(NCORES, 4)')
ifelse(PLATFORM, `ehl', `define(NCORES, 4)')
ifelse(PLATFORM, `adl', `define(NCORES, 4)')

define(CHANNELS, `4')

define(DMIC_PCM_48k_ID, `10')
define(DMIC_PCM_16k_ID, `11')

define(DMIC_PIPELINE_48k_ID, `20')
define(DMIC_PIPELINE_16k_ID, `21')

define(DMIC_DAI_LINK_48k_NAME, `NoCodec-3')
define(DMIC_DAI_LINK_16k_NAME, `NoCodec-4')

define(DMIC_DAI_LINK_48k_ID, `3')
define(DMIC_DAI_LINK_16k_ID, `4')

define(DMICPROC, `eq-iir-volume')
define(DMIC16KPROC, `eq-iir-volume')

define(DMICPROC_FILTER1, `eq_iir_coef_highpass_40hz_20db_48khz.m4')
define(DMIC16KPROC_FILTER1, `eq_iir_coef_highpass_40hz_20db_16khz.m4')

ifelse(NCORES, `4',
`
define(DMIC_48k_CORE_ID, `0')
define(DMIC_16k_CORE_ID, `0')
define(SSP0_CORE_ID, `0')
define(SSP1_CORE_ID, `0')
define(SSP2_CORE_ID, `0')
')

ifelse(NCORES, `2',
`
define(DMIC_48k_CORE_ID, `0')
define(DMIC_16k_CORE_ID, `0')
define(SSP0_CORE_ID, `0')
define(SSP1_CORE_ID, `0')
define(SSP2_CORE_ID, `0')
')

ifelse(NCORES, `1',
`
define(DMIC_48k_CORE_ID, `0')
define(DMIC_16k_CORE_ID, `0')
define(SSP0_CORE_ID, `0')
define(SSP1_CORE_ID, `0')
define(SSP2_CORE_ID, `0')
')

include(`platform/intel/intel-generic-dmic.m4')

ifelse(PLATFORM, `bxt',
`
define(SSP0_IDX, `0')
define(SSP1_IDX, `1')
define(SSP2_IDX, `5')
',
`
define(SSP0_IDX, `0')
define(SSP1_IDX, `1')
define(SSP2_IDX, `2')
')

#
# Define the pipelines
#
# PCM0 <---> volume <----> SSP0
# PCM1 <---> Volume <----> SSP1
# PCM2 <---> volume <----> SSP2
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 2 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s32le,
	1000, 0, SSP0_CORE_ID,
	48000, 48000, 48000)

# Volume switch capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 2 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	2, 0, 2, s32le,
	1000, 0, SSP0_CORE_ID,
	48000, 48000, 48000)

# Low Latency playback pipeline 3 on PCM 1 using max 2 channels of s32le.
# Set 1000us deadline on core 1 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 1, 2, s32le,
	1000, 0, SSP1_CORE_ID,
	48000, 48000, 48000)

# Volume switch capture pipeline 4 on PCM 1 using max 2 channels of s32le.
# Set 1000us deadline on core 1 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	4, 1, 2, s32le,
	1000, 0, SSP1_CORE_ID,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 2 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 2, 2, s32le,
	1000, 0, SSP2_CORE_ID,
	48000, 48000, 48000)

# Volume switch capture pipeline 6 on PCM 2 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	6, 2, 2, s32le,
	1000, 0, SSP2_CORE_ID,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     deadline, priority, core, time_domain)

# playback DAI is SSP0 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, SSP0_IDX, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s32le,
	1000, 0, SSP0_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, SSP0_IDX, NoCodec-0,
	PIPELINE_SINK_2, 2, s32le,
	1000, 0, SSP0_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SSP1 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	3, SSP, SSP1_IDX, NoCodec-1,
	PIPELINE_SOURCE_3, 2, s32le,
	1000, 0, SSP1_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP1 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	4, SSP, SSP1_IDX, NoCodec-1,
	PIPELINE_SINK_4, 2, s32le,
	1000, 0, SSP1_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is SSP2 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, SSP, SSP2_IDX, NoCodec-2,
	PIPELINE_SOURCE_5, 2, s32le,
	1000, 0, SSP2_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP2 using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	6, SSP, SSP2_IDX, NoCodec-2,
	PIPELINE_SINK_6, 2, s32le,
	1000, 0, SSP2_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(`Port'SSP0_IDX, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(`Port'SSP1_IDX, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)
PCM_DUPLEX_ADD(`Port'SSP2_IDX, 2, PIPELINE_PCM_5, PIPELINE_PCM_6)

#
# BE configurations - overrides config in ACPI if present
#

ifelse(PLATFORM, `bxt', `define(ROOT_CLK, 19_2)')
ifelse(PLATFORM, `cnl', `define(ROOT_CLK, 24)')
ifelse(PLATFORM, `cml', `define(ROOT_CLK, 24)')
ifelse(PLATFORM, `icl', `define(ROOT_CLK, 38_4)')
ifelse(PLATFORM, `jsl', `define(ROOT_CLK, 38_4)')
ifelse(PLATFORM, `tgl', `define(ROOT_CLK, 38_4)')
ifelse(PLATFORM, `ehl', `define(ROOT_CLK, 38_4)')
ifelse(PLATFORM, `adl', `define(ROOT_CLK, 38_4)')

ifelse(ROOT_CLK, `19_2',
`
DAI_CONFIG(SSP, SSP0_IDX, 0, NoCodec-0,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 32, 3, 3),
		      dnl SSP_CONFIG_DATA(type, dai_index, valid bits, mclk_id, quirks)
		      SSP_CONFIG_DATA(SSP, SSP0_IDX, 32, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, SSP1_IDX, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 32, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP1_IDX, 32, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, SSP2_IDX, 2, NoCodec-2,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 32, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP2_IDX, 32, 0, SSP_QUIRK_LBM)))
')

ifelse(ROOT_CLK, `24',
`
DAI_CONFIG(SSP, SSP0_IDX, 0, NoCodec-0,
	   dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      dnl SSP_CONFIG_DATA(type, dai_index, valid bits, mclk_id, quirks)
		      SSP_CONFIG_DATA(SSP, SSP0_IDX, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, SSP1_IDX, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP1_IDX, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, SSP2_IDX, 2, NoCodec-2,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP2_IDX, 24, 0, SSP_QUIRK_LBM)))
')

ifelse(ROOT_CLK, `38_4',
`
DAI_CONFIG(SSP, SSP0_IDX, 0, NoCodec-0,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP0_IDX, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, SSP1_IDX, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP1_IDX, 24, 0, SSP_QUIRK_LBM)))

DAI_CONFIG(SSP, SSP2_IDX, 2, NoCodec-2,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP2_IDX, 24, 0, SSP_QUIRK_LBM)))
')
