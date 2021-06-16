# Low Latency Passthrough Pipeline
#
# Pipeline Endpoints for connection are :-
#
#  B0 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# Components and Buffers
#

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_PASS_MEM_CAP)

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(0))

