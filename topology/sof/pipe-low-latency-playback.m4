# Low Latency Pipeline
#
#  Low Latency Playback PCM mixed into single sink pipe.
#  Low latency Capture PCM.
#
# Pipeline Endpoints for connection are :-
#
#	LL Playback Mixer (Mixer)
#	LL Capture Volume B4 (DAI buffer)
#	LL Playback Volume B3 (DAI buffer)
#
#
#  host PCM_P --B0--> volume(0P) --B1--+
#                                      |--ll mixer(M) --B2--> volume(LL) ---B3-->  sink DAI0
#                     pipeline n+1 >---+
#                                      |
#                     pipeline n+2 >---+
#                                      |
#                     pipeline n+3 >---+  .....etc....more pipes can be mixed here
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
		reg "0"
		shift "0"
	}
	channel."FR" {
		reg "0"
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

SectionControlMixer.STR(Master Playback Volume) {

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
# Components and Buffers
#

# Host "Low latency Playback" PCM uses pipeline DMAC and channel
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(Low Latency Playback, PIPELINE_DMAC, PIPELINE_DMAC_CHAN, 2, 0, 2)

# "Playback Volume" has 1 sink period and 2 source periods for host ping-pong
W_PGA(0, PCM PCM_ID Playback Volume, PIPELINE_FORMAT, 1, 2, 1)

# "Master Playback Volume" has 1 source and 2 sink periods for DAI ping-pong
W_PGA(1, Master Playback Volume, PIPELINE_FORMAT, 2, 1, 1)

# Mixer 0 has 1 sink and source periods.
W_MIXER(0, PIPELINE_FORMAT, 1, 1, 1)

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES))
W_BUFFER(1, COMP_BUFFER_SIZE(1,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,SCHEDULE_FRAMES))
W_BUFFER(2, COMP_BUFFER_SIZE(1,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES))
W_BUFFER(3, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES))

#
# Pipeline Graph
#
#  host PCM_P --B0--> volume(0P) --B1--+
#                                      |--ll mixer(M) --B2--> volume(LL) ---B3-->  sink DAI0
#                     pipeline n+1 >---+
#                                      |
#                     pipeline n+2 >---+
#                                      |
#                     pipeline n+3 >---+  .....etc....more pipes can be mixed here
#

SectionGraph."pipe-ll-playback-PIPELINE_ID" {
	index STR(PIPELINE_ID)

	lines [
		dapm(N_PCM, Low Latency Playback PCM_ID)
		dapm(N_BUFFER(0), N_PCM)
		dapm(N_PGA(0), N_BUFFER(0))
		dapm(N_BUFFER(1), N_PGA(0))
		dapm(N_MIXER(0), N_BUFFER(1))
		dapm(N_BUFFER(2), N_MIXER(0))
		dapm(N_PGA(1), N_BUFFER(2))
		dapm(N_BUFFER(3), N_PGA(1))
	]
}

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(3))
indir(`define', concat(`PIPELINE_MIXER_', PIPELINE_ID), N_MIXER(0))

#
# Pipeline Configuration.
#

W_PIPELINE(N_PGA(1), SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, pipe_ll_schedule_plat)

#
# PCM Configuration
#

# PCM capabilities supported by FW
SectionPCMCapabilities.STR(Low Latency Playback PCM_ID) {

	formats "S24_LE,S16_LE"
	rate_min "48000"
	rate_max "48000"
	channels_min "2"
	channels_max "2"
	periods_min "2"
	periods_max "16"
	period_size_min	"192"
	period_size_max	"16384"
	buffer_size_min	"65536"
	buffer_size_max	"65536"
}

# PCM Low Latency Playback
SectionPCM.STR(PCM PCM_ID) {

	index STR(PIPELINE_ID)

	# used for binding to the PCM
	id STR(PCM_ID)

	dai.STR(Low Latency Playback PCM_ID) {
		id STR(PCM_ID)
	}

	# Playback and Capture Configuration
	pcm."playback" {

		capabilities STR(Low Latency Playback PCM_ID)
	}
}
