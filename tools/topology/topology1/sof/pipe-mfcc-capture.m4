# Capture EQ Pipeline and PCM, 48 kHz
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- MFCC 0 <-- B1 <-- sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`mfcc.m4')

#
# Controls
#

define(DEF_MFCC_CONFIG, concat(`mfcc_config_', PIPELINE_ID))
define(DEF_MFCC_PRIV, concat(`mfcc_priv_', PIPELINE_ID))

# By default, use 40 Hz highpass response with 0 dB gain
# TODO: need to implement middle level macro handler per pipeline
ifdef(`PIPELINE_FILTER1', , `define(PIPELINE_FILTER1, `mfcc/mfcc_config.m4')')
include(PIPELINE_FILTER1)

# EQ Bytes control with max value of 255
C_CONTROLBYTES(DEF_MFCC_CONFIG, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	CONTROLBYTES_EXTOPS(
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(, 1024),
	,
	DEF_MFCC_PRIV)

#
# Components and Buffers
#

# Host "MFCC Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, MFCC Capture, 0, 2, SCHEDULE_CORE)

# "EQ 0" has 2 sink period and x source periods
W_MFCC(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE,
	LIST(`		', "DEF_MFCC_CONFIG"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-- B0 <--MFCC 0 <-- B1 <-- sink DAI0

P_GRAPH(pipe-eq-iir-capture, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_MFCC(0))',
	`dapm(N_MFCC(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), MFCC Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(MFCC Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE,
	PCM_MAX_RATE, PIPELINE_CHANNELS, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_MFCC_CONFIG')
undefine(`DEF_MFCC_PRIV')
