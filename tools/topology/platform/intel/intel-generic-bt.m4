#
# Bluetooth Audio Offload support
#

include(`ssp.m4')

# define default SSP port
ifdef(`SSP_INDEX',`',
`define(SSP_INDEX, `2')')

define(`SCO_MCLK', 19200000)
define(`SSP_NAME', concat(concat(`SSP', SSP_INDEX),`-BT'))

# variable that need to be defined in upper m4
ifdef(`BT_PIPELINE_PB_ID',`',`fatal_error(note: Need to define playback pcm id for intel-generic-bt
)')
ifdef(`BT_PIPELINE_CP_ID',`',`fatal_error(note: Need to define capture pcm id for intel-generic-bt
)')
ifdef(`BT_DAI_LINK_ID',`',`fatal_error(note: Need to define DAI link id for intel-generic-bt
)')
ifdef(`BT_PCM_ID',`',`fatal_error(note: Need to define pipeline PCM dev id for intel-generic-bt
)')
ifdef(`HW_CONFIG_ID',`',`fatal_error(note: Need to define hw_conf_id for intel-generic-bt
)')

define(`CHANNELS_MIN', 1)

# Support 8K/16K mono and 48K stereo playback
PIPELINE_PCM_ADD(sof/pipe-passthrough-playback.m4,
	BT_PIPELINE_PB_ID, BT_PCM_ID, 2, s16le,
	1000, 0, 0,
	8000, 48000, 48000)

# We are done
undefine(`CHANNELS_MIN')

# Support 8K/16K mono capture only
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
	BT_PIPELINE_CP_ID, BT_PCM_ID, 1, s16le,
	1000, 0, 0,
	8000, 16000, 48000)

# playback DAI is SSP2 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-playback.m4,
	BT_PIPELINE_PB_ID, SSP, SSP_INDEX, SSP_NAME,
	concat(`PIPELINE_SOURCE_', BT_PIPELINE_PB_ID), 2, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

# capture DAI is SSP2 using 2 periods
# Buffers use s16le format, with 48 frame per 1000us on core 0 with priority 0
DAI_ADD(sof/pipe-dai-capture.m4,
	BT_PIPELINE_CP_ID, SSP, SSP_INDEX, SSP_NAME,
	concat(`PIPELINE_SINK_', BT_PIPELINE_CP_ID), 1, s16le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

PCM_DUPLEX_ADD(Bluetooth, BT_PCM_ID, concat(`PIPELINE_PCM_', BT_PIPELINE_PB_ID), concat(`PIPELINE_PCM_', BT_PIPELINE_CP_ID))

define(`hwconfig_names', HW_CONFIG_NAMES(LIST(`     ', "hw_config1", "hw_config2", "hw_config3")))
define(`data_names', DAI_DATA_NAMES(LIST(`     ', "ssp_data1", "ssp_data2", "ssp_data3")))

# Note WB is put as default in the list
define(`ssp_config_list_1', LIST(`',
	`MULTI_SSP_CONFIG(hw_config1, HW_CONFIG_ID, DSP_A, SSP_CLOCK(mclk, SCO_MCLK, codec_mclk_in),'
		`SSP_CLOCK(bclk, 256000, codec_master, inverted),'
		`SSP_CLOCK(fsync, 16000, codec_master),'
		`SSP_TDM(1, 16, 1, 1),'
		`SSP_MULTI_CONFIG_DATA(ssp_data1, 16))',
	`MULTI_SSP_CONFIG(hw_config2, eval(HW_CONFIG_ID + 1), DSP_A, SSP_CLOCK(mclk, SCO_MCLK, codec_mclk_in),'
		`SSP_CLOCK(bclk, 128000, codec_master, inverted),'
		`SSP_CLOCK(fsync, 8000, codec_master),'
		`SSP_TDM(1, 16, 1, 1),'
		`SSP_MULTI_CONFIG_DATA(ssp_data2, 16))',
	`MULTI_SSP_CONFIG(hw_config3, eval(HW_CONFIG_ID + 2), DSP_A, SSP_CLOCK(mclk, eval(SCO_MCLK * 2), codec_mclk_in),'
		`SSP_CLOCK(bclk, 1536000, codec_slave),'
		`SSP_CLOCK(fsync, 48000, codec_slave),'
		`SSP_TDM(2, 16, 3, 0),'
		`SSP_MULTI_CONFIG_DATA(ssp_data3, 16))'))

MULTI_DAI_CONFIG(SSP, SSP_INDEX, BT_DAI_LINK_ID, SSP_NAME, ssp_config_list_1, hwconfig_names, data_names)
