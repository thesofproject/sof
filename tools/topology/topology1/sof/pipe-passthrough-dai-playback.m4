# Passthrough DAI Playback connector
#
# DAI_BUF --> B0 -->  sink DAI
#
# the ll mixer is connected to one DAI_BUF by default. Additional ones can be added later

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`buffer.m4')

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

#
# Graph connections to pipelines
# we don't connect 	`dapm(N_MIXER(0), DAI_BUF)' due to forward dependencies
#
P_GRAPH(DAI_NAME, PIPELINE_ID,
	LIST(`		',
	`dapm(N_DAI_OUT, N_BUFFER(0))'))

indir(`define', concat(`PIPELINE_PLAYBACK_SCHED_COMP_', PIPELINE_ID), N_DAI_OUT)
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(0))

undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
