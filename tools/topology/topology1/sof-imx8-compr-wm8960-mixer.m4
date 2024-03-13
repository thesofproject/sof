#
# Topology for i.MX8QXP/i.MX8QM/i.MX8MP boards mixing PCM and Compress streams
#
# CODEC: wm8960, wm8962
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`pcm.m4')
include(`sai.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include DSP configuration
include(`platform/imx/imx8.m4')

DECLARE_SOF_RT_UUID("Cadence Codec", cadence_codec_uuid, 0xd8218443, 0x5ff3,
                    0x4a4c, 0xb3, 0x88, 0x6c, 0xfe, 0x07, 0xb9, 0x56, 0xaa);

define(`CA_UUID', cadence_codec_uuid)

# Post process setup config
define(`CA_SETUP_CONTROLBYTES',
``	bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`	0x0C,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,'
`	0x20,0x00,0x00,0x00"''
)

define(`CA_SETUP_CONTROLBYTES_MAX', 300)

undefine(`DAI_PERIODS')
define(`DAI_PERIODS', 8)

#
# Define the pipelines
#
# PCM0 -----> volume -----------------v
#                                     low latency mixer ----> volume ----> `SAI_INDEX'
# Compr1 -----> Codec Adapter  -------^
# PCM0 <---- Volume <---- `SAI_INDEX'
#

# Low Latency capture pipeline 2 on PCM 0 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-low-latency-capture.m4,
	2, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAI configuration
#
# SAI port `SAI_INDEX' is our only pipeline DAI
#

# define STREAM_NAME, based on CODEC name
define(`STREAM_NAME',
	`ifelse(CODEC, `wm8960', `-wm8960-hifi',
			CODEC, `wm8962', `-wm8962',
			`fatal_error(`Codec not supported.')')')

# define DAI BE dai_link name
define(`DAI_BE_NAME', concat(concat(`sai', SAI_INDEX), STREAM_NAME))

# playback DAI is SAI`SAI_INDEX' using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 1
# this defines pipeline 1. The 'NOT_USED_IGNORED' is due to dependencies
# and is adjusted later with an explicit dapm line.
DAI_ADD(sof/pipe-mixer-volume-dai-playback.m4,
	1, SAI, SAI_INDEX, DAI_BE_NAME,
	NOT_USED_IGNORED, 2, s32le,
	1000, 1, 0, SCHEDULE_TIME_DOMAIN_TIMER,
	2, 48000)

# PCM Playback pipeline 3 on PCM 0 using max 2 channels of s32le.
# 1000us deadline on core 0 with priority 0
# this is connected to pipeline DAI 1
PIPELINE_PCM_ADD(sof/pipe-host-volume-playback.m4,
	3, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Compress Playback pipeline 4 on Compr 1 using max 2 channels of s32le.
# 5ms deadline on core 0 with priority 0
# this is connected to pipeline DAI 1
PIPELINE_PCM_ADD(sof/pipe-host-codec-adapter-playback.m4,
	4, 1, 2, s32le,
	5000, 0, 0,
	48000, 48000, 48000,
	SCHEDULE_TIME_DOMAIN_TIMER,
	PIPELINE_PLAYBACK_SCHED_COMP_1)

# Connect pipelines together
SectionGraph."PIPE_NAME" {
	index "0"

	lines [
		# PCM pipeline 3 to DAI pipeline 1
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_3)
		# PCM pipeline 4 to DAI pipeline 1
		dapm(PIPELINE_MIXER_1, PIPELINE_SOURCE_4)

	]
}

# capture DAI is SAI`SAI_INDEX' using 2 periods
# Buffers use s32le format, 1000us deadline on core 0 with priority 0
# this is part of pipeline 2
DAI_ADD(sof/pipe-dai-capture.m4,
	2, SAI, SAI_INDEX, DAI_BE_NAME,
	PIPELINE_SINK_2, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)


# PCM definitions
PCM_DUPLEX_ADD(PCM, 0, PIPELINE_PCM_3, PIPELINE_PCM_2)
COMPR_PLAYBACK_ADD(PCM Deep Buffer, 1, PIPELINE_PCM_4)

#
# BE configurations
#
DAI_CONFIG(SAI, SAI_INDEX, 0, DAI_BE_NAME,
	   SAI_CONFIG(I2S, SAI_CLOCK(mclk, 12288000, codec_mclk_in),
		      SAI_CLOCK(bclk, 3072000, codec_provider),
		      SAI_CLOCK(fsync, 48000, codec_provider),
		      SAI_TDM(2, 32, 3, 3),
		      SAI_CONFIG_DATA(SAI, SAI_INDEX, 0)))
