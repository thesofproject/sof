#
# HDMI-SSP Audio Offload support
#

include(`ssp.m4')

define(`HDMI_SSP_NAME', concat(concat(`SSP', HDMI_SSP_NUM),`-HDMI'))
define(`HDMI_SSP_PCM_NAME', concat(concat(`HDMI-', HDMI_SSP_NUM),`-In'))

# variable that need to be defined in upper m4
ifdef(`HDMI_SSP_PIPELINE_CP_ID',`',`fatal_error(note: Need to define capture pcm id for ssp intel-hdmi-in
)')
ifdef(`HDMI_SSP_DAI_LINK_ID',`',`fatal_error(note: Need to define DAI link id for ssp intel-hdmi-in
)')
ifdef(`HDMI_SSP_PCM_ID',`',`fatal_error(note: Need to define pipeline PCM dev id for ssp intel-hdmi-in
)')


# Low Latency capture pipeline 4 on PCM HDMI_SSP_PCM_ID using max 2 channels of s32le.
# 1000us deadline with priority 0 on core 0
PIPELINE_PCM_ADD(sof/pipe-low-latency-capture.m4,
	HDMI_SSP_PIPELINE_CP_ID, HDMI_SSP_PCM_ID, 2, s32le,
	1000, 0, 0,
	48000, 48000, 48000)


# capture DAI is SSP using 2 periods
# Buffers use s32le format, 1000us deadline with priority 0 on core 0
DAI_ADD(sof/pipe-dai-capture.m4,
	HDMI_SSP_PIPELINE_CP_ID, SSP, HDMI_SSP_NUM, HDMI_SSP_NAME,
	concat(`PIPELINE_SINK_', HDMI_SSP_PIPELINE_CP_ID), 2, s32le,
	1000, 0, 0, SCHEDULE_TIME_DOMAIN_DMA)


PCM_CAPTURE_ADD(HDMI_SSP_PCM_NAME, HDMI_SSP_PCM_ID, concat(`PIPELINE_PCM_', HDMI_SSP_PIPELINE_CP_ID))


#BE configuration in slave mode
DAI_CONFIG(SSP, HDMI_SSP_NUM, HDMI_SSP_DAI_LINK_ID, HDMI_SSP_NAME,
	   SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
		      SSP_CLOCK(bclk, 3072000, codec_provider),
		      SSP_CLOCK(fsync, 48000, codec_provider),
		      SSP_TDM(2, 32, 3, 3),
		      SSP_CONFIG_DATA(SSP, HDMI_SSP_NUM, 32, 0)))


undefine(`HDMI_SSP_PIPELINE_CP_ID')
undefine(`HDMI_SSP_DAI_LINK_ID')
undefine(`HDMI_SSP_PCM_ID') dnl use fixed PCM_ID
undefine(`HDMI_SSP_NUM')
undefine(`HDMI_SSP_NAME')
undefine(`HDMI_SSP_PCM_NAME')

