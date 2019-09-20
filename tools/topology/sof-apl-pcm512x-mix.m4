#
# Topology for generic Apollolake UP^2 with pcm512x codec and HDMI.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
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
# PCM0 ---> volume ---> mixer ---> volume ---> SSP5 (pcm512x)
# VIO2 ---------------->|
#
# PCM0 <--- volume <--- demux <--- SSP5
# VIO5 <----------------|
#
# Note: current DEMUX coefficients are hard-coded to multiplex pipelines 1 and
# 5. We reuse those pipeline IDs for host and guest capture pipelines to avoid
# breaking other DEMUX users. Therefore we use 2 and 3 for host and guest
# playback pipelines respectively.

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     frames, deadline, priority, core)

# Low Latency playback pipeline 2 on PCM 0 using max 2 channels of s32le.
# Schedule 48 frames per 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-low-latency-playback-1vol.m4,
	2, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Virtual playback PCM and buffer on pipeline 3 to be replaced by a guest
# pipeline
SectionWidget."VIO3.0" {
	index "3"
	type "aif_in"
	stream_name "VM FE Playback"
	no_pm "true"
}

SectionVendorTuples."BUF3.vm_tuples" {
	tokens "sof_buffer_tokens"
	tuples."word" {
		SOF_TKN_BUF_SIZE	"0"
		SOF_TKN_BUF_CAPS	"  113"
	}
}
SectionData."BUF3.vm_data" {
	tuples "BUF3.vm_tuples"
}
SectionWidget."BUF3.vm" {
	index "3"
	type "buffer"
	no_pm "true"
	data [
		"BUF3.vm_data"
	]
}

# Connect pipelines together
SectionGraph."VIO-playback" {
       index "3"

       lines [
               "BUF3.vm, , VIO3.0"
               "MIXER2.0, , BUF3.vm"
       ]
}

SectionPCMCapabilities."VM FE Playback" {
	channels_min "2"
	channels_max "2"
}

SectionPCM."vm_fe_playback" {
	id "30"

	dai."PortV0" {
		id "30"
	}

	# Playback Configuration
	pcm."playback" {

		capabilities "VM FE Playback"
	}
}

# Low Latency capture pipeline 1 on PCM 0 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-volume-capture-mux.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

# Virtual playback PCM and buffer on pipeline 5 to be replaced by a guest
# pipeline
SectionWidget."VIO5.0" {
	index "5"
	type "aif_out"
	stream_name "VM FE Capture"
	no_pm "true"
}

SectionVendorTuples."BUF5.vm_tuples" {
	tokens "sof_buffer_tokens"
	tuples."word" {
		SOF_TKN_BUF_SIZE	"0"
		SOF_TKN_BUF_CAPS	"  113"
	}
}
SectionData."BUF5.vm_data" {
	tuples "BUF5.vm_tuples"
}
SectionWidget."BUF5.vm" {
	index "5"
	type "buffer"
	no_pm "true"
	data [
		"BUF5.vm_data"
	]
}

# Connect pipelines together
SectionGraph."VIO-capture" {
       index "5"

       lines [
               "VIO5.0, , BUF5.vm"
               "BUF5.vm, , MUXDEMUX1.0"
       ]
}

SectionPCMCapabilities."VM FE Capture" {
	channels_min "2"
	channels_max "2"
}

SectionPCM."vm_fe_capture" {
	id "50"

	dai."PortV1" {
		id "50"
	}

	# Capture Configuration
	pcm."capture" {

		capabilities "VM FE Capture"
	}
}

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     frames, deadline, priority, core)

# playback DAI is SSP5 using 2 periods
# Buffers use s24le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	2, SSP, 5, SSP5-Codec,
	PIPELINE_SOURCE_2, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP5 using 2 periods
# Buffers use s16le format, 1000us deadline on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	1, SSP, 5, SSP5-Codec,
	PIPELINE_SINK_1, 2, s24le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# PCM Low Latency, id 0
dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_DUPLEX_ADD(Port5, 0, PIPELINE_PCM_2, PIPELINE_PCM_1)

#
# BE configurations - overrides config in ACPI if present
#

#SSP 5 (ID: 0)
DAI_CONFIG(SSP, 5, 0, SSP5-Codec,
	SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		SSP_CLOCK(bclk, 3072000, codec_slave),
		SSP_CLOCK(fsync, 48000, codec_slave),
		SSP_TDM(2, 32, 3, 3),
		SSP_CONFIG_DATA(SSP, 5, 24)))

DEBUG_END
