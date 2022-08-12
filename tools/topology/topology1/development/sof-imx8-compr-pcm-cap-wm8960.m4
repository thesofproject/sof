#
# Topology with codec_adapter processing component for i.MX8QM/i.MX8QXP
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`sai.m4')
include(`pcm.m4')
include(`buffer.m4')

# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

# Include DSP configuration
include(`platform/imx/imx8.m4')


# Post process setup config

 #codec Post Process setup config
#
# Define the pipelines
#
# PCM0 <---- volume <----- SAI3 (wm8960)
#

DECLARE_SOF_RT_UUID("passthrough_codec", passthrough_uuid, 0x376b5e44, 0x9c82,
		0x4ec2, 0xbc, 0x83, 0x10, 0xea, 0x10, 0x1a, 0xf8, 0x8f);

define(`CA_UUID', passthrough_uuid)

# Post process setup config for playback
define(`CA_SETUP_CONTROLBYTES',
``	bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00"''
)

define(`CA_SETUP_CONTROLBYTES_MAX', 300)

undefine(`DAI_PERIODS')
define(`DAI_PERIODS', 2)

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency capture pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-codec-adapter-capture.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period, priority, core, time_domain)

# capture DAI is SAI3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	1, SAI, 1, sai1-wm8960-hifi,
	PIPELINE_SINK_1, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)

# PCM Low Latency, id 0

dnl COMPR_CAPTURE_ADD(name, pcm_id, capture)
COMPR_CAPTURE_ADD(Port0, 0, PIPELINE_PCM_1)

dnl DAI_CONFIG(type, idx, link_id, name, sai_config)
DAI_CONFIG(SAI, 1, 0, sai1-wm8960-hifi,
	SAI_CONFIG(I2S, SAI_CLOCK(mclk, 12288000, codec_mclk_in),
		SAI_CLOCK(bclk, 3072000, codec_master),
		SAI_CLOCK(fsync, 48000, codec_master),
		SAI_TDM(2, 32, 3, 3),
		SAI_CONFIG_DATA(SAI, 1, 0)))
