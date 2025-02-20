# Low Latency Waves codec and CTC Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
# host PCM_P --> B0 --> Waves codec --> B1 --> CTC --> B2 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`google_ctc_audio_processing.m4')

ifdef(`ENDPOINT_NAME',`',`fatal_error(`Pipe requires ENDPOINT_NAME to be defined: Speakers, Headphones, etc.')')

# Waves codec setup config
define(`CA_SETUP_CONTROLBYTES',
``      bytes "0x53,0x4f,0x46,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x0C,0x00,0x00,0x00,'
`       0x00,0x10,0x00,0x03,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'

`       0x00,0x00,0x00,0x00,'
`       0x0c,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00"''
)
define(`CA_SETUP_CONTROLBYTES_MAX', 8192)
define(`CA_SETUP_CONTROLBYTES_NAME', `Waves' `ENDPOINT_NAME' `Setup ')

define(`CA_SCHEDULE_CORE', 0)

DECLARE_SOF_RT_UUID("Waves codec", waves_codec_uuid, 0xd944281a, 0xafe9,
		    0x4695, 0xa0, 0x43, 0xd7, 0xf6, 0x2b, 0x89, 0x53, 0x8e);
define(`CA_UUID', waves_codec_uuid)

# Include codec_adapter after CA_UUID definition
include(`codec_adapter.m4')

define(CA_SETUP_CONFIG, concat(`ca_setup_config_', PIPELINE_ID))
define(CA_SETUP_CONTROLBYTES_NAME_PIPE, concat(CA_SETUP_CONTROLBYTES_NAME, PIPELINE_ID))

# Codec adapter setup config
CONTROLBYTES_PRIV(CA_SETUP_CONFIG, CA_SETUP_CONTROLBYTES)

# Codec adapter Bytes control for setup config
C_CONTROLBYTES(CA_SETUP_CONTROLBYTES_NAME_PIPE, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes),
	CONTROLBYTES_EXTOPS(void, 258, 258),
	, , ,
	CONTROLBYTES_MAX(void, CA_SETUP_CONTROLBYTES_MAX),
	,
	CA_SETUP_CONFIG)

define(DEF_CTC_PRIV, concat(`ctc_priv_', PIPELINE_ID))
define(DEF_CTC_BYTES, concat(`ctc_bytes_', PIPELINE_ID))
include(`google_ctc_audio_processing_coef_default.m4')

# CTC Bytes control
C_CONTROLBYTES(DEF_CTC_BYTES, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(void, 4244),
	,
	DEF_CTC_PRIV)

#
# Components and Buffers
#

# For codec developers, please define the schedule core of codec adapter if it you would like it to
# be different from SCHEDULE_CORE.
ifdef(`CA_SCHEDULE_CORE',`', `define(`CA_SCHEDULE_CORE', `SCHEDULE_CORE')')

# Host "Playback with codec adapter" PCM
# with DAI_PERIODS sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Passthrough Playback, DAI_PERIODS, 0, SCHEDULE_CORE)

W_CODEC_ADAPTER(0, PIPELINE_FORMAT, DAI_PERIODS, DAI_PERIODS, CA_SCHEDULE_CORE,
        LIST(`          ', "CA_SETUP_CONTROLBYTES_NAME_PIPE"))

W_GOOGLE_CTC_AUDIO_PROCESSING(0, PIPELINE_FORMAT, DAI_PERIODS, DAI_PERIODS, SCHEDULE_CORE,
    LIST(`      ', "DEF_CTC_BYTES"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(DAI_PERIODS,
    COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_HOST_MEM_CAP, SCHEDULE_CORE)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
    COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_DAI_MEM_CAP, SCHEDULE_CORE)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
    COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_DAI_MEM_CAP, SCHEDULE_CORE)

#
# Pipeline Graph
#
# host PCM_P --> B0 --> Waves codec --> B1 --> CTC --> B2 --> sink DAI0

P_GRAPH(pipe-codec-adapter-ctc-playback, PIPELINE_ID,
    LIST(`      ',
    `dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
    `dapm(N_CODEC_ADAPTER(0), N_BUFFER(0))',
    `dapm(N_BUFFER(1), N_CODEC_ADAPTER(0))',
    `dapm(N_GOOGLE_CTC_AUDIO_PROCESSING(0), N_BUFFER(1))',
    `dapm(N_BUFFER(2), N_GOOGLE_CTC_AUDIO_PROCESSING(0))',
))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Passthrough Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_CTC_PRIV')
undefine(`DEF_CTC_BYTES')

undefine(`CA_SETUP_CONTROLBYTES_NAME_PIPE')
undefine(`CA_SETUP_PARAMS')

undefine(`CA_SCHEDULE_CORE')
undefine(`CA_SETUP_CONTROLBYTES_NAME')
undefine(`CA_SETUP_CONTROLBYTES_MAX')
undefine(`CA_SETUP_CONTROLBYTES')
