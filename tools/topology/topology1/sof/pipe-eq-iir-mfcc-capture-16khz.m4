# Capture EQ MFCC Pipeline and PCM, 16 kHz
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- MFCC 0 <-- B1 <--EQ_IIR 0 <-- B2 <-- sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`eq_iir.m4')
include(`mfcc.m4')

#
# Controls
#

define(DEF_MFCC_CONFIG, concat(`mfcc_config_', PIPELINE_ID))
define(DEF_MFCC_PRIV, concat(`mfcc_priv_', PIPELINE_ID))

# define filter. mfcc_config.m4 is set by default
ifdef(`DMIC16KPROC_FILTER2', , `define(DMIC16KPROC_FILTER2, `mfcc/mfcc_config.m4')')
include(DMIC16KPROC_FILTER2)

# MFCC Bytes control with max value of 255
define(DEF_MFCC_PARAM, concat(`mfcc_param_', PIPELINE_ID))
C_CONTROLBYTES(DEF_MFCC_PARAM, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(, 1024),
	,
	DEF_MFCC_PRIV)

# By default, use 40 Hz highpass response with +0 dB gain for 16khz
# TODO: need to implement middle level macro handler per pipeline
ifdef(`DMIC16KPROC_FILTER1', , `define(DMIC16KPROC_FILTER1, eq_iir_coef_highpass_40hz_0db_16khz.m4)')
define(DEF_EQIIR_PRIV, DMIC16KPROC_FILTER1)
include(DMIC16KPROC_FILTER1)
define(DEF_EQIIR_COEF, concat(`eqiir_coef_', PIPELINE_ID))

# EQ Bytes control with max value of 255
C_CONTROLBYTES(DEF_EQIIR_COEF, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	CONTROLBYTES_EXTOPS(
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(, 1024),
	,
	DEF_EQIIR_PRIV)

#
# Components and Buffers
#

# Host "Highpass Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Highpass Capture, 0, 2, SCHEDULE_CORE)

# "MFCC 0" has 2 source period and 2 sink periods
W_MFCC(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`		', "DEF_MFCC_PARAM"))

# "EQ 0" has 2 sink period and x source periods
W_EQ_IIR(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE,
	LIST(`		', "DEF_EQIIR_COEF"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-- B0 <-- MFCC 0 <-- B1 <--EQ_IIR 0 <-- B2 <-- sink DAI0

P_GRAPH(pipe-eq-iir-mfcc-capture-16khz, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_MFCC(0))',
	`dapm(N_MFCC(0), N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_EQ_IIR(0))',
	`dapm(N_EQ_IIR(0), N_BUFFER(2))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Highpass Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Highpass Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), 16000, 16000,
	PIPELINE_CHANNELS, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_EQIIR_COEF')
undefine(`DEF_EQIIR_PRIV')
undefine(`DEF_MFCC_PARAM')
undefine(`DEF_MFCC_CONFIG')
undefine(`DEF_MFCC_PRIV')
