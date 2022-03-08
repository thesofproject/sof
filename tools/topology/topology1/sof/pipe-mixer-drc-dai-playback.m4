# Mixer DAI Playback connector
#
#  DAI playback starting with a LL mixer
#
# Pipeline Endpoints for connection are :-
#
#	LL Playback Mixer (Mixer)
#	LL Playback Volume B1 (DAI buffer)
#
# DAI_BUF --> ll mixer(M) --> B0 --> DRC(LL) --> B1 -->  sink DAI
#
# the ll mixer is connected to one DAI_BUF by default. Additional ones can be added later

# Include topology builder
include(`utils.m4')
include(`mixer.m4')
include(`buffer.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`drc.m4')

#
# Controls
#

#
# DRC Configuration
#

define(DRC_priv, concat(`drc_bytes_', PIPELINE_ID))
define(MY_DRC_CTRL, concat(`drc_control_', PIPELINE_ID))
include(`drc_coef_default.m4')
C_CONTROLBYTES(MY_DRC_CTRL, PIPELINE_ID,
      CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
      CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
      , , ,
      CONTROLBYTES_MAX(, 1024),
      ,
      DRC_priv)


# Mixer 0 has 2 sink and source periods.
W_MIXER(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE)

# "DRC" has 2 sink periods and 2 source periods
W_DRC(0, PIPELINE_FORMAT, DAI_PERIODS, 2, SCHEDULE_CORE,
	LIST(`   ', "MY_DRC_CTRL"))

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
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), DAI_CHANNELS,COMP_PERIOD_FRAMES(DAI_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Graph connections to pipelines
# we don't connect 	`dapm(N_MIXER(0), DAI_BUF)' due to forward dependencies
#
P_GRAPH(DAI_NAME, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_MIXER(0))',
	`dapm(N_DRC(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_DRC(0))'
	`dapm(N_DAI_OUT, N_BUFFER(1))'))

indir(`define', concat(`PIPELINE_PLAYBACK_SCHED_COMP_', PIPELINE_ID), N_DAI_OUT)
indir(`define', concat(`PIPELINE_MIXER_', PIPELINE_ID), N_MIXER(0))

undefine(`MY_DRC_CTRL')
undefine(`DRC_priv')
