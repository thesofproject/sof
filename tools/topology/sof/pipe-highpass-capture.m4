# IIR highpass filter, volume control, and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- Volume <-- B1 <-- EQ_IIR <-- B2 <-- source DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`dai.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`eq_iir.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Capture Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw,
		256 binds the mixer control to volume get/put handlers,
		256, 256),
	CONTROLMIXER_MAX(, 80),
	false,
	CONTROLMIXER_TLV(TLV 80 steps from -50dB to +30dB for 1dB, vtlv_m50s1),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

#
# Volume Configuration
#

define(DEF_PGA_TOKENS, concat(`pga_tokens_', PIPELINE_ID))
define(DEF_PGA_CONF, concat(`pga_conf_', PIPELINE_ID))
W_VENDORTUPLES(DEF_PGA_TOKENS, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"400"'))

W_DATA(DEF_PGA_CONF, DEF_PGA_TOKENS)

#
# IIR configuration
#

define(DEF_EQIIR_COEF, concat(`eqiir_coef_', PIPELINE_ID))
define(DEF_EQIIR_PRIV, concat(`eqiir_priv_', PIPELINE_ID))

# By default, use 40 Hz highpass response with 0 dB gain
ifdef(`PIPELINE_FILTER1', , `define(PIPELINE_FILTER1, eq_iir_coef_highpass_40hz_0db_48khz.m4)')
include(PIPELINE_FILTER1)

# EQ Bytes control with max value of 255
define(DEF_EQIIR_COEF, concat(`eqiir_bytes_', PIPELINE_ID))
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

# "Volume" has 2 sink and 2 source periods
W_PGA(0, PIPELINE_FORMAT, 2, DAI_PERIODS, DEF_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Master Capture Volume"))

# "EQ_IIR 0" has 2 sink and x source periods
W_EQ_IIR(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE,
	LIST(`		', "DEF_EQIIR_COEF"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P <-- B0 <-- Volume 0 <-- B1 <-- EQ_IIR 0 <-- B2 <-- sink DAI0

P_GRAPH(pipe-pass-vol-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_PGA(0))',
	`dapm(N_PGA(0), N_BUFFER(1))',
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

PCM_CAPABILITIES(Highpass Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, PIPELINE_CHANNELS, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
undefine(`DEF_EQIIR_COEF')
undefine(`DEF_EQIIR_PRIV')
