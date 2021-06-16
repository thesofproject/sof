# Capture pipeline with SRC and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- SRC <-- B1 <-- source DAI0

# Include topology builder
include(`pipeline.m4')
include(`buffer.m4')
include(`utils.m4')
include(`src.m4')
include(`pcm.m4')
include(`dai.m4')

#
# Components and Buffers
#

# Host "SRC Capture" PCM
# with 0 sink and 3 source periods
W_PCM_CAPTURE(PCM_ID, SRC Capture, 0, 3, SCHEDULE_CORE)

#
# SRC Configuration
#

# Create unique names for data to allow several different input rate instances
define(MY_SRC_TOKENS, concat(`src_tokens_', PIPELINE_ID))
define(MY_SRC_CONF, concat(`src_conf_', PIPELINE_ID))
W_VENDORTUPLES(MY_SRC_TOKENS, sof_src_tokens,
	LIST(`		', `SOF_TKN_SRC_RATE_IN	"PIPELINE_RATE"'))

W_DATA(MY_SRC_CONF, MY_SRC_TOKENS)

# "SRC" has x source and 3 sink periods
W_SRC(0, PIPELINE_FORMAT, DAI_PERIODS, 3, MY_SRC_CONF)

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT),
	PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-- B0 <-- SRC <-- B1 <-- source DAI0

P_GRAPH(pipe-src-capture, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_SRC(0))',
	`dapm(N_SRC(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), SRC Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(SRC Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`MY_SRC_TOKENS')
undefine(`MY_SRC_CONF')
