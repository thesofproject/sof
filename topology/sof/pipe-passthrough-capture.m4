# Capture Passthrough Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- sink DAI0

# Include topology builder
include(`local.m4')


#
# Components and Buffers
#

# Host "Passthrough Capture" PCM uses pipeline DMAC and channel
# with 0 sink and 2 source periods
W_PCM_CAPTURE(Passthrough Capture, PIPELINE_DMAC, PIPELINE_DMAC_CHAN, 0, 2, 2)

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_PASS_MEM_CAP)

#
# DAI definitions
#
W_DAI_IN(DAI_TYPE, DAI_INDEX, DAI_FORMAT, 0, 2, 2, dai0c_plat_conf)

#
# DAI pipeline - always use 0 for DAIs
#
W_PIPELINE(N_DAI_IN, SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, 0, pipe_dai_schedule_plat)

#
# Pipeline Graph
#
#  host PCM_C <-- B0 <-- sink DAI0

SectionGraph."pipe-pass-capture-PIPELINE_ID" {
	index STR(PIPELINE_ID)

	lines [
		dapm(Passthrough Capture PCM_ID, N_PCMC)
		dapm(N_PCMC, N_BUFFER(0))
	]
}

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Capture PCM_ID)

#
# PCM Configuration
#

SectionPCMCapabilities.STR(Passthrough Capture PCM_ID) {

	formats "S32_LE,S24_LE,S16_LE"
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
