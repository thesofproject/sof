# Mixer DAI Playback connector
#
#  DAI playback starting with a LL mixer
#
# Pipeline Endpoints for connection are :-
#
#	LL Playback Mixer (Mixer)
#	LL Playback Volume B3 (DAI buffer)
#
# DAI_BUF -> ll mixer(M) --B0--> volume(LL) ---B1-->  sink DAI
#
# the ll mixer is connected to one DAI_BUF by default. Additional ones can be added later

# Include topology builder
include(`utils.m4')
include(`mixer.m4')
include(`mixercontrol.m4')
include(`pga.m4')
include(`buffer.m4')
include(`dai.m4')
include(`pipeline.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Playback Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -64dB to 0dB for 2dB, vtlv_m64s2),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

#
# Volume configuration
#

W_VENDORTUPLES(playback_pga_tokens, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"250"'))

W_DATA(playback_pga_conf, playback_pga_tokens)

# Mixer 0 has 2 sink and source periods.
W_MIXER(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE)

# "Master Playback Volume" has 2 source and x sink periods for DAI ping-pong
W_PGA(0, PIPELINE_FORMAT, DAI_PERIODS, 2, playback_pga_conf, SCHEDULE_CORE,
	LIST(`		', "PIPELINE_ID Master Playback Volume"))

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
	`dapm(N_PGA(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_PGA(0))'
	`dapm(N_DAI_OUT, N_BUFFER(1))'))

indir(`define', concat(`PIPELINE_PLAYBACK_SCHED_COMP_', PIPELINE_ID), N_DAI_OUT)
indir(`define', concat(`PIPELINE_MIXER_', PIPELINE_ID), N_MIXER(0))

