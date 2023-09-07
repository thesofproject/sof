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

define(SSP0_IDX, `0')
define(SSP1_IDX, `1')
define(SSP2_IDX, `2')

define(PIPE_BITS, `s32le')
define(DAI_BITS, `s24le')

#
# Define the pipelines
#
# PCM0 ---> Volume -----\
#                        Mixer ----> SSP0
# PCM3 ---> Volume -----/
# PCM1 ---> Volume ----> Mixer ----> SSP1
# PCM2 ---> volume ----> Mixer ----> SSP2
#
# SSP0 ---> Volume ----> PCM0
# SSP1 ---> Volume ----> PCM1
# SSP2 ---> Volume ----> PCM2
# DMIC0 --> IIR -------> PCM10
# DMIC1 --> IIR -------> PCM11
#

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Volume switch capture pipeline 2 on PCM 0 using max 2 channels of PIPE_BITS.
# Set 1000us deadline on core SSP0_CORE_ID with priority 0
ifdef(`DISABLE_SSP0',,
	`PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	2, 0, 2, PIPE_BITS,
	1000, 0, SSP0_CORE_ID,
	48000, 48000, 48000)')

# Volume switch capture pipeline 4 on PCM 1 using max 2 channels of PIPE_BITS.
# Set 1000us deadline on core SSP1_CORE_ID with priority 0
ifdef(`DISABLE_SSP1',,
	`PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	4, 1, 2, PIPE_BITS,
	1000, 0, SSP1_CORE_ID,
	48000, 48000, 48000)')

# Volume switch capture pipeline 6 on PCM 2 using max 2 channels of PIPE_BITS.
# Set 1000us deadline with priority 0 on core SSP2_CORE_ID
PIPELINE_PCM_ADD(sof/pipe-volume-switch-capture.m4,
	6, 2, 2, PIPE_BITS,
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
# Buffers use DAI_BITS format, 1000us deadline with priority 0 on core SSP0_CORE_ID
# The 'NOT_USED_IGNORED' is due to dependencies and is adjusted later with an explicit dapm line.
ifdef(`DISABLE_SSP0',,
	`DAI_ADD(sof/pipe-mixer-volume-dai-playback.m4,
	1, SSP, SSP0_IDX, NoCodec-0,
	NOT_USED_IGNORED, 2, DAI_BITS,
	1000, 0, SSP0_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)')

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of PIPE_BITS.
# Set 1000us deadline on core SSP0_CORE_ID with priority 0
ifdef(`DISABLE_SSP0',,
	`PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	7, 0, 2, PIPE_BITS,
	1000, 0, SSP0_CORE_ID,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)')

# Deep buffer playback pipeline 11 on PCM 3 using max 2 channels of PIPE_BITS.
# Set 1000us deadline on core SSP0_CORE_ID with priority 0.
# TODO: Modify pipeline deadline to account for deep buffering
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	11, 3, 2, PIPE_BITS,
	1000, 0, SSP0_CORE_ID,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# capture DAI is SSP0 using 2 periods
# Buffers use DAI_BITS format, 1000us deadline with priority 0 on core SSP0_IDX
ifdef(`DISABLE_SSP0',,
	`DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, SSP0_IDX, NoCodec-0,
	PIPELINE_SINK_2, 2, DAI_BITS,
	1000, 0, SSP0_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER)')

# playback DAI is SSP1 using 2 periods
# Buffers use DAI_BITS format, 1000us deadline with priority 0 on core SSP1_CORE_ID
ifdef(`DISABLE_SSP1',,
	`DAI_ADD(sof/pipe-mixer-volume-dai-playback.m4,
	3, SSP, SSP1_IDX, NoCodec-1,
	NOT_USED_IGNORED, 2, DAI_BITS,
	1000, 0, SSP1_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)')

# Low Latency playback pipeline 8 on PCM 1 using max 2 channels of PIPE_BITS.
# Set 1000us deadline on core SSP1_CORE_ID with priority 0
ifdef(`DISABLE_SSP1',,
	`PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	8, 1, 2, PIPE_BITS,
	1000, 0, SSP1_CORE_ID,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_3)')

# capture DAI is SSP1 using 2 periods
# Buffers use DAI_BITS format, 1000us deadline with priority 0 on core SSP1_CORE_ID
ifdef(`DISABLE_SSP1',,
	`DAI_ADD(sof/pipe-dai-capture.m4,
	4, SSP, SSP1_IDX, NoCodec-1,
	PIPELINE_SINK_4, 2, DAI_BITS,
	1000, 0, SSP1_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER)')

# playback DAI is SSP2 using 2 periods
# Buffers use DAI_BITS format, 1000us deadline with priority 0 on core SSP2_CORE_ID
DAI_ADD(sof/pipe-mixer-volume-dai-playback.m4,
	5, SSP, SSP2_IDX, NoCodec-2,
	NOT_USED_IGNORED, 2, DAI_BITS,
	1000, 0, SSP2_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)

# Low Latency playback pipeline 9 on PCM 2 using max 2 channels of PIPE_BITS.
# Set 1000us deadline on core SSP2_CORE_ID with priority 0
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	9, 2, 2, PIPE_BITS,
	1000, 0, SSP2_CORE_ID,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_5)

# capture DAI is SSP2 using 2 periods
# Buffers use DAI_BITS format, 1000us deadline with priority 0 on core SSP2_CORE_ID
DAI_ADD(sof/pipe-dai-capture.m4,
	6, SSP, SSP2_IDX, NoCodec-2,
	PIPELINE_SINK_6, 2, DAI_BITS,
	1000, 0, SSP2_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER)

SectionGraph."mixer-host" {
	index "0"

	lines [
		# connect mixer dai pipelines to PCM pipelines
		ifdef(`DISABLE_SSP0',,`dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_7)')
		ifdef(`DISABLE_SSP1',,	`dapm(PIPELINE_MIXER_3, PIPELINE_SOURCE_8)')
		dapm(PIPELINE_MIXER_5, PIPELINE_SOURCE_9)
	]
}

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
ifdef(`DISABLE_SSP0',,
PCM_DUPLEX_ADD(`Port'SSP0_IDX, 0, PIPELINE_PCM_7, PIPELINE_PCM_2)
)
ifdef(`DISABLE_SSP1',,
PCM_DUPLEX_ADD(`Port'SSP1_IDX, 1, PIPELINE_PCM_8, PIPELINE_PCM_4)
)
PCM_DUPLEX_ADD(`Port'SSP2_IDX, 2, PIPELINE_PCM_9, PIPELINE_PCM_6)

#
# BE configurations - overrides config in ACPI if present
#

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
		      dnl SSP_CONFIG_DATA(type, idx, valid bits, mclk_id, quirks, bclk_delay,
		      dnl clks_control, pulse_width, padding)
		      SSP_CONFIG_DATA(SSP, SSP0_IDX, 32, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))

DAI_CONFIG(SSP, SSP1_IDX, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 32, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP1_IDX, 32, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))

DAI_CONFIG(SSP, SSP2_IDX, 2, NoCodec-2,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 32, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP2_IDX, 32, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))
')

ifelse(ROOT_CLK, `24',
`
DAI_CONFIG(SSP, SSP0_IDX, 0, NoCodec-0,
	   dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP0_IDX, 24, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))

DAI_CONFIG(SSP, SSP1_IDX, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP1_IDX, 24, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))

DAI_CONFIG(SSP, SSP2_IDX, 2, NoCodec-2,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
		      SSP_CLOCK(bclk, 4800000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP2_IDX, 24, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))
')

ifelse(ROOT_CLK, `38_4',
`
DAI_CONFIG(SSP, SSP0_IDX, 0, NoCodec-0,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP0_IDX, 24, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))

DAI_CONFIG(SSP, SSP1_IDX, 1, NoCodec-1,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP1_IDX, 24, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))

DAI_CONFIG(SSP, SSP2_IDX, 2, NoCodec-2,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, SSP2_IDX, 24, 0, SSP_QUIRK_LBM, 0,
				      eval(SSP_CC_MCLK_ES | SSP_CC_BCLK_ES))))
')
