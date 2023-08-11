#
# Jasperlake differentiation for pipelines and components
#

include(`platform/intel/icl.m4')

define(`SPK_INDEX', `1')
define(`SPK_NAME', `SSP1-Codec')

undefine(`SPK_DATA_FORMAT')
define(`SPK_DATA_FORMAT', `s24le')

ifelse(HEADPHONE, `rt5650', `
define(`SET_SSP_CONFIG',
				`SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24576000, codec_mclk_in),
						SSP_CLOCK(bclk, 3072000, codec_slave),
						SSP_CLOCK(fsync, 48000, codec_slave),
						SSP_TDM(2, 32, 3, 3),
						SSP_CONFIG_DATA(SSP, 1, 24, 0, 0, 0, SSP_CC_MCLK_AON))')
', `
define(`SET_SSP_CONFIG',
				`SSP_CONFIG(I2S, SSP_CLOCK(mclk, 24000000, codec_mclk_in),
						SSP_CLOCK(bclk, 3072000, codec_slave),
						SSP_CLOCK(fsync, 48000, codec_slave),
						SSP_TDM(2, 32, 3, 3),
						SSP_CONFIG_DATA(SSP, 1, 24))')
')