# Topology for SKL+ HDA Generic machine
#

# if XPROC is not defined, define with default pipe
ifdef(`HSPROC', , `define(HSPROC, volume)')

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`hda.m4')
include(`muxdemux.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include bxt DSP configuration
include(`platform/intel/bxt.m4')

# Intel DMIC
include(`platform/intel/dmic.m4')

# The pipeline naming notation is pipe-mixer-PROCESSING-dai-DIRECTION.m4
# HSPROC is set by makefile, if not the default above is applied
define(PIPE_HEADSET_PLAYBACK, `sof/pipe-mixer-`HSPROC'-dai-playback.m4')

#
# Define the pipelines
#
# PCM0P --> volume     (pipe 1) --> HDA Analog (HDA Analog playback)
# PCM0C <-- volume, EQ (pipe 2) <-- HDA Analog (HDA Analog capture)
# PCM3  ----> volume (pipe 7) ----> iDisp1 (HDMI/DP playback, BE link 3)
# PCM4  ----> Volume (pipe 8) ----> iDisp2 (HDMI/DP playback, BE link 4)
# PCM5  ----> volume (pipe 9) ----> iDisp3 (HDMI/DP playback, BE link 5)
#

# If HSPROC_FILTERx is defined set PIPELINE_FILTERx
ifdef(`HSPROC_FILTER1', `define(PIPELINE_FILTER1, HSPROC_FILTER1)', `undefine(`PIPELINE_FILTER1')')
ifdef(`HSPROC_FILTER2', `define(PIPELINE_FILTER2, HSPROC_FILTER2)', `undefine(`PIPELINE_FILTER2')')

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-highpass-capture.m4,
	2, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 3 using max 2 channels of s24le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        7, 3, 2, s24le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 8 on PCM 4 using max 2 channels of s24le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        8, 4, 2, s24le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 9 on PCM 5 using max 2 channels of s24le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        9, 5, 2, s24le,
        1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

# playback DAI is HDA Analog using 2 periods
# Dai buffers use s32le format, 1000us deadline with priority 0 on core 0
# The 'NOT_USED_IGNORED' is due to dependencies and is adjusted later with an explicit dapm line.
DAI_ADD(PIPE_HEADSET_PLAYBACK,
        1, HDA, 0, Analog Playback and Capture,
        NOT_USED_IGNORED, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER, 2, 48000)

# Low Latency playback pipeline 1 on PCM 30 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	30, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Deep buffer playback pipeline 31 on PCM 31 using max 2 channels of s32le
# Set 1000us deadline on core 0 with priority 0.
# TODO: Modify pipeline deadline to account for deep buffering
ifdef(`DEEP_BUFFER',
`
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	31, 31, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)
'
)

# Undefine PIPELINE_FILTERx to avoid to propagate elsewhere, other endpoints
# with filters blobs will need similar handling as HSPROC_FILTERx.
undefine(`PIPELINE_FILTER1')
undefine(`PIPELINE_FILTER2')

# capture DAI is HDA Analog using 2 periods
# Dai buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-capture.m4,
        2, HDA, 1, Analog Playback and Capture,
	PIPELINE_SINK_2, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# Dai buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
        7, HDA, 4, iDisp1,
        PIPELINE_SOURCE_7, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Dai buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
        8, HDA, 5, iDisp2,
        PIPELINE_SOURCE_8, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Dai buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-playback.m4,
        9, HDA, 6, iDisp3,
        PIPELINE_SOURCE_9, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

SectionGraph."mixer-host" {
	index "0"

	lines [
		# connect mixer dai pipelines to PCM pipelines
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_30)
ifdef(`DEEP_BUFFER',
`
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_31)
'
)
	]
}

PCM_DUPLEX_ADD(HDA Analog, 0, PIPELINE_PCM_30, PIPELINE_PCM_2)
ifdef(`DEEP_BUFFER',
`
PCM_PLAYBACK_ADD(HDA Analog Deep Buffer, 31, PIPELINE_PCM_31)
'
)
PCM_PLAYBACK_ADD(HDMI1, 3, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI2, 4, PIPELINE_PCM_8)
PCM_PLAYBACK_ADD(HDMI3, 5, PIPELINE_PCM_9)

#
# PCM8C <-- volume <-- demux <-- volume <-- demux <-- DMIC
#                        |                    |
# PCM7C <-------------  -+                    |
#                                             |
# PCM6C <-------------------------------------+

define(DMIC_PCM_CHANNELS, `2')
define(DMIC_48k_PCM_1_NAME, `Passthrough Capture 11')
define(DMIC_48k_PCM_2_NAME, `Passthrough Capture 13')
define(DMIC_48k_PCM_3_NAME, `Passthrough Capture 14')
define(DMIC_PCM_1_48k_ID, `6')
define(DMIC_PCM_2_48k_ID, `7')
define(DMIC_PCM_3_48k_ID, `8')
define(DMIC_DAI_LINK_48k_ID, `6')
define(DMIC_DAI_LINK_48k_NAME, `dmic01')
define(DMIC_48k_PERIOD_US, `1000')
define(DMIC_48k_CORE_ID, `0')
define(DEF_STEREO_PDM, `STEREO_PDM0')

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
define(matrix11, `ROUTE_MATRIX(11,
                             `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

define(matrix12, `ROUTE_MATRIX(12,
                             `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

define(matrix21, `ROUTE_MATRIX(13,
                             `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')

define(matrix22, `ROUTE_MATRIX(14,
                             `BITS_TO_BYTE(1, 0, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 1, 0 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 1 ,0 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,1 ,0 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,1 ,0 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,1 ,0 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,1 ,0)',
                             `BITS_TO_BYTE(0, 0, 0 ,0 ,0 ,0 ,0 ,1)')')


dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period , priority, core, time_domain,
dnl     channels, rate, dynamic_pipe)
DAI_ADD(sof/pipe-dai-demux-capture.m4,
        10, DMIC, 0, DMIC_DAI_LINK_48k_NAME,
        NOT_USED_IGNORED, 2, s32le,
        1000, 0, DMIC_48k_CORE_ID, SCHEDULE_TIME_DOMAIN_TIMER,
	2, 48000, 0)

# RAW DAI capture pipeline

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp, dynamic)
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	11, DMIC_PCM_1_48k_ID, 2, s32le,
	DMIC_48k_PERIOD_US, 0, DMIC_48k_CORE_ID,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER, PIPELINE_DEMUX_CAPTURE_SCHED_COMP_10, 0)


# Intermediate processing1-demux pipeline
# Check: PIPELINE_ADD arg 13 DYNAMIC_PIPE errors -- omit now

dnl PIPELINE_ADD(pipeline,
dnl     pipe id, max channels, format,
dnl     period, priority, core,
dnl     sched_comp, time_domain,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate, dynamic)
PIPELINE_ADD(sof/pipe-volume-demux.m4,
	12, 2, s32le,
	DMIC_48k_PERIOD_US, 0, DMIC_48k_CORE_ID,
	PIPELINE_DEMUX_CAPTURE_SCHED_COMP_10, SCHEDULE_TIME_DOMAIN_TIMER,
	48000, 48000, 48000)

# Capture PCM pipeline from processing1

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp, dynamic)
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	13, DMIC_PCM_2_48k_ID, DMIC_PCM_CHANNELS, s32le,
	DMIC_48k_PERIOD_US, 0, DMIC_48k_CORE_ID,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER, PIPELINE_DEMUX_CAPTURE_SCHED_COMP_10, 0)

# Capture PCM pipeline with processing2

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp, dynamic)
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	14, DMIC_PCM_3_48k_ID, DMIC_PCM_CHANNELS, s32le,
	DMIC_48k_PERIOD_US, 0, DMIC_48k_CORE_ID,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER, PIPELINE_DEMUX_CAPTURE_SCHED_COMP_10, 0)

# Add pipeline widgets here to pipelines 11, 13, 34 to avoid modify
# pipe-passthrough-capture.m4 and pipe-volume-capture.m4 with
# W_PIPELINE() additions
W_PIPELINE_TOP(11, DMIC0.IN, 1000, 0, 0, 1, pipe_dai_schedule_plat)
W_PIPELINE_TOP(13, DMIC0.IN, 1000, 0, 0, 1, pipe_dai_schedule_plat)
W_PIPELINE_TOP(14, DMIC0.IN, 1000, 0, 0, 1, pipe_dai_schedule_plat)

dnl name, num_streams, route_matrix list
MUXDEMUX_CONFIG(demux_priv_10, 2, LIST_NONEWLINE(`', `matrix11,', `matrix12'))
MUXDEMUX_CONFIG(demux_priv_12, 2, LIST_NONEWLINE(`', `matrix21,', `matrix22'))

SectionGraph."dai-demux" {
	index "0"

	lines [
		# connect mixer dai pipelines to PCM pipelines
		dapm(PIPELINE_SINK_11, PIPELINE_DEMUX_SOURCE_10)
		dapm(PIPELINE_SINK_12, PIPELINE_DEMUX_SOURCE_10)
		dapm(PIPELINE_SINK_13, PIPELINE_DEMUX_SOURCE_12)
		dapm(PIPELINE_SINK_14, PIPELINE_DEMUX_SOURCE_12)
	]
}

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
dnl PCM_CAPTURE_ADD(name, pipeline, capture)
PCM_CAPTURE_ADD(DMIC_48k_PCM_1_NAME, DMIC_PCM_1_48k_ID, PIPELINE_PCM_11)
PCM_CAPTURE_ADD(DMIC_48k_PCM_2_NAME, DMIC_PCM_2_48k_ID, PIPELINE_PCM_13)
PCM_CAPTURE_ADD(DMIC_48k_PCM_3_NAME, DMIC_PCM_3_48k_ID, PIPELINE_PCM_14)

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)
DAI_CONFIG(DMIC, 0, DMIC_DAI_LINK_48k_ID, DMIC_DAI_LINK_48k_NAME,
           DMIC_CONFIG(1, 2400000, 4800000, 40, 60, 48000,
    	               DMIC_WORD_LENGTH(s32le), 200, DMIC, 0,
           	       PDM_CONFIG(DMIC, 0, DEF_STEREO_PDM)))

#
# BE configurations - overrides config in ACPI if present
#

# HDA outputs
DAI_CONFIG(HDA, 0, 4, Analog Playback and Capture,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))

# 3 HDMI/DP outputs (ID: 3,4,5)
DAI_CONFIG(HDA, 4, 1, iDisp1,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 4, 48000, 2)))
DAI_CONFIG(HDA, 5, 2, iDisp2,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 5, 48000, 2)))
DAI_CONFIG(HDA, 6, 3, iDisp3,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 6, 48000, 2)))


VIRTUAL_DAPM_ROUTE_IN(codec0_in, HDA, 1, IN, 1)
VIRTUAL_DAPM_ROUTE_IN(codec1_in, HDA, 3, IN, 2)
VIRTUAL_DAPM_ROUTE_OUT(codec0_out, HDA, 0, OUT, 3)
VIRTUAL_DAPM_ROUTE_OUT(codec1_out, HDA, 2, OUT, 4)

# codec2 is not supported in dai links but it exists
# in dapm routes, so hack this one to HDA1
VIRTUAL_DAPM_ROUTE_IN(codec2_in, HDA, 3, IN, 5)
VIRTUAL_DAPM_ROUTE_OUT(codec2_out, HDA, 2, OUT, 6)

VIRTUAL_DAPM_ROUTE_OUT(iDisp1_out, HDA, 4, OUT, 7)
VIRTUAL_DAPM_ROUTE_OUT(iDisp2_out, HDA, 5, OUT, 8)
VIRTUAL_DAPM_ROUTE_OUT(iDisp3_out, HDA, 6, OUT, 9)

VIRTUAL_WIDGET(iDisp3 Tx, out_drv, 0)
VIRTUAL_WIDGET(iDisp2 Tx, out_drv, 1)
VIRTUAL_WIDGET(iDisp1 Tx, out_drv, 2)
VIRTUAL_WIDGET(Analog CPU Playback, out_drv, 3)
VIRTUAL_WIDGET(Digital CPU Playback, out_drv, 4)
VIRTUAL_WIDGET(Alt Analog CPU Playback, out_drv, 5)
VIRTUAL_WIDGET(Analog CPU Capture, input, 6)
VIRTUAL_WIDGET(Digital CPU Capture, input, 7)
VIRTUAL_WIDGET(Alt Analog CPU Capture, input, 8)
