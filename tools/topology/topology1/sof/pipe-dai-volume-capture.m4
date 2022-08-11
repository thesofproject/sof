#  DAI capture volume connector
#
#  DAI capture ending with volume
#
# Pipeline Endpoints for connection are :-
#
#	LL Capture volume
#	LL Capture Volume (Volume buffer)
#
# Source DAI 0 --> B0 --> volume(LL) --> VOL_BUF
#

# Include topology builder
include(`utils.m4')
include(`mixer.m4')
include(`mixercontrol.m4')
include(`pga.m4')
include(`buffer.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Capture Volume, PIPELINE_ID,
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

# "Master Capture Volume" has 2 source and x sink periods for DAI ping-pong
W_PGA(0, PIPELINE_FORMAT, DAI_PERIODS, 2, DEF_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Master Capture Volume"))

#
# DAI definitions
#
W_DAI_IN(DAI_TYPE, DAI_INDEX, DAI_BE, DAI_FORMAT, 0, DAI_PERIODS, SCHEDULE_CORE)

#
# DAI pipeline - always use 0 for DAIs - FIXME WHY 0?
#
W_PIPELINE(N_DAI_IN, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_dai_schedule_plat)

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), DAI_CHANNELS, COMP_PERIOD_FRAMES(DAI_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Graph connections to pipelines
#
P_GRAPH(DAI_NAME, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_DAI_IN)',
	`dapm(N_PGA(0), N_BUFFER(0))'))

indir(`define', concat(`PIPELINE_CAPTURE_SCHED_COMP_', PIPELINE_ID), N_DAI_IN)
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_PGA(0))

undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
