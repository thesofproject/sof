# Low Latency Passthrough Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> sink DAI0

# Include topology builder
include(`local.m4')


#
# Components and Buffers
#

# Host "Passthrough Playback" PCM uses pipeline DMAC and channel
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(Passthrough Playback, PIPELINE_DMAC, PIPELINE_DMAC_CHAN, 2, 0, 2)

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES))

#
# DAI definitions
#
W_DAI_OUT(DAI_SNAME, DAI_TYPE, DAI_INDEX, DAI_FORMAT, 0, 2, 2, dai0p_plat_conf)

#
# DAI pipeline - always use 0 for DAIs
#
W_PIPELINE(N_DAI_OUT, SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, pipe_dai_schedule_plat)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> sink DAI0

SectionGraph."pipe-pass-playback-PIPELINE_ID" {
	index STR(PIPELINE_ID)

	lines [
		dapm(N_PCM, Passthrough Playback PCM_ID)
		dapm(N_BUFFER(0), N_PCM)
		dapm(N_DAI_OUT, N_BUFFER(0))
	]
}


#
# PCM Configuration
#

SectionPCMCapabilities.STR(Passthrough Playback PCM_ID) {

	formats "S24_LE,S16_LE"
	rate_min "48000"
	rate_max "48000"
	channels_min "2"
	channels_max "4"
	periods_min "2"
	periods_max "16"
	period_size_min	"192"
	period_size_max	"16384"
	buffer_size_min	"65536"
	buffer_size_max	"65536"
}

# PCM Low Latency Passthrough Playback
SectionPCM.STR(PCM PCM_ID) {

	index STR(PIPELINE_ID)

	# used for binding to the PCM
	id STR(PCM_ID)

	dai.STR(Passthrough Playback PCM_ID) {
		id STR(PCM_ID)
	}

	pcm."playback" {

		capabilities STR(Passthrough Playback PCM_ID)
	}
}
