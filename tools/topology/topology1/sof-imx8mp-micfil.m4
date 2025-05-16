#
# Topology for i.MX8M with MICFIL found on IMX-AUD-HAT
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`micfil.m4')
# Include TLV library
include(`common/tlv.m4')

# Include Token library
include(`sof/tokens.m4')

#Include DSP configuration
include(`platform/imx/imx8.m4')

#DMIC
# Capture pipeline 3 on PCM 1 using max 4 channels of s32le.
PIPELINE_PCM_ADD(sof/pipe-passthrough-capture.m4,
        1, 0, 4, s32le,
        1000, 0, 0,
        48000, 48000, 48000)

DAI_ADD(sof/pipe-dai-capture.m4, 1, MICFIL, 2, micfil-dmic-hifi,
PIPELINE_SINK_1, 2, s32le, 1000, 0, 0, SCHEDULE_TIME_DOMAIN_TIMER)

dnl DAI_CONFIG(type, dai_index, link_id, name, acpdmic_config)
DAI_CONFIG(MICFIL, 0, 0, micfil-dmic-hifi,
	   MICFIL_CONFIG(MICFIL_CONFIG_DATA(MICFIL, 0, 48000, 4)))

# PCM id 1
PCM_CAPTURE_ADD(MICFIL, 0, PIPELINE_PCM_1)
#/**********************************************************************************/
