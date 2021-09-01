# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- ASRC <-- sink DAI

# Include topology builder
include(`utils.m4')
include(`asrc.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# Components and Buffers
#

# Host "ASRC Capture" PCM
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

# "ASRC" has x 3 sink and x source periods
W_ASRC(0, PIPELINE_FORMAT, 3, DAI_PERIODS, MY_ASRC_CONF)

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(3,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP, SCHEDULE_CORE)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP, SCHEDULE_CORE)

#
# Pipeline Graph
#
#  host PCM_P <-- B0 <-- ASRC <-- B1 <-- sink DAI

P_GRAPH(pipe-asrc-capture, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_ASRC(0))',
	`dapm(N_ASRC(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), ASRC Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(ASRC Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT),
	PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192,
	16384, 65536, 65536)

undefine(`MY_ASRC_TOKENS')
undefine(`MY_ASRC_CONF')
