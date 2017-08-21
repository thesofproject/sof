# DAI Playback connector

# Include topology builder
include(`local.m4')

#
# DAI definitions
#
W_DAI_IN(DAI_SNAME, DAI_TYPE, DAI_INDEX, DAI_FORMAT, 2, 0, 0, dai0c_plat_conf)

#
# DAI pipeline - always use 0 for DAIs
#
W_PIPELINE(N_DAI_IN, SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, pipe_dai_schedule_plat)

#
# Graph connections to pipelines

SectionGraph.STR(DAI_NAME) {
	index STR(PIPELINE_ID)

	lines [
		dapm(DAI_BUF, N_DAI_IN)
	]
}
