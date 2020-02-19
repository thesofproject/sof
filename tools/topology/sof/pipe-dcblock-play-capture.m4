# Low Latency Passthrough with volume, DC Block Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_P --> B0 --> Volume 0 --> B1 --> host PCM_C

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`dai.m4')
include(`mixercontrol.m4')
include(`bytecontrol.m4')
include(`pipeline.m4')
include(`dcblock.m4')

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
	LIST(`	', KCONTROL_CHANNEL(FL, 1, 0), KCONTROL_CHANNEL(FR, 1, 1)))

#
# Volume configuration
#
W_VENDORTUPLES(playback_pga_tokens, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"250"'))

W_DATA(playback_pga_conf, playback_pga_tokens)

#
# Components and Buffers
#

# Host "Passthrough Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Passthrough Playback, 2, 0)

# Host "Passthrough Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Passthrough Capture, 0, 2)


# "Volume" has 2 source and x sink periods
W_PGA(0, PIPELINE_FORMAT, DAI_PERIODS, 2, playback_pga_conf, LIST(`		', "PIPELINE_ID Master Playback Volume"))

# "DC Block" has 2 sink periods and 2 source periods
W_DCBLOCK(0, PIPELINE_FORMAT, 2, 2,)

# Playback Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --> B0 --> DCBlock--> B1 --> Volume 0 --> B2 --> host PCM_C

P_GRAPH(pipe-pass-vol-play-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_DCBLOCK(0), N_BUFFER(0))',
 	`dapm(N_BUFFER(1), N_DCBLOCK(0))',
 	`dapm(N_PGA(0), N_BUFFER(1))',
 	`dapm(N_BUFFER(2), N_PGA(0))',
  `dapm(N_PCMC(PCM_ID), N_BUFFER(2))'))
#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_PCM_SINK_', PIPELINE_ID), Passthrough Capture PCM_ID)
indir(`define', concat(`PIPELINE_PCM_SOURCE_', PIPELINE_ID), Passthrough Playback PCM_ID)


#
# PCM Configuration

#
PCM_CAPABILITIES(Passthrough PlayCapture PCM_ID, `S32_LE,S24_LE,S16_LE', PCM_MIN_RATE, PCM_MAX_RATE, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)
