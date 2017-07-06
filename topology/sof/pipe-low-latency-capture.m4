# Low Latency Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <--B5-- volume(0C) <--B4-- source DAI0

# Include topology builder
include(`local.m4')

#
# Controls
#

SectionControlMixer.STR(PCM PCM_ID Capture Volume) {

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

	# TLV 40 steps from -90dB to +20dB for 3dB
	max "40"
	invert "false"
	tlv "vtlv_m90s3"
}

#
# Components and Buffers
#

W_PCM_CAPTURE(Low Latency Capture)
W_PGA(Capture Volume)
W_BUFFER(0, BUF_INT_SIZE)
W_BUFFER(1, BUF_PCM_SIZE)

#
# Pipeline Graph
#
#  host PCM <--B1-- volume( <--B0-- source DAI0

SectionGraph."pipe-ll-capture-PIPELINE_ID" {
	index STR(PIPELINE_ID)

	lines [
		dapm(N_PCM, N_BUFFER(1))
		dapm(N_BUFFER(1), N_PGA(Capture Volume))
		dapm(N_PGA(Capture Volume), N_BUFFER(0))
	]
}

#
# Pipeline Configuration.
#

W_PIPELINE(N_PGA(Capture Volume), SCHEDULE_DEADLINE, pipe_ll_schedule_plat)

#
# PCM Configuration
#

SectionPCMCapabilities.STR(Low Latency Capture PCM_ID) {

	formats "S24_LE,S16_LE"
	rate_min "48000"
	rate_max "48000"
	channels_min "2"
	channels_max "4"
	periods_min "2"
	periods_max "4"
	period_size_min	"192"
	period_size_max	"16384"
	buffer_size_min	"384"
	buffer_size_max	"65536"
}

# PCM Low Latency Capture
SectionPCM.STR(PCM PCM_ID) {

	index STR(PIPELINE_ID)

	# used for binding to the PCM
	id STR(PCM_ID)

	dai.STR(Low Latency Capture PCM_ID) {
		id STR(PCM_ID)
	}

	pcm."capture" {

		capabilities STR(Low Latency Capture PCM_ID)
	}
}
