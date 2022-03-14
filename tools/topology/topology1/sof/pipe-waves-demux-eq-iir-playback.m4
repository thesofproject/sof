# Demux EQ band split pipeline with Waves codec
#
# host PCM_P --B0--> Waves --B1--> demux(M) --B2--> eq_iir --B3--> sink DAI0
#

DECLARE_SOF_RT_UUID("Waves codec", waves_codec_uuid, 0xd944281a, 0xafe9,
		    0x4695, 0xa0, 0x43, 0xd7, 0xf6, 0x2b, 0x89, 0x53, 0x8e);
define(`CA_UUID', waves_codec_uuid)

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`muxdemux.m4')
include(`bytecontrol.m4')
include(`eq_iir.m4')
include(`codec_adapter.m4')

ifdef(`ENDPOINT_NAME',`',`fatal_error(`Pipe requires ENDPOINT_NAME to be defined: Speakers, Headphones, etc.')')

define(`SETUP_PARAMS_NAME', `Waves' `ENDPOINT_NAME' `Setup' PIPELINE_ID)

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

define(`RUNTIME_PARAMS_NAME', `Waves' `ENDPOINT_NAME' `Runtime' PIPELINE_ID)

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

# demux Bytes control with max value of 255
C_CONTROLBYTES(concat(`DEMUX', PIPELINE_ID), PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,	concat(`demux_priv_', PIPELINE_ID))

# EQ IIR Bytes control
define(DEF_EQIIR_COEF, concat(`eqiir_coef_', PIPELINE_ID))
define(DEF_EQIIR_PRIV, concat(`eqiir_priv_', PIPELINE_ID))

# define filter. eq_iir_coef_bandsplit.m4 is set by default
ifdef(`PIPELINE_FILTER1', , `define(PIPELINE_FILTER1, eq_iir_coef_bandsplit.m4)')
include(PIPELINE_FILTER1)

C_CONTROLBYTES(DEF_EQIIR_COEF, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 1024),
	,
	DEF_EQIIR_PRIV)

#
# Components and Buffers
#

# Host "Speaker Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Speaker Playback, 2, 0, SCHEDULE_CORE)

W_CODEC_ADAPTER(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
        LIST(`          ', "RUNTIME_PARAMS_NAME", "SETUP_PARAMS_NAME"))

# "EQ IIR" has x sink period and 2 source periods
W_EQ_IIR(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`		', "DEF_EQIIR_COEF"))

# Mux 0 has 2 sink and source periods.
W_MUXDEMUX(0, 1, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`         ', concat(`DEMUX', PIPELINE_ID)))

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(3, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), 4, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Pipeline Graph
#
# host PCM_P --B0--> Waves --B1--> demux(M) --B2--> eq_iir --B3--> sink DAI0

P_GRAPH(pipe-demux-eq-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_CODEC_ADAPTER(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_CODEC_ADAPTER(0))',
	`dapm(N_MUXDEMUX(0), N_BUFFER(1))',
	`dapm(N_BUFFER(2), N_MUXDEMUX(0))',
	`dapm(N_EQ_IIR(0), N_BUFFER(2))',
	`dapm(N_BUFFER(3), N_EQ_IIR(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(3))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Speaker Playback PCM_ID)

#
# PCM Configuration
#


# PCM capabilities supported by FW
PCM_CAPABILITIES(Speaker Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), 48000, 48000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_EQIIR_COEF')
undefine(`DEF_EQIIR_PRIV')
