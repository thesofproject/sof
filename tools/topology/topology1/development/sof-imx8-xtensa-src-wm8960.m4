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

# codec Post Process setup config
#
# Define the pipelines
#
# PCM0_P --> B0 --> CODEC_ADAPTER --> B1 --> SAI1 (wm8960)
#


DECLARE_SOF_RT_UUID("Cadence Codec", cadence_codec_uuid, 0xd8218443, 0x5ff3,
                    0x4a4c, 0xb3, 0x88, 0x6c, 0xfe, 0x07, 0xb9, 0x56, 0xaa);

define(`CA_UUID', cadence_codec_uuid)

# Codec Adapter setup config control bytes (little endian)
#  : bytes "abi_header, ca_config, [codec_param0, codec_param1...]"
#  - 32 bytes abi_header: you could get by command "sof-ctl -t 0 -g <payload_size> -b"
#    - [0:3]: magic number 0x00464f53
#    - [4:7]: type 0
#    - [8:11]: payload size in bytes (not including abi header bytes)
#    - [12:15]: abi 3.1.0
#    - [16:31]: reserved 0s
#  - 20 bytes ca_config: codec adapter setup config parameters, for more details please refer
#                        struct ca_config under audio/codec_adapter/codec/generic.h
#    - [0]: API ID, e.g. 0x01
#    - [1:3]: codec ID, e.g. 0xd03311
#    - [4:7]: reserved 0s
#    - [8:11]: sample rate, e.g. 48000
#    - [12:15]: sample width in bits, e.g. 32
#    - [16:19]: channels, e.g. 2
# - (optional) 12+ bytes codec_param: codec TLV parameters container, for more details please refer
#                                     struct codec_param under audio/codec_adapter/codec/generic.h
#    - [0:3]: param ID
#    - [4:7]: size in bytes (ID + size + data)
#    - [8:n-1]: data[], the param data

# Post process setup config
#
# Last 60 bytes set some SRC parameters:
# XA_SRC_PP_CONFIG_PARAM_INPUT_SAMPLE_RATE, with param ID = 0, to 44100 Hz (AC44)
# XA_SRC_PP_CONFIG_PARAM_OUTPUT_SAMPLE_RATE, with param ID = 1, to 48000 Hz (BB80)
# XA_SRC_PP_CONFIG_PARAM_INPUT_CHUNK_SIZE, with param ID = 2, to 96 samples, since we have 48kHz input sample rate and 2 bytes per sample (we use S16 format)
# XA_SRC_PP_CONFIG_PARAM_INPUT_CHANNELS, with param ID = 4, to 2 channels
# XA_SRC_PP_CONFIG_PARAM_BYTES_PER_SAMPLE, with param ID = 8, to 2 bytes per sample since we use S16 format
define(`CA_SETUP_CONTROLBYTES',
``	bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`	0x50,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x09,0x01,0xDE,0xCA,0x00,0x00,0x00,0x00,'
`	0x80,0xBB,0x00,0x00,0x20,0x00,0x00,0x00,'
`	0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x0C,0x00,0x00,0x00,0x44,0xAC,0x00,0x00,'
`	0x01,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,'
`	0x80,0xBB,0x00,0x00,0x02,0x00,0x00,0x00,'
`	0x0C,0x00,0x00,0x00,0x60,0x00,0x00,0x00,'
`	0x04,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,'
`	0x02,0x00,0x00,0x00,0x08,0x00,0x00,0x00,'
`	0x0C,0x00,0x00,0x00,0x02,0x00,0x00,0x00"''
)

define(`CA_SETUP_CONTROLBYTES_MAX', 300)

dnl PIPELINE_PCM_ADD(pipeline,
dnl     pipe id, pcm, max channels, format,
dnl     period, priority, core,
dnl     pcm_min_rate, pcm_max_rate, pipeline_rate,
dnl     time_domain, sched_comp)

# Low Latency playback pipeline 1 on PCM 0 using max 2 channels of s32le.
# Set 1000us deadline on core 0 with priority 0
PIPELINE_PCM_ADD(sof/pipe-codec-adapter-playback.m4,
	1, 0, 2, s32le,
	1000, 0, 0,
	8000, 192000, 48000)

#
# DAIs configuration
#

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period, priority, core, time_domain)

# playback DAI is SAI1 using 2 periods
# Buffers use s32le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	1, SAI, 1, sai1-wm8960-hifi,
	PIPELINE_SOURCE_1, 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)


# PCM Low Latency, id 0

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
PCM_PLAYBACK_ADD(Port0, 0, PIPELINE_PCM_1)

dnl DAI_CONFIG(type, idx, link_id, name, sai_config)
DAI_CONFIG(SAI, 1, 0, sai1-wm8960-hifi,
	SAI_CONFIG(I2S, SAI_CLOCK(mclk, 12288000, codec_mclk_in),
		SAI_CLOCK(bclk, 3072000, codec_master),
		SAI_CLOCK(fsync, 48000, codec_master),
		SAI_TDM(2, 32, 3, 3),
		SAI_CONFIG_DATA(SAI, 1, 0)))
