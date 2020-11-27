# Low Latency Passthrough Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`codec_adapter.m4')
include(`bytecontrol.m4')

ifdef(`PP_CORE',`', `define(`PP_CORE', 1)')
undefine(`DAI_PERIODS')
define(`DAI_PERIODS', 8)

#
# Controls
#

# Post process setup config
CONTROLBYTES_PRIV(PP_SETUP_CONFIG,
`	bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`	0x5C,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x01,0x01,0xDE,0xCA,0x00,0x00,0x00,0x00,'
`	0x80,0xBB,0x00,0x00,0x20,0x00,0x00,0x00,'
`	0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x0C,0x00,0x00,0x00,0x40,0x00,0x00,0x00,'
`	0x01,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,'
`	0x80,0xBB,0x00,0x00,0x02,0x00,0x00,0x00,'
`	0x0C,0x00,0x00,0x00,0x20,0x00,0x00,0x00,'
`	0x03,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,'
`	0x01,0x00,0x00,0x00,0x04,0x00,0x00,0x00,'
`	0x0C,0x00,0x00,0x00,0x02,0x00,0x00,0x00,'
`	0x05,0x00,0x00,0x00,0x0C,0x00,0x00,0x00,'
`	0x02,0x00,0x00,0x00"'
)

# Post process Bytes control for setup config
C_CONTROLBYTES(Post Process Setup Config, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes),
	CONTROLBYTES_EXTOPS(void, 258, 258),
	, , ,
	CONTROLBYTES_MAX(void, 300),
	,
	PP_SETUP_CONFIG)


# Post process runtime params
CONTROLBYTES_PRIV(PP_RUNTIME_PARAMS,
`	bytes "0x53,0x4f,0x46,0x00,0x01,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00"'
)

# Post process Bytes control for runtime config
C_CONTROLBYTES(Post Process Runtime Params, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes),
        CONTROLBYTES_EXTOPS(void, 258, 258),
        , , ,
        CONTROLBYTES_MAX(void, 157),
        ,
        PP_RUNTIME_PARAMS)

#
# Components and Buffers
#

# Host "Playback with post processing" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Passthrough Playback, DAI_PERIODS, 0, SCHEDULE_CORE)

W_CODEC_ADAPTER(0, PIPELINE_FORMAT, DAI_PERIODS, DAI_PERIODS, PP_CORE,
        LIST(`          ', "Post Process Setup Config", "Post Process Runtime Params"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(DAI_PERIODS,
    COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_HOST_MEM_CAP, SCHEDULE_CORE)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
    COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_DAI_MEM_CAP, SCHEDULE_CORE)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> CODEC_ADAPTER -> B1 --> sink DAI0

P_GRAPH(pipe-pass-playback-PIPELINE_ID, PIPELINE_ID,
    LIST(`      ',
    `dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
    `dapm(N_CODEC_ADAPTER(0), N_BUFFER(0))',
    `dapm(N_BUFFER(1), N_CODEC_ADAPTER(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Passthrough Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)
