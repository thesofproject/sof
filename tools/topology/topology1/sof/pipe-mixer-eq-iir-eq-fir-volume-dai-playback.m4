# Mixer DAI Playback connector
#
#  DAI playback starting with a LL mixer
#
# Pipeline Endpoints for connection are :-
#
#	LL Playback Mixer (Mixer)
#	LL Playback Volume B3 (DAI buffer)
#
# DAI_BUF --> ll mixer(M) --> B0 --> EQ_IIR 0 --> B1  --> EQ_FIR 0 --> B2 --> volume(LL) --> B3-->  sink DAI
#
# the ll mixer is connected to one DAI_BUF by default. Additional ones can be added later

# Include topology builder
include(`utils.m4')
include(`mixer.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')
include(`pga.m4')
include(`buffer.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`eq_iir.m4')
include(`eq_fir.m4')

# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Playback Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -64dB to 0dB for 2dB, vtlv_m64s2),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

#
# Volume configuration
#

define(DEF_PGA_TOKENS, concat(`pga_tokens_', PIPELINE_ID))
define(DEF_PGA_CONF, concat(`pga_conf_', PIPELINE_ID))

W_VENDORTUPLES(DEF_PGA_TOKENS, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"20"'))

W_DATA(DEF_PGA_CONF, DEF_PGA_TOKENS)

# Mixer 0 has 2 sink and source periods.
W_MIXER(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE)

#
# IIR EQ
#

define(DEF_EQIIR_COEF, concat(`eqiir_coef_', PIPELINE_ID))
define(DEF_EQIIR_PRIV, concat(`eqiir_priv_', PIPELINE_ID))

# By default, use coefficients for pass frequency response
ifdef(`PIPELINE_FILTER1', , `define(PIPELINE_FILTER1, eq_iir_coef_pass.m4)')
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
# FIR EQ
#

define(DEF_EQFIR_COEF, concat(`eqfir_coef_', PIPELINE_ID))
define(DEF_EQFIR_PRIV, concat(`eqfir_priv_', PIPELINE_ID))

# By default, use coefficients for pass frequency response
ifdef(`PIPELINE_FILTER2', , `define(PIPELINE_FILTER2, eq_fir_coef_pass.m4)')
include(PIPELINE_FILTER2)

# EQ Bytes control with max value of 255
C_CONTROLBYTES(DEF_EQFIR_COEF, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 4096),
	,
	DEF_EQFIR_PRIV)

# "EQ 0" has 2 sink period and 2 source periods
W_EQ_IIR(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`		', "DEF_EQIIR_COEF"))

# "EQ 0" has 2 sink period and 2 source periods
W_EQ_FIR(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`		', "DEF_EQFIR_COEF"))

# "Master Playback Volume" has 2 source and x sink periods for DAI ping-pong
W_PGA(0, PIPELINE_FORMAT, DAI_PERIODS, 2, DEF_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Master Playback Volume"))

#
# DAI definitions
#
W_DAI_OUT(DAI_TYPE, DAI_INDEX, DAI_BE, DAI_FORMAT, 0, DAI_PERIODS, SCHEDULE_CORE)

#
# DAI pipeline - always use 0 for DAIs - FIXME WHY 0?
#
W_PIPELINE(N_DAI_OUT, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_dai_schedule_plat)

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), DAI_CHANNELS, COMP_PERIOD_FRAMES(DAI_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), DAI_CHANNELS,COMP_PERIOD_FRAMES(DAI_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), DAI_CHANNELS,COMP_PERIOD_FRAMES(DAI_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(3, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), DAI_CHANNELS,COMP_PERIOD_FRAMES(DAI_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Graph connections to pipelines
# we don't connect 	`dapm(N_MIXER(0), DAI_BUF)' due to forward dependencies
#
P_GRAPH(DAI_NAME, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_MIXER(0))',
	`dapm(N_EQ_IIR(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_EQ_IIR(0))',
	`dapm(N_EQ_FIR(0), N_BUFFER(1))',
	`dapm(N_BUFFER(2), N_EQ_FIR(0))',
	`dapm(N_PGA(0), N_BUFFER(2))',
	`dapm(N_BUFFER(3), N_PGA(0))'
	`dapm(N_DAI_OUT, N_BUFFER(3))'))

indir(`define', concat(`PIPELINE_PLAYBACK_SCHED_COMP_', PIPELINE_ID), N_DAI_OUT)
indir(`define', concat(`PIPELINE_MIXER_', PIPELINE_ID), N_MIXER(0))

undefine(`DEF_EQIIR_COEF')
undefine(`DEF_EQIIR_PRIV')
undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
