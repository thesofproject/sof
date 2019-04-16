# Low Latency Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <--B5-- volume(0C) <--B4-- source DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`mixercontrol.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(PCM PCM_ID Capture Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 40),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -64dB to 0dB for 2dB, vtlv_m64s2),
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 0, 0), KCONTROL_CHANNEL(FR, 0, 1)))

#
# Components and Buffers
#

# Host "Low Latency Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Low Latency Capture, 0, 2)

# "Capture Volume" has 2 sink and source periods for host and DAI ping-pong
W_PGA(0, PIPELINE_FORMAT, 2, 2, LIST(`		', "PIPELINE_ID PCM PCM_ID Capture Volume"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_DAI_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_HOST_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM <--B1-- volume <--B0-- source DAI0

P_GRAPH(pipe-ll-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_PGA(0))',
	`dapm(N_PGA(0), N_BUFFER(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(0))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Low Latency Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Low Latency Capture PCM_ID, `S32_LE,S24_LE,S16_LE', 48000, 48000, 2, PIPELINE_CHANNELS, 2, 4, 192, 16384, 65536, 65536)
