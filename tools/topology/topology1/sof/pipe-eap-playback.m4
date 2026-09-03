# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> EAP 0 --> B1 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')
include(`enumcontrol.m4')
include(`pipeline.m4')
include(`eap.m4')

#
# Controls
#
# Include defines for EAP controls
include(`eap_controls.m4')

#
# Components and Buffers
#
# Host "EAP Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, EAP Playback, 2, 0)

# "EAP 0" has x sink period and 2 source periods
W_EAP(0, PIPELINE_FORMAT, DAI_PERIODS, 2, SCHEDULE_CORE,
	LIST(`		', "DEF_EAP_CFG"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> EAP --> B1 --> sink DAI0
P_GRAPH(pipe-eap-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_EAP(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_EAP(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID),
	EAP Playback PCM_ID)

#
# PCM Configuration

#
PCM_CAPABILITIES(EAP Playback PCM_ID, `S16_LE',
	PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`DAI_EAP_CFG')
