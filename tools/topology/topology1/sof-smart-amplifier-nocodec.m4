#
# Topology for Tigerlake smart amplifier nocodec mode verification
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')
include(`ssp.m4')

# Include Token library
include(`sof/tokens.m4')

# Include platform specific DSP configuration
include(`platform/intel/tgl.m4')

DEBUG_START

#
# Define the pipelines
#
# PCM0 ----> smart_amp ----> SSP(SSP_INDEX)
#             ^
#             |
#             |
# PCM0 <---- demux <----- SSP(SSP_INDEX)
#

# Smart amplifier related
# SSP related
#define smart amplifier SSP index
define(`SMART_SSP_INDEX', 0)
#define SSP BE dai_link name
define(`SMART_SSP_NAME', `NoCodec-0')
#define BE dai_link ID
define(`SMART_BE_ID', 0)
#define SSP QUIRK
define(`SMART_SSP_QUIRK', `SSP_QUIRK_LBM')

# Playback related
define(`SMART_PB_PPL_ID', 1)
define(`SMART_PB_CH_NUM', 2)
define(`SMART_TX_CHANNELS', 4)
define(`SMART_RX_CHANNELS', 8)
define(`SMART_FB_CHANNELS', 8)
# Ref capture related
define(`SMART_REF_PPL_ID', 2)
define(`SMART_REF_CH_NUM', 4)
# PCM related
define(`SMART_PCM_ID', 0)
define(`SMART_PCM_NAME', `smart373-spk')

# Include Smart Amplifier support
include(`sof-smart-amplifier.m4')

DEBUG_END
