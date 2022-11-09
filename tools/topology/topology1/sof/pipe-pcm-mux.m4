# Low Power PCM Media Pipeline
#
#  Low power PCM media playback with volume.
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --B0--> volume(0P) --B1--> Endpoint Pipeline
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`muxdemux.m4')
include(`pipeline.m4')
include(`pcm.m4')
include(`bytecontrol.m4')

#
# Controls
#



#
# Components and Buffers
#

# Host "Media Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Media Playback MUX, 8, 0, SCHEDULE_CORE)

# Media Source Buffers to SRC
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)

#
# Pipeline Graph
#
#  PCM --B0 --> Endpoint Pipeline


P_GRAPH(pipe-pcm-mux, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))'))


#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(0))


#
# Pipeline Configuration.
#
W_PIPELINE(SCHED_COMP, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_media_schedule_plat)


#
# PCM Configuration
#

# PCM capabilities supported by FW

PCM_CAPABILITIES(Media Playback MUX PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

# PCM Media Playback
SectionPCM.STR(Media Playback MUX PCM_ID) {

	index STR(PIPELINE_ID)

	# used for binding to the PCM
	id STR(PCM_ID)

	dai.STR(Media Playback MUX PCM_ID) {
		id STR(PCM_ID)
	}

	# Playback Configuration
	pcm."playback" {

		capabilities STR(Media Playback MUX PCM_ID)
	}
}
