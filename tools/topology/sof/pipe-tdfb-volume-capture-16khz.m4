# Capture EQ Pipeline and PCM, 48 kHz
#
# Pipeline Endpoints for connection are :-
#
#  host PCM_C <-- B0 <-- Volume 0 <-- B1 <-- TDFB 0 <-- B2 <-- sink DAI0

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`pga.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`bytecontrol.m4')
include(`mixercontrol.m4')
include(`tdfb.m4')

ifdef(`PGA_NAME', `', `define(PGA_NAME, N_PGA(0))')
define(`CONTROL_NAME_VOLUME', 2nd Capture Volume)
define(`CONTROL_NAME_SWITCH', 2nd Capture Switch)
define(`CONTROL_NAME', `CONTROL_NAME_VOLUME')

#
# Controls
#

define(DEF_TDFB_PRIV, concat(`tdfb_priv_', PIPELINE_ID))

# Define filter. A passthrough is set by default.
ifdef(`PIPELINE_FILTER1', , `define(PIPELINE_FILTER1, `tdfb/coef_line2_pass.m4')')
include(PIPELINE_FILTER1)

# TDFB Bytes control with max value of 255
define(DEF_TDFB_BYTES, concat(`tdfb_bytes_', PIPELINE_ID))
C_CONTROLBYTES(DEF_TDFB_BYTES, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(, 4096),
	,
	DEF_TDFB_PRIV)

# Volume Mixer control with max value of 32
C_CONTROLMIXER(Capture Volume, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw,
		256 binds the mixer control to volume get/put handlers,
		256, 256),
	CONTROLMIXER_MAX(, 70),
	false,
	CONTROLMIXER_TLV(TLV 80 steps from -50dB to +20dB for 1dB, vtlv_m50s1),
	Channel register and shift for Front Left/Right,
	VOLUME_CHANNEL_MAP)

undefine(`CONTROL_NAME')
define(`CONTROL_NAME', `CONTROL_NAME_SWITCH')

# Switch type Mixer Control with max value of 1
C_CONTROLMIXER(Capture Switch, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 259 binds the mixer control to switch get/put handlers, 259, 259),
	CONTROLMIXER_MAX(max 1 indicates switch type control, 1),
	false,
	,
	Channel register and shift for Front Left/Right,
	SWITCH_CHANNEL_MAP,
	"1", "1")

# Volume Configuration
define(DEF_PGA_TOKENS, concat(`pga_tokens_', PIPELINE_ID))
define(DEF_PGA_CONF, concat(`pga_conf_', PIPELINE_ID))

W_VENDORTUPLES(DEF_PGA_TOKENS, sof_volume_tokens,
LIST(`		', `SOF_TKN_VOLUME_RAMP_STEP_TYPE	"0"'
     `		', `SOF_TKN_VOLUME_RAMP_STEP_MS		"250"'))

W_DATA(DEF_PGA_CONF, DEF_PGA_TOKENS)

#
# Components and Buffers
#

# Host "TDFB Capture" PCM
# with 0 sink and 2 source periods
W_PCM_CAPTURE(PCM_ID, TDFB Capture, 0, 2, SCHEDULE_CORE)

# "Volume" has 2 source and 2 sink periods
W_PGA(0, PIPELINE_FORMAT, 2, 2, DEF_PGA_CONF, SCHEDULE_CORE,
	LIST(`		', "CONTROL_NAME_VOLUME",
	"CONTROL_NAME_SWITCH"))

# "TDFB 0" has 2 sink period and x source periods
W_TDFB(0, PIPELINE_FORMAT, 2, DAI_PERIODS, SCHEDULE_CORE,
	LIST(`		', "DEF_TDFB_BYTES"))

# Capture Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

W_BUFFER(2, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS,
	COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)), PLATFORM_PASS_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_C <-- B0 <-- Volume 0 <-- B1 <-- TDFB 0 <-- B2 <-- sink DAI0

P_GRAPH(pipe-pass-capture-PIPELINE_ID, PIPELINE_ID,
	LIST(`		',
	`dapm(N_PCMC(PCM_ID), N_BUFFER(0))',
	`dapm(N_BUFFER(0), PGA_NAME)',
	`dapm(PGA_NAME, N_BUFFER(1))',
	`dapm(N_BUFFER(1), N_TDFB(0))',
	`dapm(N_TDFB(0), N_BUFFER(2))'))

undefine(`PGA_NAME')
undefine(`CONTROL_NAME')
undefine(`CONTROL_NAME_VOLUME')
undefine(`CONTROL_NAME_SWITCH')

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SINK_', PIPELINE_ID), N_BUFFER(2))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), TDFB Capture PCM_ID)

#
# PCM Configuration
#

PCM_CAPABILITIES(TDFB Capture PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), PCM_MIN_RATE,
	PCM_MAX_RATE, PIPELINE_CHANNELS, PIPELINE_CHANNELS,
	2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_PGA_TOKENS')
undefine(`DEF_PGA_CONF')
undefine(`DEF_TDFB_PRIV')
undefine(`DEF_TDFB_BYTES')
