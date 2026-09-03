# virtual playback passthrough pipeline and dai
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> virtual-playback-passthrough

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# Components and Buffers
#

# Host "Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Playback, DAI_PERIODS, 0, SCHEDULE_CORE)


# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP, SCHEDULE_CORE)

# Virtual input widget
VIRTUAL_WIDGET(VIRTUAL PLAYBACK PASSTHROUGH PIPELINE_ID, input, PIPELINE_ID)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> virtual-playback-passthrough

P_GRAPH(pipe-virtual-passthrough-playback-sched, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(VIRTUAL PLAYBACK PASSTHROUGH PIPELINE_ID, N_BUFFER(0))' ))
#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Playback PCM_ID)

#
# Pipeline Configuration.
#

W_PIPELINE(SCHED_COMP, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_media_schedule_plat)

#
# PCM Configuration

#
PCM_CAPABILITIES(Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

