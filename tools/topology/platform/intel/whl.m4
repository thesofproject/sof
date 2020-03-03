include(`platform/intel/cnl.m4')

#SSP setting for WHL platform
undefine(`SSP_INDEX')
define(`SSP_INDEX', 1)

undefine(`SSP_NAME')
define(`SSP_NAME', `SSP1-Codec')

undefine(`SSP_MCLK_RATE')
define(`SSP_MCLK_RATE', `24000000')

include(`platform/intel/dmic.m4')
