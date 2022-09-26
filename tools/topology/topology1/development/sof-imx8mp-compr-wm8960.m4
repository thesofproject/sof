#
# Topology with codec_adapter processing component for i.MX8MP
# supporting following codecs: MP3, AAC.

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
# PCM0 <----> volume <-----> SAI3 (wm8960)
#


DECLARE_SOF_RT_UUID("Cadence Codec", cadence_codec_uuid, 0xd8218443, 0x5ff3,
                    0x4a4c, 0xb3, 0x88, 0x6c, 0xfe, 0x07, 0xb9, 0x56, 0xaa);

define(`CA_UUID', cadence_codec_uuid)

# Post process setup config
define(`CA_SETUP_CONTROLBYTES',
``	bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`	0x24,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x07,0x00,0x02,0x00,0x0C,0x00,0x00,0x00,'
`	0x18,0x00,0x00,0x00,0x03,0x00,0x02,0x00,'
`	0x0C,0x00,0x00,0x00,0x02,0x00,0x00,0x00,'
`	0x00,0x00,0x06,0x00,0x0C,0x00,0x00,0x00,'
`	0x20,0x00,0x00,0x00"''
)

define(`CA_SETUP_CONTROLBYTES_MAX', 300)

undefine(`DAI_PERIODS')
define(`DAI_PERIODS', 8)

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-codec-adapter-playback.m4,
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

# playback DAI is SAI3 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SAI, 3, sai3-wm8960-hifi,
	PIPELINE_SOURCE_1, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)


# PCM Low Latency, id 0

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
COMPR_PLAYBACK_ADD(Port0, 0, PIPELINE_PCM_1)

dnl DAI_CONFIG(type, idx, link_id, name, sai_config)
DAI_CONFIG(SAI, 3, 0, sai3-wm8960-hifi,
	SAI_CONFIG(I2S, SAI_CLOCK(mclk, 12288000, codec_mclk_in),
		SAI_CLOCK(bclk, 3072000, codec_master),
		SAI_CLOCK(fsync, 48000, codec_master),
		SAI_TDM(2, 32, 3, 3),
		SAI_CONFIG_DATA(SAI, 3, 0)))
