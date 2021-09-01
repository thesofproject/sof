# Low Latency Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> ASRC --> B1 --> sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`asrc.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# Controls
#

#
# Components and Buffers
#

# Host "ASRC Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, ASRC Playback, 2, 0, SCHEDULE_CORE)

#
# ASRC Configuration
#

define(MY_ASRC_TOKENS, concat(`asrc_tokens_', PIPELINE_ID))
define(MY_ASRC_CONF, concat(`asrc_conf_', PIPELINE_ID))
W_VENDORTUPLES(MY_ASRC_TOKENS, sof_asrc_tokens,
LIST(`		', `SOF_TKN_ASRC_RATE_OUT "PIPELINE_RATE"'
     `		', `SOF_TKN_ASRC_ASYNCHRONOUS_MODE "1"'
     `		', `SOF_TKN_ASRC_OPERATION_MODE "0"'))

W_DATA(MY_ASRC_CONF, MY_ASRC_TOKENS)

# "ASRC" has x sink and 2 source periods
W_ASRC(0, PIPELINE_FORMAT, DAI_PERIODS, 2, MY_ASRC_CONF)

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
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
#  host PCM_P --> B0 --> ASRC --> B1 --> sink DAI0

P_GRAPH(pipe-asrc-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_ASRC(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_ASRC(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), ASRC Playback PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(ASRC Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE,
	PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`MY_ASRC_TOKENS')
undefine(`MY_ASRC_CONF')
