# DAI Playback connector

# Include topology builder
include(`local.m4')

#
# DAI definitions
#
W_DAI_OUT(DAI_SNAME, DAI_TYPE, DAI_INDEX, DAI_FORMAT, 0, 2, 2, dai0p_plat_conf)

#
# DAI pipeline - always use 0 for DAIs
#
W_PIPELINE(N_DAI_OUT, SCHEDULE_DEADLINE, SCHEDULE_PRIORITY, SCHEDULE_FRAMES, SCHEDULE_CORE, pipe_dai_schedule_plat)

#
# Graph connections to pipelines

SectionGraph.STR(DAI_NAME) {
	index STR(PIPELINE_ID)

	lines [
		dapm(N_DAI_OUT, DAI_BUF)
	]
}
