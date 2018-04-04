# Passthrough with volume Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- Volume 0 <-- B1 <-- source DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`dai.m4')
include(`mixercontrol.m4')
include(`pipeline.m4')

#
# Controls
#
# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Capture Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 256 binds the mixer control to volume get/put handlers, 256, 256),
	CONTROLMIXER_MAX(, 32),
	false,
	CONTROLMIXER_TLV(TLV 32 steps from -90dB to +6dB for 3dB, vtlv_m90s3),
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 1, 0), KCONTROL_CHANNEL(FR, 1, 1)))

#
# Components and Buffers
#

# Host "Passthrough Capture" PCM uses pipeline DMAC and channel
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Passthrough Capture, PIPELINE_DMAC, PIPELINE_DMAC_CHAN, 0, 2, 2)

# "Volume" has 2 source and 2 sink periods
W_PGA(0, PIPELINE_FORMAT, 2, 2, 2, LIST(`		', "Master Capture Volume PIPELINE_ID"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(DAI_FORMAT), PIPELINE_CHANNELS, SCHEDULE_FRAMES),
	PLATFORM_DAI_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P <-- B0 <-- Volume 0 <-- B1 <-- sink DAI0

P_GRAPH(pipe-pass-vol-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(Passthrough Capture PCM_ID, N_PCMC(PCM_ID))',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_PGA(0))',
	`dapm(N_PGA(0), N_BUFFER(1))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(1))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Passthrough Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Passthrough Capture PCM_ID, `S32_LE,S24_LE,S16_LE', 48000, 48000, 2, 4, 2, 16, 192, 16384, 65536, 65536)

