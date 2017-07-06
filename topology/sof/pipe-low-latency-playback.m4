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

W_PCM_PLAYBACK(Low Latency Playback)
W_PGA(Playback Volume)
W_PGA(Mixer Volume)
W_MIXER(0)
W_BUFFER(0, BUF_PCM_SIZE)
W_BUFFER(1, BUF_INT_SIZE)
W_BUFFER(2, BUF_INT_SIZE)
W_BUFFER(3, BUF_INT_SIZE)

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
		dapm(N_BUFFER(0), N_PCM)
		dapm(N_PGA(Playback Volume), N_BUFFER(0))
		dapm(N_BUFFER(1), N_PGA(Playback Volume))
		dapm(N_MIXER(0), N_BUFFER(1))
		dapm(N_BUFFER(2), N_MIXER(0))
		dapm(N_PGA(Mixer Volume), N_BUFFER(2))
		dapm(N_BUFFER(3), N_PGA(Mixer Volume))
	]
}

#
# Pipeline Configuration.
#

W_PIPELINE(N_PGA(Mixer Volume), SCHEDULE_DEADLINE, pipe_ll_schedule_plat)

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
	periods_max "4"
	period_size_min	"192"
	period_size_max	"16384"
	buffer_size_min	"384"
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
