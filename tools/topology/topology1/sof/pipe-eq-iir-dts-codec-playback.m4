# Low Latency Passthrough with DTS codec Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
# host PCM_P --> B0 --> EQ 0 --> B1 --> DTS codec --> B2 --> sink DAI0

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

#
# DTS Codec
#

include(`dts_codec_adapter.m4')

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

ifdef(`CA_SCHEDULE_CORE',`', `define(`CA_SCHEDULE_CORE', `SCHEDULE_CORE')')

# Host "Playback with codec adapter" PCM
# with DAI_PERIODS sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Passthrough Playback, DAI_PERIODS, 0, SCHEDULE_CORE)

W_CODEC_ADAPTER(0, PIPELINE_FORMAT, DAI_PERIODS, DAI_PERIODS, CA_SCHEDULE_CORE,
        LIST(`          ', "CA_SETUP_CONTROLBYTES_NAME_PIPE"))

# "EQ 0" has 2 sink period and 2 source periods
W_EQ_IIR(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
    LIST(`      ', "DEF_EQIIR_COEF"))

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

undefine(`CA_SETUP_CONTROLBYTES_NAME_PIPE')
undefine(`CA_SETUP_PARAMS')

undefine(`CA_SCHEDULE_CORE')
undefine(`CA_SETUP_CONTROLBYTES_NAME')
undefine(`CA_SETUP_CONTROLBYTES_MAX')
undefine(`CA_SETUP_CONTROLBYTES')

undefine(`DEF_EQIIR_COEF')
undefine(`DEF_EQIIR_PRIV')
