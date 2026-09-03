# Low Latency Passthrough Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> EQ 0 --> B1 --> CODEC_ADAPTER -> B2 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`codec_adapter.m4')
include(`bytecontrol.m4')
include(`eq_iir.m4')

#
# Controls
#

# For codec developers, please define the following control bytes of setup config and runtime params
# and the corresponding max bytenum on your own.

# Codec Adapter setup config control bytes (little endian)
#  : bytes "abi_header, [codec_param0, codec_param1...]"
#  - 32 bytes abi_header: you could get by command "sof-ctl -t 0 -g <payload_size> -b"
#    - [0:3]: magic number 0x00464f53
#    - [4:7]: type 0
#    - [8:11]: payload size in bytes (not including abi header bytes)
#    - [12:15]: abi 3.1.0
#    - [16:31]: reserved 0s
# - (optional) 12+ bytes codec_param: codec TLV parameters container, for more details please refer
#                                     struct codec_param under audio/codec_adapter/codec/generic.h
#    - [0:1]: param ID
#    - [2:3]: codec ID (when supporting multiple codecs, 0 otherwise)
#    - [4:7]: size in bytes (ID + size + data)
#    - [8:n-1]: data[], the param data
ifdef(`CA_SETUP_CONTROLBYTES',`', `define(`CA_SETUP_CONTROLBYTES',
``	bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00"''
)')
ifdef(`CA_SETUP_CONTROLBYTES_MAX',`', `define(`CA_SETUP_CONTROLBYTES_MAX', 176)')

# Codec Adapter runtime param control bytes (little endian)
#  : bytes "abi_header, [codec_param_0, codec_param_1...]"
#  - 32 bytes abi_header: you could get by command "sof-ctl -t 1 -g <payload_size> -b"
ifdef(`CA_RUNTIME_CONTROLBYTES',`', `define(`CA_RUNTIME_CONTROLBYTES',
``	bytes "0x53,0x4f,0x46,0x00,0x01,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00"''
)')
ifdef(`CA_RUNTIME_CONTROLBYTES_MAX',`', `define(`CA_RUNTIME_CONTROLBYTES_MAX', 157)')

# For codec developers, rename bytes control names if necessary.
ifdef(`CA_SETUP_CONTROLBYTES_NAME',`',
	`define(`CA_SETUP_CONTROLBYTES_NAME', `CA Setup Config')')
ifdef(`CA_RUNTIME_CONTROLBYTES_NAME',`',
	`define(`CA_RUNTIME_CONTROLBYTES_NAME', `CA Runtime Params')')

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

define(CA_RUNTIME_PARAMS, concat(`ca_runtime_params_', PIPELINE_ID))
define(CA_RUNTIME_CONTROLBYTES_NAME_PIPE, concat(CA_RUNTIME_CONTROLBYTES_NAME, PIPELINE_ID))

# Codec adapter runtime params
CONTROLBYTES_PRIV(CA_RUNTIME_PARAMS, CA_RUNTIME_CONTROLBYTES)

# Codec adapter Bytes control for runtime config
C_CONTROLBYTES(CA_RUNTIME_CONTROLBYTES_NAME_PIPE, PIPELINE_ID,
        CONTROLBYTES_OPS(bytes),
        CONTROLBYTES_EXTOPS(void, 258, 258),
        , , ,
        CONTROLBYTES_MAX(void, CA_RUNTIME_CONTROLBYTES_MAX),
        ,
        CA_RUNTIME_PARAMS)

#
# IIR EQ
#
define(DEF_EQIIR_COEF, concat(`eqiir_coef_', PIPELINE_ID))
define(DEF_EQIIR_PRIV, concat(`eqiir_priv_', PIPELINE_ID))

# define filter. eq_iir_coef_flat.m4 is set by default
ifdef(`PIPELINE_FILTER1', , `define(PIPELINE_FILTER1, eq_iir_coef_flat.m4)')
include(PIPELINE_FILTER1)

# EQ Bytes control with max value of 255
C_CONTROLBYTES(DEF_EQIIR_COEF, PIPELINE_ID,
    CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
    CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
    , , ,
    CONTROLBYTES_MAX(, 1024),
    ,
    DEF_EQIIR_PRIV)

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
        LIST(`          ', "CA_SETUP_CONTROLBYTES_NAME_PIPE", "CA_RUNTIME_CONTROLBYTES_NAME_PIPE"))

# "EQ 0" has 2 sink period and 2 source periods
W_EQ_IIR(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
    LIST(`      ', "DEF_EQIIR_COEF"))

# Playback Buffers
#W_BUFFER(0, COMP_BUFFER_SIZE(DAI_PERIODS,
#    COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
#    PLATFORM_HOST_MEM_CAP, SCHEDULE_CORE)
#W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
#    COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
#    PLATFORM_DAI_MEM_CAP, SCHEDULE_CORE)

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
    COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
    COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_HOST_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
    COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
    PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> EQ 0 --> B1 --> CODEC_ADAPTER -> B2 --> sink DAI0

P_GRAPH(pipe-pass-playback-PIPELINE_ID, PIPELINE_ID,
    LIST(`      ',
    `dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
    `dapm(N_EQ_IIR(0), N_BUFFER(0))',
    `dapm(N_BUFFER(1), N_EQ_IIR(0))',
    `dapm(N_CODEC_ADAPTER(0), N_BUFFER(1))',
    `dapm(N_BUFFER(2), N_CODEC_ADAPTER(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Passthrough Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`CA_RUNTIME_CONTROLBYTES_NAME_PIPE')
undefine(`CA_RUNTIME_PARAMS')
undefine(`CA_SETUP_CONTROLBYTES_NAME_PIPE')
undefine(`CA_SETUP_PARAMS')

undefine(`CA_SCHEDULE_CORE')
undefine(`CA_RUNTIME_CONTROLBYTES_NAME')
undefine(`CA_SETUP_CONTROLBYTES_NAME')
undefine(`CA_RUNTIME_CONTROLBYTES_MAX')
undefine(`CA_RUNTIME_CONTROLBYTES')
undefine(`CA_SETUP_CONTROLBYTES_MAX')
undefine(`CA_SETUP_CONTROLBYTES')

undefine(`DEF_EQIIR_COEF')
undefine(`DEF_EQIIR_PRIV')
