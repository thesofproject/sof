# Demux EQ band split pipeline with 3-band DRC
#
# host PCM_P --B0--> demux(M) --B1--> eq_iir --B2--> multiband_drc --B3--> sink DAI0
#

# Include topology builder
include(`utils.m4')
include(`buffer.m4')
include(`pcm.m4')
include(`muxdemux.m4')
include(`bytecontrol.m4')
include(`multiband_drc.m4')
include(`eq_iir.m4')

# demux Bytes control with max value of 255
C_CONTROLBYTES(concat(`DEMUX', PIPELINE_ID), PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the mixer control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 304),
	,	concat(`demux_priv_', PIPELINE_ID))

# EQ IIR Bytes control
define(DEF_EQIIR_COEF, concat(`eqiir_coef_', PIPELINE_ID))
define(DEF_EQIIR_PRIV, concat(`eqiir_priv_', PIPELINE_ID))

# define filter. eq_iir_coef_bandsplit.m4 is set by default
ifdef(`PIPELINE_FILTER1', , `define(PIPELINE_FILTER1, eq_iir_coef_bandsplit.m4)')
include(PIPELINE_FILTER1)

C_CONTROLBYTES(DEF_EQIIR_COEF, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 1024),
	,
	DEF_EQIIR_PRIV)

# 3-band DRC Bytes control
define(MULTIBAND_DRC_priv, concat(`multiband_drc_bytes_', PIPELINE_ID))
define(MY_MULTIBAND_DRC_CTRL, concat(`multiband_drc_control_', PIPELINE_ID))
include(`multiband_drc_coef_default.m4')
C_CONTROLBYTES(MY_MULTIBAND_DRC_CTRL, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes, 258 binds the control to bytes get/put handlers, 258, 258),
	CONTROLBYTES_EXTOPS(258 binds the control to bytes get/put handlers, 258, 258),
	, , ,
	CONTROLBYTES_MAX(, 1024),
	,
	MULTIBAND_DRC_priv)

#
# Components and Buffers
#

# Host "Speaker Playback" PCM
# with 2 sink and 0 source periods
W_PCM_PLAYBACK(PCM_ID, Speaker Playback, 2, 0, SCHEDULE_CORE)

# "EQ IIR" has x sink period and 2 source periods
W_EQ_IIR(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`		', "DEF_EQIIR_COEF"))

# Mux 0 has 2 sink and source periods.
W_MUXDEMUX(0, 1, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`         ', concat(`DEMUX', PIPELINE_ID)))

# "MULTIBAND_DRC" has 2 sink periods and 2 source periods
W_MULTIBAND_DRC(0, PIPELINE_FORMAT, 2, 2, SCHEDULE_CORE,
	LIST(`   ', "MY_MULTIBAND_DRC_CTRL"))

# Low Latency Buffers
W_BUFFER(0, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(1, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)
W_BUFFER(2, COMP_BUFFER_SIZE(2,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), PIPELINE_CHANNELS, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_HOST_MEM_CAP)
W_BUFFER(3, COMP_BUFFER_SIZE(DAI_PERIODS,
	COMP_SAMPLE_SIZE(PIPELINE_FORMAT), 4, COMP_PERIOD_FRAMES(PCM_MAX_RATE, SCHEDULE_PERIOD)),
	PLATFORM_COMP_MEM_CAP)

#
# Pipeline Graph
#
#  host PCM_P --B0--> Demux(M) --B1--> eq_iir --B2--> multiband_drc --B3--> sink DAI0

P_GRAPH(pipe-demux-eq-drc-playback, PIPELINE_ID,
	LIST(`		',
	`dapm(N_BUFFER(0), N_PCMP(PCM_ID))',
	`dapm(N_MUXDEMUX(0), N_BUFFER(0))',
	`dapm(N_BUFFER(1), N_MUXDEMUX(0))',
	`dapm(N_EQ_IIR(0), N_BUFFER(1))',
	`dapm(N_BUFFER(2), N_EQ_IIR(0))'
	`dapm(N_MULTIBAND_DRC(0), N_BUFFER(2))',
	`dapm(N_BUFFER(3), N_MULTIBAND_DRC(0))'))

#
# Pipeline Source and Sinks
#
indir(`define', concat(`PIPELINE_SOURCE_', PIPELINE_ID), N_BUFFER(3))
indir(`define', concat(`PIPELINE_PCM_', PIPELINE_ID), Speaker Playback PCM_ID)

#
# PCM Configuration
#


# PCM capabilities supported by FW
PCM_CAPABILITIES(Speaker Playback PCM_ID, CAPABILITY_FORMAT_NAME(PIPELINE_FORMAT), 48000, 48000, 2, PIPELINE_CHANNELS, 2, 16, 192, 16384, 65536, 65536)

undefine(`DEF_EQIIR_COEF')
undefine(`DEF_EQIIR_PRIV')
undefine(`MY_MULTIBAND_DRC_CTRL')
undefine(`MULTIBAND_DRC_priv')

