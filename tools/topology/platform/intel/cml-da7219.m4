include(`platform/intel/cml.m4')

#SSP/DMIC machine specific settings on CML platform
undefine(`SSP_INDEX')
define(`SSP_INDEX', 0)

undefine(`SSP_NAME')
define(`SSP_NAME', `SSP0-Codec')

undefine(`DMIC_PCM_NUM')
define(`DMIC_PCM_NUM', `2')

undefine(`UNUSED_SSP_ROUTE1')
define(`UNUSED_SSP_ROUTE1', `ssp5')

undefine(`UNUSED_SSP_ROUTE2')
define(`UNUSED_SSP_ROUTE2', `ssp1')
