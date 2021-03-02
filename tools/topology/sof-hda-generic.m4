# Topology for SKL+ HDA Generic machine
#

# if XPROC is not defined, define with default pipe
ifdef(`DMICPROC', , `define(DMICPROC, eq-iir-volume)')
ifdef(`DMIC16KPROC', , `define(DMIC16KPROC, eq-iir-volume)')
ifdef(`HSPROC', , `define(HSPROC, volume)')

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`hda.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include bxt DSP configuration
include(`platform/intel/bxt.m4')

# Define pipeline id for intel-generic-dmic.m4
# to generate dmic setting

ifelse(CHANNELS, `0', ,
`
define(DMIC_PCM_48k_ID, `6')
define(DMIC_PCM_16k_ID, `7')
define(DMIC_DAI_LINK_48k_ID, `6')
define(DMIC_DAI_LINK_16k_ID, `7')
define(DMIC_PIPELINE_48k_ID, `10')
define(DMIC_PIPELINE_16k_ID, `11')

include(`platform/intel/intel-generic-dmic.m4')
'
)

# The pipeline naming notation is pipe-PROCESSING-DIRECTION.m4
# HSPROC is set by makefile, if not the default above is applied
define(PIPE_HEADSET_PLAYBACK, `sof/pipe-`HSPROC'-playback.m4')

#
# Define the pipelines
#
# PCM0P --> volume     (pipe 1) --> HDA Analog (HDA Analog playback)
# PCM0C <-- volume, EQ (pipe 2) <-- HDA Analog (HDA Analog capture)
# PCM1P --> volume     (pipe 3) --> HDA Digital (HDA Digital playback)
# PCM1C <-- volume, EQ (pipe 4) <-- HDA Digital (HDA Digital capture)
# PCM3  ----> volume (pipe 7) ----> iDisp1 (HDMI/DP playback, BE link 3)
# PCM4  ----> Volume (pipe 8) ----> iDisp2 (HDMI/DP playback, BE link 4)
# PCM5  ----> volume (pipe 9) ----> iDisp3 (HDMI/DP playback, BE link 5)
#

# If HSPROC_FILTERx is defined set PIPELINE_FILTERx
ifdef(`HSPROC_FILTER1', `define(PIPELINE_FILTER1, HSPROC_FILTER1)', `undefine(`PIPELINE_FILTER1')')
ifdef(`HSPROC_FILTER2', `define(PIPELINE_FILTER2, HSPROC_FILTER2)', `undefine(`PIPELINE_FILTER2')')

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(PIPE_HEADSET_PLAYBACK,
	1, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Undefine PIPELINE_FILTERx to avoid to propagate elsewhere, other endpoints
# with filters blobs will need similar handling as HSPROC_FILTERx.
undefine(`PIPELINE_FILTER1')
undefine(`PIPELINE_FILTER2')

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-highpass-capture.m4,
	2, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 3 on PCM 1 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 1, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 4 on PCM 1 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-highpass-capture.m4,
	4, 1, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 7 on PCM 3 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        7, 3, 2, s24le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 8 on PCM 4 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        8, 4, 2, s24le,
        1000, 0, 0,
	48000, 48000, 48000)

# Low Latency playback pipeline 9 on PCM 5 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        9, 5, 2, s24le,
        1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

# playback DAI is HDA Analog using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        1, HDA, 0, Analog Playback and Capture,
        PIPELINE_SOURCE_1, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is HDA Analog using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        2, HDA, 1, Analog Playback and Capture,
	PIPELINE_SINK_2, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is HDA Digital using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        3, HDA, 2, Digital Playback and Capture,
        PIPELINE_SOURCE_3, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is HDA Digital using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        4, HDA, 3, Digital Playback and Capture,
	PIPELINE_SINK_4, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp1 using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        7, HDA, 4, iDisp1,
        PIPELINE_SOURCE_7, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp2 using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        8, HDA, 5, iDisp2,
        PIPELINE_SOURCE_8, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# playback DAI is iDisp3 using 2 periods
# Dai buffers use s32le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        9, HDA, 6, iDisp3,
        PIPELINE_SOURCE_9, 2, s32le,
        1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

PCM_DUPLEX_ADD(HDA Analog, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(HDA Digital, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)
PCM_PLAYBACK_ADD(HDMI1, 3, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI2, 4, PIPELINE_PCM_8)
PCM_PLAYBACK_ADD(HDMI3, 5, PIPELINE_PCM_9)

#
# BE configurations - overrides config in ACPI if present
#

# HDA outputs
DAI_CONFIG(HDA, 0, 4, Analog Playback and Capture,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 0, 48000, 2)))
DAI_CONFIG(HDA, 1, 5, Digital Playback and Capture,
	HDA_CONFIG(HDA_CONFIG_DATA(HDA, 1, 48000, 2)))
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
