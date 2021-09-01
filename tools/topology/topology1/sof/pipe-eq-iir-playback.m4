# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> EQ_IIR 0 --> B1 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')
include(`eq_iir.m4')

#
# Controls
#

define(DEF_EQIIR_COEF, concat(`eqiir_coef_', PIPELINE_ID))
define(DEF_EQIIR_PRIV, concat(`eqiir_priv_', PIPELINE_ID))

# define filter. eq_iir_coef_flat.m4 is set by default
ifdef(`PIPELINE_FILTER1', , `define(PIPELINE_FILTER1, eq_iir_coef_flat.m4)')
include(PIPELINE_FILTER1)

# EQ Bytes control with max value of 255
define(MY_CONTROLBYTES, concat(`EQIIR_CONTROLBYTES_', PIPELINE_ID))
C_CONTROLBYTES(MY_CONTROLBYTES, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(, 1024),
	,
	DEF_EQIIR_PRIV)

#
# Components and Buffers
#

# Host "EQ IIR Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, EQ IIR Playback, 2, 0, SCHEDULE_CORE)

# "EQ 0" has x sink period and 2 source periods
W_EQ_IIR(0, PIPELINE_FORMAT, DAI_PERIODS, 2, SCHEDULE_CORE,
	LIST(`		', "MY_CONTROLBYTES"))

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP, SCHEDULE_CORE)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP, SCHEDULE_CORE)

#
# Pipeline Graph
#
#  host host PCM_P --> B0 --> EQ_IIR 0 --> B1 --> sink DAI0

P_GRAPH(pipe-eq-iir-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_EQ_IIR(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_EQ_IIR(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID),
	EQ IIR Playback PCM_ID)

#
# PCM Configuration

#
PCM_CAPABILITIES(EQ IIR Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`MY_CONTROLBYTES')
undefine(`DEF_EQIIR_COEF')
undefine(`DEF_EQIIR_PRIV')
