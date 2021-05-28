# Capture EQ Pipeline and PCM, 48 kHz
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <--EQ_FIR 0 <-- B1 <-- sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`eq_fir.m4')

#
# Controls
#

define(DEF_EQFIR_COEF, concat(`eqfir_coef_', PIPELINE_ID))
define(DEF_EQFIR_PRIV, concat(`eqfir_priv_', PIPELINE_ID))

# define filter. eq_fir_coef_pass.m4 is set by default
ifdef(`PIPELINE_FILTER2', , `define(PIPELINE_FILTER2, eq_fir_coef_pass.m4)')
include(PIPELINE_FILTER2)

# EQ Bytes control with max value of 255
define(MY_CONTROLBYTES, concat(`EQFIR_CONTROLBYTES_', PIPELINE_ID))
C_CONTROLBYTES(MY_CONTROLBYTES, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	CONTROLBYTES_EXTOPS(
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(, 4096),
	,
	DEF_EQFIR_PRIV)

#
# Components and Buffers
#

# Host "EQ FIR Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, EQ FIR Capture, 0, 2, SCHEDULE_CORE)

# "EQ 0" has 2 sink period and x source periods
W_EQ_FIR(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE,
	LIST(`		', "MY_CONTROLBYTES"))

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
#  host PCM_C <-- B0 <--EQ_FIR 0 <-- B1 <-- sink DAI0

P_GRAPH(pipe-eq-fir-capture, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_EQ_FIR(0))',
	`dapm(N_EQ_FIR(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), EQ FIR Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(EQ FIR Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE,
	PCM_MAX_RATE, PIPELINE_CHANNELS, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`MY_CONTROLBYTES')
undefine(`MY_EQFIR_COEF')
undefine(`MY_EQFIR_PRIV')
