# Topology for SKL+ HDA Generic machine
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

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
define(DMIC_PIPELINE_48k_ID, `10')
define(DMIC_PIPELINE_16k_ID, `11')

include(`platform/intel/intel-generic-dmic.m4')
'
)

#
# Define the pipelines
#
# PCM0  <---> volume (pipe 1,2) <----> HDA Analog (HDA Analog playback/capture)
# PCM1  <---> volume (pipe 3,4) <----> HDA Digital (HDA Digital playback/capture)

include(`sof-hda-generic-idisp.m4')
#
# Include the pipelines
#
# PCM3  ----> volume (pipe 7)   -----> iDisp1 (HDMI/DP playback, BE link 3)
# PCM4  ----> Volume (pipe 8)   -----> iDisp2 (HDMI/DP playback, BE link 4)
# PCM5  ----> volume (pipe 9)   -----> iDisp3 (HDMI/DP playback, BE link 5)
#

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s24le,
	1000, 0, 0,
	48000, 48000, 48000)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
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
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	4, 1, 2, s24le,
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

PCM_DUPLEX_ADD(HDA Analog, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(HDA Digital, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)

#
# BE configurations - overrides config in ACPI if present
#

# HDA outputs
DAI_CONFIG(HDA, 0, 4, Analog Playback and Capture)
DAI_CONFIG(HDA, 1, 5, Digital Playback and Capture)

VIRTUAL_DAPM_ROUTE_IN(codec0_in, HDA, 1, IN, 1)
VIRTUAL_DAPM_ROUTE_IN(codec1_in, HDA, 3, IN, 2)
VIRTUAL_DAPM_ROUTE_OUT(codec0_out, HDA, 0, OUT, 3)
VIRTUAL_DAPM_ROUTE_OUT(codec1_out, HDA, 2, OUT, 4)

# codec2 is not supported in dai links but it exists
# in dapm routes, so hack this one to HDA1
VIRTUAL_DAPM_ROUTE_IN(codec2_in, HDA, 3, IN, 5)
VIRTUAL_DAPM_ROUTE_OUT(codec2_out, HDA, 2, OUT, 6)

VIRTUAL_WIDGET(Analog CPU Playback, out_drv, 3)
VIRTUAL_WIDGET(Digital CPU Playback, out_drv, 4)
VIRTUAL_WIDGET(Alt Analog CPU Playback, out_drv, 5)
VIRTUAL_WIDGET(Analog CPU Capture, input, 6)
VIRTUAL_WIDGET(Digital CPU Capture, input, 7)
VIRTUAL_WIDGET(Alt Analog CPU Capture, input, 8)
