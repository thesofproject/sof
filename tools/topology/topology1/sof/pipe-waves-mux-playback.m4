# 2-way Pipeline with Waves codec
#
#  Low Latency Playback with Waves codec and Mux.
#
# Pipeline Endpoints for connection are :-
#
#      B0 (sink endpoint)
#      Mux
#      B2 (source endpoint)
#
#  source -- B0 --> Waves(Tweeter) --> B1 --> Mux --> B2 --> sink DAI
#

DECLARE_SOF_RT_UUID("Waves codec", waves_codec_uuid, 0xd944281a, 0xafe9,
		    0x4695, 0xa0, 0x43, 0xd7, 0xf6, 0x2b, 0x89, 0x53, 0x8e);
define(`CA_UUID', waves_codec_uuid)

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`muxdemux.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`codec_adapter.m4')

ifdef(`ENDPOINT_NAME',`',`fatal_error(`Pipe requires ENDPOINT_NAME to be defined: Speakers, Headphones, etc.')')

# Mux Bytes control with max value of 255
C_CONTROLBYTES(concat(`MUX', PIPELINE_ID), PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,       concat(`mux_priv_', PIPELINE_ID))

define(`SETUP_PARAMS_NAME', `Waves' `ENDPOINT_NAME' `Setup Tweeter')

CONTROLBYTES_PRIV(PP_SETUP_CONFIG,
`       bytes "0x53,0x4f,0x46,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x20,0x00,0x00,0x00,'
`       0x00,0x10,0x00,0x03,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'

`       0x00,0x01,0x41,0x57,'
`       0x00,0x00,0x00,0x00,'
`       0x80,0xBB,0x00,0x00,'
`       0x20,0x00,0x00,0x00,'
`       0x02,0x00,0x00,0x00,'

`       0x00,0x00,0x00,0x00,'
`       0x0c,0x00,0x00,0x00,'

`       0x00,0x00,0x00,0x00"'
)

# Post process Bytes control for setup config
C_CONTROLBYTES(SETUP_PARAMS_NAME, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes),
	CONTROLBYTES_EXTOPS(void, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 8192),
	,
	PP_SETUP_CONFIG)

define(`RUNTIME_PARAMS_NAME', `Waves' `ENDPOINT_NAME' `Runtime Tweeter')

CONTROLBYTES_PRIV(PP_RUNTIME_PARAMS,
`       bytes "0x53,0x4f,0x46,0x00,'
`       0x01,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x10,0x00,0x03,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00"'
)

# Post process Bytes control for runtime config
C_CONTROLBYTES(RUNTIME_PARAMS_NAME, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes),
	CONTROLBYTES_EXTOPS(void, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 8192),
	,
	PP_RUNTIME_PARAMS)

#
# Components and Buffers
#

W_CODEC_ADAPTER(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`          ', "RUNTIME_PARAMS_NAME", "SETUP_PARAMS_NAME"))

W_MUXDEMUX(0, 0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`         ', concat(`MUX', PIPELINE_ID)))

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), 4, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Graph connections to pipelines
#
#  source -- B0 --> Waves(Tweeter) --> B1 --> Mux --> B2 --> sink DAI
#

P_GRAPH(pipe-playback-sched-PIPELINE_ID, PIPELINE_ID,
	LIST(`          ',
	`dapm(N_CODEC_ADAPTER(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_CODEC_ADAPTER(0))',
	`dapm(N_MUXDEMUX(0), N_BUFFER(1))',
	`dapm(N_BUFFER(2), N_MUXDEMUX(0))'))

indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_MUX_', PIPELINE_ID), N_MUXDEMUX(0))
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(0))
