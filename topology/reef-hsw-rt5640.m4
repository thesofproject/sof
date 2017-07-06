#
# Topology for generic Haswell board with no codec.
#

# Include topology builder
include(`local.m4')
include(`build.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include Haswell DSP configuration
include(`dsps/hsw.m4')

#
# Define the pipelines
#

# Low Latency playback pipeline 1 on PCM 0
PIPELINE_PCM_ADD(sof/pipe-low-latency-playback.m4, 1, 0, 768, 768, 48, 1000, 0)

# Low Latency capture pipeline 2 on PCM 1
PIPELINE_PCM_ADD(sof/pipe-low-latency-capture.m4, 2, 1, 768, 768, 48, 1000, 0)

# PCM Media Playback pipeline 3 on PCM 2
PIPELINE_PCM_ADD(sof/pipe-pcm-media.m4, 3, 2, 1536, 1536, 96, 2000, 1)

# PCM Media Playback pipeline 4 on PCM 3
PIPELINE_PCM_ADD(sof/pipe-pcm-media.m4, 4, 3, 1536, 1536, 96, 2000, 1)

# Tone Playback pipeline 5
PIPELINE_ADD(sof/pipe-tone.m4, 5, 3072, 192, 4000, 2)


#
# DAI configuration
#

# SSP port 2 is our pipeline DAI
DAI_ADD(sof/pipe-dai.m4, SSP2, I2S Audio, NPIPELINE_BUFFER(2, 0), NPIPELINE_BUFFER(1, 3))

# Connect pipelines together
SectionGraph."pipe-hsw-rt5640" {
	index "0"

	lines [
		# media 0
		dapm(NPIPELINE_MIXER(1, 0), NPIPELINE_BUFFER(3, 2))
		# media 1
		dapm(NPIPELINE_MIXER(1, 0), NPIPELINE_BUFFER(4, 2))
		#tone
		dapm(NPIPELINE_MIXER(1, 0), NPIPELINE_BUFFER(5, 1))
	]
}


#
# BE configurations - overrides config in ACPI if present
#
SectionHWConfig."SSP0" {

	id 		"2"
	format 		"I2S"
	bclk_master	"true"
	bclk_freq	"2400000"
	mclk_freq	"24000000"
	fsyn_master	"true"
	fsync_freq	"48000"
	tdm_slots	"2"
	tdm_slot_width	"25"
	tx_slots	"2"
	rx_slots	"2"
}
