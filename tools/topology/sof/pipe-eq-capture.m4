# Capture EQ Pipeline and PCM, 48 kHz
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- Volume 0 <-- B1 <--EQ_IIR 0 <-- B2 <-- sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`mixercontrol.m4')
include(`eq_iir.m4')

#
# Controls
#

# Volume Mixer control with max value of 32
C_CONTROLMIXER(Master Capture Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw,
		256 binds the mixer control to volume get/put handlers,
		256, 256),
	CONTROLMIXER_MAX(, 70),
	false,
	CONTROLMIXER_TLV(TLV 80 steps from -50dB to +20dB for 1dB, vtlv_m50s1),
	Channel register and shift for Front Left/Right,
	LIST(`	', KCONTROL_CHANNEL(FL, 1, 0), KCONTROL_CHANNEL(FR, 1, 1)))

# Volume Configuration
W_VENDORTUPLES(capture_pga_tokens, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"250"'))

W_DATA(capture_pga_conf, capture_pga_tokens)

# Use 50 Hz highpass response with +20 dB gain
include(`eq_iir_coef_highpass_50hz_20db_48khz.m4')

# EQ Bytes control with max value of 255
C_CONTROLBYTES(EQIIR_C48, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	CONTROLBYTES_EXTOPS(
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,
	EQIIR_HP50HZ20dB48K_priv)

#
# Components and Buffers
#

# Host "Highpass Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, Highpass Capture, 0, 2)

# "Volume" has 2 source and 2 sink periods
W_PGA(0, PIPELINE_FORMAT, 2, 2,
	 capture_pga_conf, LIST(`		',
		"PIPELINE_ID Master Capture Volume"))

# "EQ 0" has 2 sink period and 2 source periods
W_EQ_IIR(0, PIPELINE_FORMAT, 2, 2, LIST(`		', "EQIIR_C48"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-- B0 <-- Volume 0 <-- B1 <--EQ_IIR 0 <-- B2 <-- sink DAI0

P_GRAPH(pipe-pass-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), N_PGA(0))',
	`dapm(N_PGA(0), N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_EQ_IIR(0))',
	`dapm(N_EQ_IIR(0), N_BUFFER(2))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Highpass Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(Highpass Capture PCM_ID, `S32_LE,S24_LE,S16_LE', PCM_MIN_RATE,
	PCM_MAX_RATE, PIPELINE_CHANNELS, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)
