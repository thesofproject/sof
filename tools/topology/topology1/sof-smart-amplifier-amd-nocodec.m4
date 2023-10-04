#
# Topology for Vangogh smart amplifier
#

# Include topology builder
include(`utils.m4')
include(`dai.m4')
include(`pipeline.m4')

# Include Token library
include(`sof/tokens.m4')

# Include ACP DSP configuration
include(`platform/amd/acp.m4')


DEBUG_START

#
# Define the pipelines
#
# PCM1 ----> smart_amp ----> I2SHS
#             ^
#             |
#             |
# PCM1 <---- demux <----- I2SHS
#

# Smart amplifier related
#0->1

#define BE dai_link ID
#TBD
#0->1
define(`SMART_BE_ID', 1)

# Playback related
#2->1
define(`SMART_PB_PPL_ID', 2)
define(`SMART_PB_CH_NUM', 2)
define(`SMART_TX_CHANNELS', 2)
define(`SMART_RX_CHANNELS', 2)
define(`SMART_FB_CHANNELS', 2)
# Ref capture related
#7->2
define(`SMART_REF_PPL_ID', 6)
define(`SMART_REF_CH_NUM', 2)
# PCM related
#1->0
define(`SMART_PCM_ID', 1)
define(`SMART_PCM_NAME', `I2SHS')

# Include Smart Amplifier support
include(`sof-smart-amplifier-amd.m4')

DEBUG_END
