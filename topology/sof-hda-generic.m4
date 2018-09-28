#
# Topology for SKL+ HDA Generic machine
#

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

#
# Define the pipelines
#
# PCM0  <---> volume (pipe 1,2) <----> HDA Analog (HDA Analog playback/capture)
# PCM1  <---> volume (pipe 3,4) <----> HDA Digital (HDA Digital playback/capture)
# PCM3  ----> volume (pipe 7)   -----> iDisp1 (HDMI/DP playback, BE link 3)
# PCM4  ----> Volume (pipe 8)   -----> iDisp2 (HDMI/DP playback, BE link 4)
# PCM5  ----> volume (pipe 9)   -----> iDisp3 (HDMI/DP playback, BE link 5)
#

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s32le,
	48, 1000, 0, 0)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	2, 0, 2, s32le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 3 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
	3, 1, 2, s32le,
	48, 1000, 0, 0)

# Low Latency capture pipeline 4 on PCM 1 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture.m4,
	4, 1, 2, s32le,
	48, 1000, 0, 0)

# Low Latency playback pipeline 7 on PCM 3 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        7, 3, 2, s32le,
        48, 1000, 0, 0)

# Low Latency playback pipeline 8 on PCM 4 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        8, 4, 2, s32le,
        48, 1000, 0, 0)

# Low Latency playback pipeline 9 on PCM 5 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-playback.m4,
        9, 5, 2, s32le,
        48, 1000, 0, 0)

#
# DAIs configuration
#

# playback DAI is HDA Analog using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        1, HDA, 0, Analog Playback and Capture,
        PIPELINE_SOURCE_1, 2, s32le,
        48, 1000, 0, 0)

# capture DAI is HDA Analog using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        2, HDA, 0, Analog Playback and Capture,
	PIPELINE_SINK_2, 2, s32le,
	48, 1000, 0, 0)

# playback DAI is HDA Digital using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        3, HDA, 1, Digital Playback and Capture,
        PIPELINE_SOURCE_3, 2, s32le,
        48, 1000, 0, 0)

# capture DAI is HDA Digital using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
        4, HDA, 1, Digital Playback and Capture,
	PIPELINE_SINK_4, 2, s32le,
	48, 1000, 0, 0)

# playback DAI is iDisp1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        7, HDA, 3, iDisp1,
        PIPELINE_SOURCE_7, 2, s32le,
        48, 1000, 0, 0)

# playback DAI is iDisp2 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        8, HDA, 4, iDisp2,
        PIPELINE_SOURCE_8, 2, s32le,
        48, 1000, 0, 0)

# playback DAI is iDisp3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
        9, HDA, 5, iDisp3,
        PIPELINE_SOURCE_9, 2, s32le,
        48, 1000, 0, 0)

PCM_DUPLEX_ADD(HDA Analog, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)
PCM_DUPLEX_ADD(HDA Digital, 1, PIPELINE_PCM_3, PIPELINE_PCM_4)
PCM_PLAYBACK_ADD(HDMI1, 3, PIPELINE_PCM_7)
PCM_PLAYBACK_ADD(HDMI2, 4, PIPELINE_PCM_8)
PCM_PLAYBACK_ADD(HDMI3, 5, PIPELINE_PCM_9)

#
# BE configurations - overrides config in ACPI if present
#

# HDA outputs
HDA_DAI_CONFIG(0, 4, Analog Playback and Capture)
HDA_DAI_CONFIG(1, 5, Digital Playback and Capture)
# 3 HDMI/DP outputs (ID: 3,4,5)
HDA_DAI_CONFIG(3, 1, iDisp1)
HDA_DAI_CONFIG(4, 2, iDisp2)
HDA_DAI_CONFIG(5, 3, iDisp3)

