#
# Topology for Tigerlake with CODEC amp + rt5682 codec + DMIC + 4 HDMI
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')
include(`muxdemux.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Tigerlake DSP configuration
include(`platform/intel/'PLATFORM`.m4')
include(`platform/intel/dmic.m4')

# Include machine driver definitions
include(`platform/intel/intel-boards.m4')

DEBUG_START

#
# Check option conflicts
#
ifdef(`2CH_2WAY', `ifdef(`4CH_PASSTHROUGH', `fatal_error(note: 2CH_2WAY and 4CH_PASSTHROUGH are mutually exclusive)')')

#
# Set headphone type, default is RT5682
#
ifdef(`USE_DA7219',`define(HEADPHONE, `DA7219')',`
	ifdef(`USE_RT5650',`define(HEADPHONE, `RT5650')',`define(HEADPHONE, `RT5682')')')

#
# Set the MCLK rate for headphone and amplifier SSP ports
#
ifelse(
	HEADPHONE, `DA7219', `
	define(MCLK_RATE, `24576000')',
	HEADPHONE, `RT5650', `
	define(MCLK_RATE, `24576000')
	define(CODEC, `RT5650')',
	HEADPHONE, `RT5682', `
	define(MCLK_RATE, `19200000')',
	`fatal_error(note: unknown HEADPHONE type)')

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

define(matrix2, `ROUTE_MATRIX(9,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

define(matrix3, `ROUTE_MATRIX(1,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,0)')')

define(matrix4, `ROUTE_MATRIX(10,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

dnl name, num_streams, route_matrix list
ifdef(`NO_AMP',`',`
ifdef(`GOOGLE_RTC_AUDIO_PROCESSING',
	`ifdef(`2CH_2WAY',
	`MUXDEMUX_CONFIG(demux_priv_1, 2, LIST_NONEWLINE(`', `matrix3,', `matrix4'))',
	`MUXDEMUX_CONFIG(demux_priv_1, 3, LIST_NONEWLINE(`', `matrix1,', `matrix2,', `matrix4'))')',
	`ifdef(`2CH_2WAY',
	`MUXDEMUX_CONFIG(demux_priv_1, 1, LIST_NONEWLINE(`', `matrix3'))',
	`MUXDEMUX_CONFIG(demux_priv_1, 2, LIST_NONEWLINE(`', `matrix1,', `matrix2'))')'
)')

#
# Define the pipelines
#
ifdef(`2CH_2WAY',`
ifdef(`WAVES',`
# PCM0 --> waves --> demux(2to4-ch) --> eq_iir --> SSP$AMP_SSP (Speaker - CODEC)',`
# PCM0 --> demux(2to4-ch) --> eq_iir --> SSP$AMP_SSP (Speaker - CODEC)')',`
ifdef(`WAVES',`
# PCM0 --> waves --> volume --> demux --> SSP$AMP_SSP (Speaker - CODEC)
#                                 |
# PCM6 <--------------------------+',`
# PCM0 --> volume --> demux --> SSP$AMP_SSP (Speaker - CODEC)
#                       |
# PCM6 <----------------+')')
# PCM1 <---> volume <----> SSP0  (Headset - ALC5682)
# PCM99 <---- volume <----- DMIC01 (dmic0 capture)
# PCM2 ----> volume -----> iDisp1
# PCM3 ----> volume -----> iDisp2
# PCM4 ----> volume -----> iDisp3
# PCM5 ----> volume -----> iDisp4
# PCM99 <---- volume <---- DMIC01 (dmic 48k capture)
# PCM100 <---- kpb <---- DMIC16K (dmic 16k capture)

ifdef(`SPK_MIC_PERIOD_US',`', `
ifdef(`GOOGLE_RTC_AUDIO_PROCESSING',`define(`SPK_MIC_PERIOD_US', 10000)',`define(`SPK_MIC_PERIOD_US', 1000)')')

ifdef(`NO_AMP',`',`
# Define pipeline id for sof-tgl-CODEC-rt5682.m4
ifdef(`AMP_SSP',`',`fatal_error(note: Define AMP_SSP for speaker amp SSP Index)')
# Speaker related
# SSP related
# define speaker SSP index
define(`SPK_SSP_INDEX', AMP_SSP)
# define SSP BE dai_link name
define(`SPK_SSP_NAME', concat(concat(`SSP', SPK_SSP_INDEX),`-Codec'))
# define BE dai_link ID
define(`SPK_BE_ID', BOARD_SPK_BE_ID)
# Ref capture related
# Ref capture BE dai_name
define(`SPK_REF_DAI_NAME', concat(concat(`SSP', SPK_SSP_INDEX),`.IN'))')

# to generate dmic setting with kwd when we have dmic
# define channel
ifdef(`CHANNELS', `', `define(`CHANNELS', 4)')
# define kfbm with volume
define(KFBM_TYPE, `vol-kfbm')
# define pcm, pipeline and dai id
define(DMIC_PCM_48k_ID, `99')
define(DMIC_PIPELINE_48k_ID, `10')
define(DMIC_DAI_LINK_48k_ID, BOARD_DMIC_BE_ID_BASE)
define(DMIC_PCM_16k_ID, `100')
define(DMIC_PIPELINE_16k_ID, `11')
define(DMIC_PIPELINE_KWD_ID, `12')
define(DMIC_DAI_LINK_16k_ID, eval(BOARD_DMIC_BE_ID_BASE + 1))
define(DMIC_PIPELINE_48k_CORE_ID, `1')

ifdef(`GOOGLE_RTC_AUDIO_PROCESSING',
	`ifdef(`RTNR',
		`define(`DMICPROC', rtnr-google-rtc-audio-processing)',
		`define(`DMICPROC', google-rtc-audio-processing)')'
	`define(`DMIC_48k_PERIOD_US', 10000)'
	,
	`ifdef(`RTNR',
		`define(`DMICPROC', rtnr)'
# 5ms period is required for RTNR build 20220728 and later versions
		`define(`DMIC_48k_PERIOD_US', 5000)')'
)

# Run Speakers pipeline on core#1 by default for low power considering
ifdef(`GOOGLE_RTC_AUDIO_PROCESSING', `define(`SPK_PLAYBACK_CORE', DMIC_PIPELINE_48k_CORE_ID)',
        `ifdef(`SPK_PLAYBACK_CORE', `', `define(`SPK_PLAYBACK_CORE', `1')')')

# Google RTC Audio processing processes 10ms at a time. It needs to have time to process it.
ifdef(`GOOGLE_RTC_AUDIO_PROCESSING', `define(`DMIC_48k_PERIOD', 10000)', `')

# When Google RTC Audio (AEC) is applied, put DMIC pipe task after Speaker in schedule order.
ifdef(`GOOGLE_RTC_AUDIO_PROCESSING', `define(`DMIC_48k_PRIORITY', 1)', `')

# define pcm, pipeline and dai id
define(KWD_PIPE_SCH_DEADLINE_US, 5000)

# include the generic dmic with kwd
ifdef(`NOHOTWORD',
`
define(NO16KDMIC)
define(DMIC_48k_CORE_ID, 1)
ifdef(`DMICPROC',`',`define(`DMICPROC', passthrough)')
include(`platform/intel/intel-generic-dmic.m4')',
`include(`platform/intel/intel-generic-dmic-kwd.m4')')

ifdef(`BT_OFFLOAD', `
# BT offload support
define(`BT_PIPELINE_PB_ID', `13')
define(`BT_PIPELINE_CP_ID', `14')
define(`BT_DAI_LINK_ID', BOARD_BT_BE_ID)
define(`BT_PCM_ID', `7')
define(`HW_CONFIG_ID', BOARD_BT_BE_ID)
include(`platform/intel/intel-generic-bt.m4')')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

ifdef(`NO_AMP',`',`
`# Low Latency playback pipeline 1 on PCM 0 using max 'ifdef(`4CH_PASSTHROUGH', `4', `2')` channels of s24le.'
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
define(`ENDPOINT_NAME', `Speakers')
PIPELINE_PCM_ADD(
	ifdef(`2CH_2WAY',
	      ifdef(`WAVES', sof/pipe-waves-demux-eq-iir-playback.m4,
		    sof/pipe-demux-eq-iir-playback.m4),
	      ifdef(`WAVES', sof/pipe-waves-codec-demux-playback.m4,
		    ifdef(`DRC_EQ', sof/pipe-drc-eq-volume-demux-playback.m4,
			ifdef(`DTS', sof/pipe-eq-iir-dts-codec-demux-playback.m4,
				sof/pipe-volume-demux-playback.m4)))),
	1, 0, ifdef(`4CH_PASSTHROUGH', `4', `2'), s32le,
	SPK_MIC_PERIOD_US, 0, SPK_PLAYBACK_CORE,
	48000, 48000, 48000)
undefine(`ENDPOINT_NAME')')

ifdef(`NO_HEADPHONE',`',`
# define Waves integration on Headphones = WAVES && !WAVES_SPK_ONLY
ifdef(`WAVES', `ifdef(`WAVES_SPK_ONLY', `', `define(`WAVES_HEADPHONE')')', `')

# Low Latency playback pipeline 2 on PCM 1 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
define(`ENDPOINT_NAME', `Headphones')
PIPELINE_PCM_ADD(
	ifdef(`WAVES_HEADPHONE', sof/pipe-waves-codec-playback.m4,
	ifdef(`DTS', sof/pipe-eq-iir-dts-codec-playback.m4, sof/pipe-volume-playback.m4)),
	2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)
undefine(`ENDPOINT_NAME')

# Low Latency capture pipeline 3 on PCM 1 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	3, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)')

# Low Latency playback pipeline 2 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 2, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 3 on PCM 3 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 3, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 4 on PCM 4 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 4, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	8, 5, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)

ifdef(`NO_AMP',`',`
# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, SPK_SSP_INDEX, SPK_SSP_NAME,
	PIPELINE_SOURCE_1, 2, FMT,
	SPK_MIC_PERIOD_US, 0, SPK_PLAYBACK_CORE, SCHEDULE_TIME_DOMAIN_TIMER)

ifelse(CODEC, `MAX98390', `
# Low Latency capture pipeline 9 on PCM 6 using max 4 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	9, 6, 4, s32le,
	SPK_MIC_PERIOD_US, 0, SPK_PLAYBACK_CORE,
	48000, 48000, 48000)

# capture DAI is SSP1 using 2 periods
# Buffers use FMT format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	9, SSP, SPK_SSP_INDEX, SPK_SSP_NAME,
	PIPELINE_SINK_9, 2, FMT,
	SPK_MIC_PERIOD_US, 0, SPK_PLAYBACK_CORE, SCHEDULE_TIME_DOMAIN_TIMER)
', CODEC, `CS35L41', `
# Low Latency capture pipeline 9 on PCM 6 using max 4 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	9, 6, 4, s32le,
	SPK_MIC_PERIOD_US, 0, SPK_PLAYBACK_CORE,
	48000, 48000, 48000)

# capture DAI is SSP1 using 2 periods
# Buffers use FMT format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	9, SSP, SPK_SSP_INDEX, SPK_SSP_NAME,
	PIPELINE_SINK_9, 2, FMT,
	SPK_MIC_PERIOD_US, 0, SPK_PLAYBACK_CORE, SCHEDULE_TIME_DOMAIN_TIMER)',
`
ifdef(`2CH_2WAY',`# No echo reference for 2-way speakers',
`
# The echo refenrence pipeline has no connections in it,
# it is used for the capture DAI widget to dock.
DAI_ADD(sof/pipe-echo-ref-dai-capture.m4,
	29, SSP, SPK_SSP_INDEX, SPK_SSP_NAME,
	PIPELINE_SINK_29, 3, FMT,
	SPK_MIC_PERIOD_US, 0, SPK_PLAYBACK_CORE, SCHEDULE_TIME_DOMAIN_TIMER)

`# Capture pipeline 9 from demux on PCM 6 using max 'ifdef(`4CH_PASSTHROUGH', `4', `2')` channels of s32le.'
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture-sched.m4,
	9, 6, ifdef(`4CH_PASSTHROUGH', `4', `2'), s32le,
	SPK_MIC_PERIOD_US, 1, SPK_PLAYBACK_CORE,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Connect demux to capture
SectionGraph."PIPE_CAP" {
	index "0"

	lines [
		# mux to capture
		dapm(PIPELINE_SINK_9, PIPELINE_DEMUX_1)
	]
}

# Connect virtual capture to dai
SectionGraph."PIPE_CAP_VIRT" {
	index "9"

	lines [
		# mux to capture
		dapm(ECHO REF 9, SPK_REF_DAI_NAME)
	]
}
')')')

dnl if using Google AEC
ifdef(`GOOGLE_RTC_AUDIO_PROCESSING',
`# Connect demux to capture'
`SectionGraph."PIPE_GOOGLE_RTC_AUDIO_PROCESSING_REF_AEC" {'
`        index "0"'
`        lines ['
`                # mux to capture'
`                dapm(N_AEC_REF_BUF, PIPELINE_DEMUX_1)'
`       ]'
`}'
dnl else
, `')

ifdef(`NO_HEADPHONE',`',`
# playback DAI is SSP0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	2, SSP, 0, SSP0-Codec,
	PIPELINE_SOURCE_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	3, SSP, 0, SSP0-Codec,
	PIPELINE_SINK_3, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)')

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	5, HDA, 0, iDisp1,
	PIPELINE_SOURCE_5, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	6, HDA, 1, iDisp2,
	PIPELINE_SOURCE_6, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	7, HDA, 2, iDisp3,
	PIPELINE_SOURCE_7, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp4 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	8, HDA, 3, iDisp4,
	PIPELINE_SOURCE_8, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
ifdef(`NO_AMP',`',`
PCM_PLAYBACK_ADD(Speakers, 0, PIPELINE_PCM_1)')
ifdef(`NO_HEADPHONE',`',`
PCM_DUPLEX_ADD(Headset, 1, PIPELINE_PCM_2, PIPELINE_PCM_3)')
PCM_PLAYBACK_ADD(HDMI1, 2, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI2, 3, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI3, 4, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI4, 5, PIPELINE_PCM_8)
ifdef(`NO_AMP',`',`
ifdef(`2CH_2WAY',`# No echo reference for 2-way speakers',
`PCM_CAPTURE_ADD(EchoRef, 6, PIPELINE_PCM_9)')')

#
# BE conf2igurations - overrides config in ACPI if present
#
dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
dnl SSP_CLOCK(clock, freq, codec_master, polarity)
dnl SSP_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional
dnl ssp1-maxmspk

ifdef(`NO_AMP',`',`
# SSP SPK_SSP_INDEX (ID: SPK_BE_ID)
DAI_CONFIG(SSP, SPK_SSP_INDEX, SPK_BE_ID, SPK_SSP_NAME,
ifelse(
	CODEC, `MAX98357A', `
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 1536000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 16, 3, 3),
		SSP_CONFIG_DATA(SSP, SPK_SSP_INDEX, 16)))',
	CODEC, `MAX98360A', `
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 3072000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 32, 3, 3),
		SSP_CONFIG_DATA(SSP, SPK_SSP_INDEX, 32)))',
	CODEC, `MAX98360A_TDM', `
	SSP_CONFIG(DSP_A, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 12288000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(8, 32, 15, 15),
		SSP_CONFIG_DATA(SSP, SPK_SSP_INDEX, 32)))',
	CODEC, `RT1011', `
	SSP_CONFIG(DSP_A, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 4800000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(4, 25, 3, 15),
		SSP_CONFIG_DATA(SSP, SPK_SSP_INDEX, 24)))',
	CODEC, `MAX98390', `
	SSP_CONFIG(DSP_B, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 6144000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(4, 32, 3, 15),
		SSP_CONFIG_DATA(SSP, SPK_SSP_INDEX, 32)))',
	CODEC, `RT1019', `
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
                SSP_CLOCK(bclk, 3072000, codec_slave),
                SSP_CLOCK(fsync, 48000, codec_slave),
                SSP_TDM(2, 32, 3, 3),
                SSP_CONFIG_DATA(SSP, SPK_SSP_INDEX, 24)))',
	CODEC, `CS35L41', `
	SSP_CONFIG(DSP_A, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 6144000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(4, 32, 3, 15),
		SSP_CONFIG_DATA(SSP, SPK_SSP_INDEX, 24, 0, 0, 0, SSP_CC_BCLK_ES)))',
	CODEC, `RT5650', `
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 3072000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 32, 3, 3),
		SSP_CONFIG_DATA(SSP, SPK_SSP_INDEX, 24, 0, 0, 0, SSP_CC_MCLK_AON)))',
	)')

ifdef(`NO_HEADPHONE',`',`
# SSP 0 (ID: BOARD_HP_BE_ID)
DAI_CONFIG(SSP, 0, BOARD_HP_BE_ID, SSP0-Codec,
ifelse(
	HEADPHONE, `DA7219', `
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 3072000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 32, 3, 3),
		SSP_CONFIG_DATA(SSP, 0, 24)))',
	HEADPHONE, `RT5682', `
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 2400000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 25, 3, 3),
		SSP_CONFIG_DATA(SSP, 0, 24, 0, 0, 0, eval(SSP_CC_BCLK_ES | SSP_CC_MCLK_AON))))',
	HEADPHONE, `RT5650', `
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, MCLK_RATE, codec_mclk_in),
		SSP_CLOCK(bclk, 3072000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 32, 3, 3),
		SSP_CONFIG_DATA(SSP, 0, 24, 0, 0, 0, SSP_CC_MCLK_AON)))',
	)')

# 4 HDMI/DP outputs (ID: 3,4,5,6)
DAI_CONFIG(HDA, 0, BOARD_HDMI_BE_ID_BASE, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, eval(BOARD_HDMI_BE_ID_BASE + 1), iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, eval(BOARD_HDMI_BE_ID_BASE + 2), iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))
DAI_CONFIG(HDA, 3, eval(BOARD_HDMI_BE_ID_BASE + 3), iDisp4,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 3, 48000, 2)))

DEBUG_END
