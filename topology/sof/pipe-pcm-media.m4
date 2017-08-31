# Low Power PCM Media Pipeline
#
#  Low power PCM media playback with SRC and volume.
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --B0--> volume(0P) --B1--> SRC -- B2 --> Endpoint Pipeline
#

# Include topology builder
include(`local.m4')

#
# Controls
#

SectionControlMixer.STR(PCM PCM_ID Playback Volume) {

	# control belongs to this index group
	index STR(PIPELINE_ID)

	# Channel register and shift for Front Left/Right
	channel."FL" {
		reg "1"
		shift "0"
	}
	channel."FR" {
		reg "1"
		shift "1"
	}

	# control uses bespoke driver get/put/info ID 0
	ops."ctl" {
		info "volsw"
		get "256"
		put "256"
	}

	# TLV 32 steps from -90dB to +6dB for 3dB
	max "32"
	invert "false"
	tlv "vtlv_m90s3"
}


#
# SRC Configuration
#

SectionVendorTuples."media_src_tokens" {
	tokens "sof_src_tokens"

	tuples."word" {
		SOF_TKN_SRC_RATE_IN	16000
		SOF_TKN_SRC_RATE_IN	44100
		SOF_TKN_SRC_RATE_OUT	48000
	}
}

SectionData."media_src_conf" {
	tuples "media_src_tokens"
}

#
# Components and Buffers
#

# Host "Low latency Playback" PCM uses pipeline DMAC and channel
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(Media Playback, PIPELINE_DMAC, PIPELINE_DMAC_CHAN, 2, 0, 2)

# "Playback Volume" has 2 sink period and 2 source periods for host ping-pong
W_PGA(0, PCM PCM_ID Playback Volume, PIPELINE_FORMAT, 2, 2, 2)

# "SRC 0" has 2 sink and source periods.
W_SRC(0, media_src_conf, PIPELINE_FORMAT, 2, 2, 2)

# Media Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES))
W_BUFFER(1,COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES))
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES))

#
# Pipeline Graph
#
#  PCM --B0--> volume --B1--> SRC --> B2 --> Endpoint Pipeline
#

SectionGraph."pipe-media-PIPELINE_ID" {
	index STR(PIPELINE_ID)

	lines [
		dapm(N_PCM, Media Playback PCM_ID)
		dapm(N_BUFFER(0), N_PCM)
		dapm(N_PGA(0), N_BUFFER(0))
		dapm(N_BUFFER(1), N_PGA(0))
		dapm(N_SRC(0), N_BUFFER(1))
		dapm(N_BUFFER(2), N_SRC(0))
	]
}

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))

#
# Pipeline Configuration.
#

W_PIPELINE(N_SRC(0), SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, pipe_media_schedule_plat)

#
# PCM Configuration
#

# PCM capabilities supported by FW

SectionPCMCapabilities.STR(Media Playback PCM_ID) {
	formats "S24_LE,S16_LE"
	rate_min "8000"
	rate_max "192000"
	channels_min "2"
	channels_max "2"
	periods_min "2"
	periods_max "32"
	period_size_min	"4096"
	period_size_max	"262144"
	buffer_size_min	"8388608"
	buffer_size_max	"8388608"
}

# PCM Low Latency Playback and Capture
SectionPCM.STR(PCM PCM_ID) {

	index STR(PIPELINE_ID)

	# used for binding to the PCM
	id STR(PCM_ID)

	dai.STR(Media Playback PCM_ID) {
		id STR(PCM_ID)
	}

	# Playback and Capture Configuration
	pcm."playback" {

		capabilities STR(Media Playback PCM_ID)
	}
}
