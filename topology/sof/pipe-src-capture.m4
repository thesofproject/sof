# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> SRC --> sink DAI0

# Include topology builder
include(`local.m4')


#
# Components and Buffers
#

# Host "Passthrough Capture" PCM uses pipeline DMAC and channel
# with 4 sink and 0 source periods
W_PCM_CAPTURE(Passthrough Capture, PIPELINE_DMAC, PIPELINE_DMAC_CHAN, 4, 0, 2)

#
# SRC Configuration
#

SectionVendorTuples."media_src_tokens" {
	tokens "sof_src_tokens"

	tuples."word" {
		SOF_TKN_SRC_RATE_OUT	"48000"
	}
}

SectionData."media_src_conf" {
	tuples "media_src_tokens"
}

# "SRC" has 4 source and 4 sink periods
W_SRC(0, PIPELINE_FORMAT, 4, 4, media_src_conf, 2)

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(4,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES))
W_BUFFER(1, COMP_BUFFER_SIZE(4,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES))

#
# DAI definitions
#
W_DAI_IN(DAI_TYPE, DAI_INDEX, DAI_FORMAT, 0, DAI_PERIODS,
	DAI_PERIODS, dai0c_plat_conf)

#
# DAI pipeline
#
W_PIPELINE(N_DAI_IN, SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES,
	SCHEDULE_CORE, 0, pipe_dai_schedule_plat)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> SRC 0 --> B1 --> sink DAI0

SectionGraph."pipe-pass-src-capture-PIPELINE_ID" {
	index STR(PIPELINE_ID)

	lines [
		dapm(Passthrough Capture PCM_ID, N_PCMC)
		dapm(N_PCMC, N_BUFFER(0))
		dapm(N_BUFFER(0), N_SRC(0))
		dapm(N_SRC(0), N_BUFFER(1))
	]
}

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Capture PCM_ID)

#
# PCM Configuration
#

SectionPCMCapabilities.STR(Passthrough Capture PCM_ID) {

	formats "S32_LE,S24_LE,S16_LE"
	rate_min "8000"
	rate_max "96000"
	channels_min "2"
	channels_max "4"
	periods_min "2"
	periods_max "16"
	period_size_min	"192"
	period_size_max	"16384"
	buffer_size_min	"65536"
	buffer_size_max	"65536"
}
