#
# Topology for Alderlake with headphone, DMIC, HDMI with/without speaker.
#

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

# Include Alderlake DSP configuration
include(`platform/intel/adl.m4')
include(`platform/intel/dmic.m4')

DEBUG_START

ifdef(`NO_AMP',,`
ifdef(`AMP_SSP',`',`fatal_error(note: Define AMP_SSP for speaker amp SSP Index)')')

ifdef(`NO_AMP',,`
ifdef(`SMART_AMP',,`
include(`muxdemux.m4')

#
# Define the demux configure
#
# PCM0 --> volume --> demux --> SSP1/SSP2 (speaker - max98360a/rt1019)
#                       |
# PCM6 <----------------+

#define speaker SSP index
define(`SPK_SSP_INDEX', AMP_SSP)
# define SSP BE dai_link name
define(`SPK_SSP_NAME', concat(concat(`SSP', SPK_SSP_INDEX),`-Codec'))
# define BE dai_link ID
define(`SPK_BE_ID', 7)
# Ref capture related
# Ref capture BE dai_name
define(`SPK_REF_DAI_NAME', concat(concat(`SSP', SPK_SSP_INDEX),`.IN'))
define(`DEMUX_ROUTE')')')
ifdef(`DEMUX_ROUTE',
dnl Configure demux
dnl name, pipeline_id, routing_matrix_rows
dnl Diagonal 1's in routing matrix mean that every input channel is
dnl copied to corresponding output channels in all output streams.
dnl I.e. row index is the input channel, 1 means it is copied to
dnl corresponding output channel (column index), 0 means it is discarded.
dnl There's a separate matrix for all outputs.
`define(matrix1, `ROUTE_MATRIX(1,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')'

`define(matrix2, `ROUTE_MATRIX(9,
			     `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
			     `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')'

dnl name, num_streams, route_matrix list
`MUXDEMUX_CONFIG(demux_priv_1, 2, LIST(`	', `matrix1,', `matrix2'))')

ifdef(`NO_AMP',,`
ifdef(`SMART_AMP',`
# PCM0 ----> smart_amp ----> SSP1 (Speaker - max98373)
#              ^
#              |
#              |
# PCM0 <---- demux <----- SSP1 (Speaker - max98373)')')
# PCM1 <---> volume <----> SSP0  (Headset - NAU8825)
# PCM2 ----> volume -----> iDisp1
# PCM3 ----> volume -----> iDisp2
# PCM4 ----> volume -----> iDisp3
# PCM5 ----> volume -----> iDisp4
ifdef(`BT_OFFLOAD', `
# PCM7 ----> passthrough ----> SSP2 (Bluetooth)')
# PCM99 <---- volume <---- DMIC01 (dmic 48k capture)
# PCM100 <---- kpb <---- DMIC16K (dmic 16k capture)

ifdef(`NO_AMP',,`
ifdef(`SMART_AMP',`
# Smart amplifier related
# SSP related
#define smart amplifier SSP index
define(`SMART_SSP_INDEX', AMP_SSP)
#define SSP BE dai_link name
define(`SMART_SSP_NAME', concat(concat(`SSP', AMP_SSP),`-Codec'))
#define BE dai_link ID
define(`SMART_BE_ID', 7)
#define SSP mclk
define(`SSP_MCLK', 24576000)
# Playback related
define(`SMART_PB_PPL_ID', 1)
define(`SMART_PB_CH_NUM', 2)
define(`SMART_TX_CHANNELS', 4)
define(`SMART_RX_CHANNELS', 4)
define(`SMART_FB_CHANNELS', 4)
# Ref capture related
define(`SMART_REF_PPL_ID', 11)
define(`SMART_REF_CH_NUM', 2)
# PCM related
define(`SMART_PCM_ID', 0)
define(`SMART_PCM_NAME', `smart373-spk')

# Include Smart Amplifier support
include(`sof-smart-amplifier.m4')')')

# Define pipeline id for intel-generic-dmic-kwd.m4
# to generate dmic setting with kwd when we have dmic
# define channel
define(CHANNELS, `4')
# define kfbm with volume
define(KFBM_TYPE, `vol-kfbm')
# define pcm, pipeline and dai id
define(DMIC_PCM_48k_ID, `99')
define(DMIC_PIPELINE_48k_ID, `4')
define(DMIC_DAI_LINK_48k_ID, `1')
define(DMIC_PCM_16k_ID, `100')
define(DMIC_PIPELINE_16k_ID, `12')
define(DMIC_PIPELINE_KWD_ID, `13')
define(DMIC_DAI_LINK_16k_ID, `2')
# Offload DMIC_PIPELINE_48K to secondary core for IGO.
ifdef(`IGO', `define(`DMIC_PIPELINE_48k_CORE_ID', 1)')
# define pcm, pipeline and dai id
define(KWD_PIPE_SCH_DEADLINE_US, 20000)
# include the generic dmic with kwd
include(`platform/intel/intel-generic-dmic-kwd.m4')

# define(`SSP_MCLK', )
ifdef(`SSP_MCLK',`',`define(`SSP_MCLK', 19200000)')

ifdef(`BT_OFFLOAD', `
define(`BT_PIPELINE_PB_ID', eval(DMIC_PIPELINE_KWD_ID + 1))
define(`BT_PIPELINE_CP_ID', eval(DMIC_PIPELINE_KWD_ID + 2))
define(`BT_DAI_LINK_ID', 7)

ifdef(`AMP_SSP',`
define(`BT_DAI_LINK_ID', 8)')

define(`BT_PCM_ID', `7')
define(`HW_CONFIG_ID', eval(BT_DAI_LINK_ID))
include(`platform/intel/intel-generic-bt.m4')')

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate)

ifdef(`NO_AMP',,`
ifdef(`SMART_AMP',,`
# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-demux-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)')')

# Low Latency playback pipeline 2 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	2, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 3 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	3, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 5 on PCM 2 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	5, 2, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 6 on PCM 3 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	6, 3, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 4 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	7, 4, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 8 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	8, 5, 2, s32le,
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
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        2, SSP, 0, SSP0-Codec,
        PIPELINE_SOURCE_2, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP0 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        3, SSP, 0, SSP0-Codec,
        PIPELINE_SINK_3, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

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

ifdef(`NO_AMP',,`
ifdef(`SMART_AMP',,`
# playback DAI is SSP1 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        1, SSP, SPK_SSP_INDEX, SPK_SSP_NAME,
        PIPELINE_SOURCE_1, 2, s16le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)
# currently this dai is here as "virtual" capture backend
W_DAI_IN(SSP, SPK_SSP_INDEX, SPK_SSP_NAME, s16le, 3, 0)

# Capture pipeline 9 from demux on PCM 6 using max 2 channels of s32le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture-sched.m4,
	9, 6, 2, s32le,
	1000, 1, 0,
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
}')')

#
# Bind PCM with the pipeline
#
dnl PCM_PLAYBACK_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Headset, 1, PIPELINE_PCM_2, PIPELINE_PCM_3)
PCM_PLAYBACK_ADD(HDMI1, 2, PIPELINE_PCM_5)
PCM_PLAYBACK_ADD(HDMI2, 3, PIPELINE_PCM_6)
PCM_PLAYBACK_ADD(HDMI3, 4, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI4, 5, PIPELINE_PCM_8)
ifdef(`NO_AMP',,`
ifdef(`SMART_AMP',,`
PCM_PLAYBACK_ADD(Speakers, 0, PIPELINE_PCM_1)
PCM_CAPTURE_ADD(EchoRef, 6, PIPELINE_PCM_9)')')

#
# BE configurations - overrides config in ACPI if present
#
dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
dnl SSP_CONFIG(format, mclk, bclk, fsync, tdm, ssp_config_data)
dnl SSP_CLOCK(clock, freq, codec_master, polarity)
dnl SSP_CONFIG_DATA(type, idx, valid bits, mclk_id)
dnl mclk_id is optional
dnl ssp1-maxmspk

#SSP 0 (ID: 0)
DAI_CONFIG(SSP, 0, 0, SSP0-Codec,
        SSP_CONFIG(I2S, SSP_CLOCK(mclk, SSP_MCLK, codec_mclk_in),
                      SSP_CLOCK(bclk, 3072000, codec_slave),
                      SSP_CLOCK(fsync, 48000, codec_slave),
                      SSP_TDM(2, 32, 3, 3),
                      SSP_CONFIG_DATA(SSP, 0, 32, 0, 0, 0, SSP_CC_BCLK_ES)))

# 4 HDMI/DP outputs (ID: 3,4,5,6)
DAI_CONFIG(HDA, 0, 3, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, 4, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
DAI_CONFIG(HDA, 2, 5, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 2, 48000, 2)))
DAI_CONFIG(HDA, 3, 6, iDisp4,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 3, 48000, 2)))

ifdef(`NO_AMP',,`
ifdef(`SMART_AMP',,`
`# SSP' SPK_SSP_INDEX `(ID: 7)'
DAI_CONFIG(SSP, SPK_SSP_INDEX, SPK_BE_ID, SPK_SSP_NAME,
        SSP_CONFIG(I2S, SSP_CLOCK(mclk, SSP_MCLK, codec_mclk_in),
                SSP_CLOCK(bclk, 1536000, codec_slave),
                SSP_CLOCK(fsync, 48000, codec_slave),
                SSP_TDM(2, 16, 3, 3),
                SSP_CONFIG_DATA(SSP, SPK_SSP_INDEX, 16)))')')

DEBUG_END
