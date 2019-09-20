#
# Topology for generic Apollolake UP^2 with pcm512x codec and HDMI - guest part.
#

# Include topology builder
include(`utils.m4')
include(`pcm.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Apollolake DSP configuration
include(`platform/intel/bxt.m4')

DEBUG_START

# Define the pipelines
#
# PCM0 ----> volume (-----> SSP5 (pcm512x))
# PCM0 (<----- SSP5 (pcm512x))

# Virtual playback PCM
SectionVendorTuples."VIO3.0_comp_tuples" {
	tokens "sof_comp_tokens"
	tuples."word" {
		SOF_TKN_COMP_REF	"11"
	}
}
SectionData."VIO3.0_comp_data" {
	tuples "VIO3.0_comp_tuples"
}
SectionVendorTuples."VIO3.0_dai_tuples" {
	tokens "sof_dai_tokens"
	tuples."word" {
		SOF_TKN_DAI_DIRECTION	"0"
	}
}
SectionData."VIO3.0_dai_data" {
	tuples "VIO3.0_dai_tuples"
}
SectionWidget."SSP5.OUT" {
	index "3"
	type "dai_in"
	no_pm "true"
	stream_name "NoCodec-0"
	data [
		"VIO3.0_comp_data"
		"VIO3.0_dai_data"
	]
}

# Virtual capture PCM
SectionVendorTuples."VIO5.0_comp_tuples" {
	tokens "sof_comp_tokens"
	tuples."word" {
		SOF_TKN_COMP_REF	"2"
	}
}
SectionData."VIO5.0_comp_data" {
	tuples "VIO5.0_comp_tuples"
}
SectionVendorTuples."VIO5.0_dai_tuples" {
	tokens "sof_dai_tokens"
	tuples."word" {
		SOF_TKN_DAI_DIRECTION	"1"
	}
}
SectionData."VIO5.0_dai_data" {
	tuples "VIO5.0_dai_tuples"
}
SectionWidget."SSP5.IN" {
	index "5"
	type "dai_out"
	no_pm "true"
	stream_name "NoCodec-0"
	data [
		"VIO5.0_comp_data"
		"VIO5.0_dai_data"
	]
}

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

# Low Latency playback pipeline 3 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-vm-playback.m4,
	3, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

W_PIPELINE(SSP5.OUT, 1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER, pipe_media_schedule_plat)

P_GRAPH(SSP5-vm-playback-3, 3,
	LIST(`		',
	`dapm(SSP5.OUT, BUF3.1)',
	))

# Low Latency capture pipeline 5 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-vm-capture.m4,
	5, 1, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

W_PIPELINE(SSP5.IN, 1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER, pipe_media_schedule_plat)

P_GRAPH(SSP5-vm-capture-5, 5,
	LIST(`		',
	`dapm(BUF5.1, SSP5.IN)',
	))

# PCM Low Latency, id 0
dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port5, 0, PIPELINE_PCM_3, PIPELINE_PCM_5)

DEBUG_END
