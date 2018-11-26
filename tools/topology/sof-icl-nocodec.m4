#
# Topology for generic Icelake board with nocodec
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Icelake DSP configuration
include(`platform/intel/icl.m4')

#
# Define the pipelines
#
# PCM0 <---> Volume <---> SSP0 (NoCodec)
#

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-volume-playback.m4,
	1, 0, 2, s24le,
	48, 1000, 0, 0, SSP, 0, s24le, 2)

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s24le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_DAI_ADD(sof/pipe-volume-capture.m4,
	2, 0, 2, s24le,
	48, 1000, 0, 0, SSP, 0, s24le, 2)

#
# DAI configuration
#
# SSP port 0 is our only pipeline DAI
#

# playback DAI is SSP0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SSP, 0, NoCodec-0,
	PIPELINE_SOURCE_1, 2, s24le,
	48, 1000, 0, 0)

# capture DAI is SSP0 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SSP, 0, NoCodec-0,
	PIPELINE_SINK_2, 2, s24le,
	48, 1000, 0, 0)

# PCM Low Latency
PCM_DUPLEX_ADD(Passthrough, 0, PIPELINE_PCM_1, PIPELINE_PCM_2)

#
# BE configurations - overrides config in ACPI if present
#
DAI_CONFIG(SSP, 0, 0, NoCodec-0,
	   SSP_CONFIG(DSP_B, SSP_CLOCK(mclk, 38400000, codec_mclk_in),
		      SSP_CLOCK(bclk, 2400000, codec_slave),
		      SSP_CLOCK(fsync, 48000, codec_slave),
		      SSP_TDM(2, 25, 3, 3),
		      SSP_CONFIG_DATA(SSP, 0, 24)))
