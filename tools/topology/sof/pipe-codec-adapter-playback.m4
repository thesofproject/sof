# Low Latency Passthrough Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> CODEC_ADAPTER -> B1 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`codec_adapter.m4')
include(`bytecontrol.m4')

#
# Controls
#

# For codec developers, please define the following control bytes of setup config and runtime params
# and the corresponding max bytenum on your own.

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
ifdef(`CA_SETUP_CONTROLBYTES',`', `define(`CA_SETUP_CONTROLBYTES',
``	bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`	0x14,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`	0x01,0x11,0x33,0xd0,0x00,0x00,0x00,0x00,'
`	0x80,0xbb,0x00,0x00,0x20,0x00,0x00,0x00,'
`	0x02,0x00,0x00,0x00"''
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
