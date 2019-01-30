#
# Topology for ApolloLake with direct attach digital microphones array for
# keword detection and triggering use case.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')
include(`platform/intel/dmic.m4')

DEBUG_START

#
# Define the pipelines
#
# PCM 0 <-------+- Mux 0 <-- B0 <-- DMIC6 (DMIC01)
#               |
# Keyword <-----+

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)


# Passthrough capture pipeline 1 on PCM 0 using max 2 channels.
# Schedule 16 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-mux-capture.m4,
	1, 0, 2, s32le,
	16, 1000, 0, 0)


# keyword detector pipe
dnl PIPELINE_ADD(pipeline,
dnl     pipe id, max channels, format,
dnl     frames, deadline, priority, core)
PIPELINE_ADD(sof/pipe-detect.m4, 1, 2, s16le, 16, 1000, 0, 0)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)


# capture DAI is DMIC 0 using 1000 periods (64kB buffer for 2 secs of audio)
# Buffers use s16le format, with 16 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	1, DMIC, 0, NoCodec-6,
	PIPELINE_SINK_1, 1000, s16le,
	16, 1000, 0, 0)

# Connect pipelines together
SectionGraph."pipe-sound-trigger" {
	index "0"

	lines [
		#key detect
		dapm(PIPELINE_SINK_1, PIPELINE_SOURCE_1)
	]
}

dnl PCM_CAPTURE_ADD(name, pipeline, capture)
PCM_CAPTURE_ADD(DMIC01, 1, PIPELINE_PCM_1)

#
# BE configurations - overrides config in ACPI if present
#

dnl DAI_CONFIG(type, dai_index, link_id, name, ssp_config/dmic_config)

DAI_CONFIG(DMIC, 0, 6, NoCodec-6,
           dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
           dnl             sample_rate,
           dnl             fifo word length, type, dai_index, pdm controller config)
           DMIC_CONFIG(1, 500000, 4800000, 40, 60, 16000,
                dnl DMIC_WORD_LENGTH(frame_format)
                DMIC_WORD_LENGTH(s16le), DMIC, 0,
                dnl PDM_CONFIG(type, dai_index, num pdm active, pdm tuples list)
                dnl STEREO_PDM0 is a pre-defined pdm config for stereo capture
                PDM_CONFIG(DMIC, 0, STEREO_PDM0)))

# AFter PR#864 is merged
#DAI_CONFIG(DMIC, 1, 7, NoCodec-7,
#           dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
#           dnl             sample_rate,
#           dnl             fifo word length, type, dai_index, pdm controller config)
#           DMIC_CONFIG(1, 500000, 4800000, 40, 60, 16000,
#                dnl DMIC_WORD_LENGTH(frame_format)
#                DMIC_WORD_LENGTH(s16le), DMIC, 1,
#                dnl PDM_CONFIG(type, dai_index, num pdm active, pdm tuples list)
#                dnl STEREO_PDM0 is a pre-defined pdm config for stereo capture
#                PDM_CONFIG(DMIC, 0, STEREO_PDM0)))

DEBUG_END
