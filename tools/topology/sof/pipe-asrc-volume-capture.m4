# Capture pipeline with ASRC and volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- ASRC <-- B1 <-- PGA <-- B2 <-- sink DAI0

# Include topology builder
include(`mixercontrol.m4')
include(`pipeline.m4')
include(`buffer.m4')
include(`utils.m4')
include(`asrc.m4')
include(`dai.m4')
include(`pcm.m4')
include(`pga.m4')

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

# Volume Configuration
define(MY_PGA_TOKENS, concat(`pga_tokens_', PIPELINE_ID))
define(MY_PGA_CONF, concat(`pga_conf_', PIPELINE_ID))

W_VENDORTUPLES(MY_PGA_TOKENS, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"250"'))

W_DATA(MY_PGA_CONF, MY_PGA_TOKENS)

#
# Components and Buffers
#

# Host "Passthrough Capture" PCM
# with 0 sink and 3 source periods
W_PCM_CAPTURE(PCM_ID, ASRC Capture, 0, 3, SCHEDULE_CORE)

#
# ASRC Configuration
#

define(MY_ASRC_TOKENS, concat(`asrc_tokens_', PIPELINE_ID))
define(MY_ASRC_CONF, concat(`asrc_conf_', PIPELINE_ID))
W_VENDORTUPLES(MY_ASRC_TOKENS, sof_asrc_tokens,
LIST(`		', `SOF_TKN_ASRC_RATE_IN "PIPELINE_RATE"'
     `		', `SOF_TKN_ASRC_ASYNCHRONOUS_MODE "1"'
     `		', `SOF_TKN_ASRC_OPERATION_MODE "1"'))

W_DATA(MY_ASRC_CONF, MY_ASRC_TOKENS)

#
# Components
#

# "ASRC" has 3 sink and 3 source periods
W_ASRC(0, PIPELINE_FORMAT, 3, 3, MY_ASRC_CONF)

# "Volume" has 3 sink and x source periods
W_PGA(0, PIPELINE_FORMAT, 3, DAI_PERIODS, MY_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Master Capture Volume"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT),
	PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT),
	PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-- B0 <-- ASRC <-- B1 <-- PGA <-- B2 <-- sink DAI0

P_GRAPH(pipe-pass-src-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_ASRC(0))',
	`dapm(N_ASRC(0), N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_PGA(0))',
	`dapm(N_PGA(0), N_BUFFER(2))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID),
	ASRC Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(ASRC Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`MY_PGA_TOKENS')
undefine(`MY_PGA_CONF')
undefine(`MY_ASRC_TOKENS')
undefine(`MY_ASRC_CONF')
