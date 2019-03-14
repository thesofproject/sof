# DAI Playback connector

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# DAI definitions
#
W_DAI_OUT(DAI_TYPE, DAI_INDEX, DAI_BE, DAI_FORMAT, 0, 2, 2)

#
# DAI pipeline - always use 0 for DAIs
#
W_PIPELINE(N_DAI_OUT, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_dai_schedule_plat)

#
# Graph connections to pipelines

P_GRAPH(DAI_NAME, PIPELINE_ID,
	LIST(`		', `dapm(N_DAI_OUT, DAI_BUF)'))
