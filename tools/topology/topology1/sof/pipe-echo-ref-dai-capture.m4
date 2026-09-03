#
# The pipeline for echo reference feature, it is used
# for the capture DAI to dock.
#
# No connections in this pipeline.
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# DAI definitions
#
W_DAI_IN(DAI_TYPE, DAI_INDEX, DAI_BE, DAI_FORMAT, DAI_PERIODS, 0, SCHEDULE_CORE)

#
# DAI pipeline - always use 0 for DAIs
#
W_PIPELINE(N_DAI_IN, SCHEDULE_PERIOD, SCHEDULE_PRIORITY, SCHEDULE_CORE, SCHEDULE_TIME_DOMAIN, pipe_dai_schedule_plat)

indir(`define', concat(`PIPELINE_SCHED_COMP_', PIPELINE_ID), N_DAI_IN)
