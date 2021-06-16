divert(-1)

include(`debug.m4')

dnl Define macros for DAI IN/OUT widgets and DAI config
DECLARE_SOF_RT_UUID("dai", dai_comp_uuid, 0xc2b00d27, 0xffbc, 0x4150,
		 0xa5, 0x1a, 0x24, 0x5c, 0x79, 0xc5, 0xe5, 0x4b)

dnl N_DAI(name)
define(`N_DAI', DAI_NAME)
define(`N_DAI_OUT', DAI_NAME`.OUT')
define(`N_DAI_IN', DAI_NAME`.IN')

dnl W_DAI_OUT(type, index, dai_link, format, periods_sink, periods_source, core)
define(`W_DAI_OUT',
`SectionVendorTuples."'N_DAI_OUT`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(dai_comp_uuid)
`	}'
`}'
`SectionData."'N_DAI_OUT`_data_uuid" {'
`	tuples "'N_DAI_OUT`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_DAI_OUT`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($5)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($6)
`		SOF_TKN_COMP_CORE_ID'			STR($7)
`	}'
`}'
`SectionData."'N_DAI_OUT`_data_w_comp" {'
`	tuples "'N_DAI_OUT`_tuples_w_comp"'
`}'
`SectionVendorTuples."'N_DAI_OUT`_tuples_w" {'
`	tokens "sof_dai_tokens"'
`	tuples."word" {'
`		SOF_TKN_DAI_INDEX'	$2
`		SOF_TKN_DAI_DIRECTION'	"0"
`	}'
`}'
`SectionData."'N_DAI_OUT`_data_w" {'
`	tuples "'N_DAI_OUT`_tuples_w"'
`}'
`SectionVendorTuples."'N_DAI_OUT`_tuples_str" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'	$1
`	}'
`}'
`SectionData."'N_DAI_OUT`_data_str" {'
`	tuples "'N_DAI_OUT`_tuples_str"'
`}'
`SectionVendorTuples."'N_DAI_OUT`_tuples_comp_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($4)
`	}'
`}'
`SectionData."'N_DAI_OUT`_data_comp_str" {'
`	tuples "'N_DAI_OUT`_tuples_comp_str"'
`}'
`SectionWidget."'N_DAI_OUT`" {'
`	index "'PIPELINE_ID`"'
`	type "dai_in"'
`	stream_name' STR($3)
`	no_pm "true"'
`	data ['
`		"'N_DAI_OUT`_data_uuid"'
`		"'N_DAI_OUT`_data_w"'
`		"'N_DAI_OUT`_data_w_comp"'
`		"'N_DAI_OUT`_data_str"'
`		"'N_DAI_OUT`_data_comp_str"'
`	]'
`}')

dnl W_DAI_IN(type, index, dai_link, format, periods_sink, periods_source, core)
define(`W_DAI_IN',
`SectionVendorTuples."'N_DAI_IN`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(dai_comp_uuid)
`	}'
`}'
`SectionData."'N_DAI_IN`_data_uuid" {'
`	tuples "'N_DAI_IN`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_DAI_IN`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($5)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($6)
`		SOF_TKN_COMP_CORE_ID'			STR($7)
`	}'
`}'
`SectionData."'N_DAI_IN`_data_w_comp" {'
`	tuples "'N_DAI_IN`_tuples_w_comp"'
`}'
`SectionVendorTuples."'N_DAI_IN`_tuples_w" {'
`	tokens "sof_dai_tokens"'
`	tuples."word" {'
`		SOF_TKN_DAI_INDEX'	$2
`		SOF_TKN_DAI_DIRECTION'	"1"
`	}'
`}'
`SectionData."'N_DAI_IN`_data_w" {'
`	tuples "'N_DAI_IN`_tuples_w"'
`}'
`SectionVendorTuples."'N_DAI_IN`_tuples_str" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'	$1
`	}'
`}'
`SectionData."'N_DAI_IN`_data_str" {'
`	tuples "'N_DAI_IN`_tuples_str"'
`}'
`SectionVendorTuples."'N_DAI_IN`_tuples_comp_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($4)
`	}'
`}'
`SectionData."'N_DAI_IN`_data_comp_str" {'
`	tuples "'N_DAI_IN`_tuples_comp_str"'
`}'
`SectionWidget."'N_DAI_IN`" {'
`	index "'PIPELINE_ID`"'
`	type "dai_out"'
`	stream_name' STR($3)
`	no_pm "true"'
`	data ['
`		"'N_DAI_IN`_data_uuid"'
`		"'N_DAI_IN`_data_w"'
`		"'N_DAI_IN`_data_w_comp"'
`		"'N_DAI_IN`_data_str"'
`		"'N_DAI_IN`_data_comp_str"'
`	]'
`}')

dnl D_DAI(id, playback, capture, data))
define(`D_DAI', `SectionDAI."'N_DAI`" {'
`	index "'PIPELINE_ID`"'
`	id "'$1`"'
`	playback "'$2`"'
`	capture "'$3`"'
`}')

dnl DAI Config)
define(`N_DAI_CONFIG', `DAICONFIG.'$1)
dnl DAI_CONFIG(type, idx, link_id, name, sai_config/esai_config/ssp_config/dmic_config)
define(`DO_DAI_CONFIG',
`SectionHWConfig."'$1$2`" {'
`'
`	id		"'$3`"'
`'
`	ifelse($1, `SSP', $5, $1, `HDA', $5, $1, `ALH', $5, $1, `ESAI', $5, $1, `SAI', $5, `}')'
`ifelse($1, `DMIC', $5, `')'
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples_common" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'		STR($1)
`	}'
`	tuples."word" {'
`		SOF_TKN_DAI_INDEX'		STR($2)
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data_common" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples_common"'
`}'
`'
`SectionBE."'$4`" {'
`	id "'$3`"'
`	index "0"'
`	default_hw_conf_id	"'$3`"'
`'
`	hw_configs ['
`		"'$1$2`"'
`	]'
`	data ['
`		"'N_DAI_CONFIG($1$2)`_data"'
`		"'N_DAI_CONFIG($1$2)`_data_common"'
`ifelse($1, `DMIC',`		"'N_DAI_CONFIG($1$2)`_pdm_data"', `')'
`	]'
`}'
`DEBUG_DAI_CONFIG($1, $3)'
)

dnl DAI_CONFIG(type, idx, link_id, name, ssp_config/dmic_config)
define(`DAI_CONFIG',
`ifelse(`$#', `5', `DO_DAI_CONFIG($1, $2, $3, $4, $5)', `$#', `4', `DO_DAI_CONFIG($1, $2, $3, $4)', `fatal_error(`Invalid parameters ($#) to DAI_CONFIG')')'
)

define(`HW_CONFIG_NAMES',
`	hw_configs ['
`	$1'
`	]')

define(`DAI_DATA_NAMES',
`	data ['
`	$1'
`		"N_DAI_CONFIG_data_common"')

dnl DO_MULTI_DAI_CONFIG(type, idx, link_id, name, sai_config/esai_config/ssp_config/dmic_config)
define(`DO_MULTI_DAI_CONFIG',
`$5'
`SectionVendorTuples."N_DAI_CONFIG_tuples_common" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'		STR($1)
`	}'
`	tuples."word" {'
`		SOF_TKN_DAI_INDEX'		STR($2)
`	}'
`}'
`SectionData."N_DAI_CONFIG_data_common" {'
`	tuples "N_DAI_CONFIG_tuples_common"'
`}'
`'
`SectionBE."'$4`" {'
`	id "'$3`"'
`	index "0"'
`	default_hw_conf_id	"'$3`"'
`'
`	$6'
`	$7'
`ifelse($1, `DMIC',`		"'N_DAI_CONFIG($1$2)`_pdm_data"', `')'
`	]'
`}'
`DEBUG_DAI_CONFIG($1, $3)'
)

dnl MULTI_DAI_CONFIG(type, idx, link_id, name, ssp_config/dmic_config times n)
 define(`MULTI_DAI_CONFIG',
`ifelse(`eval($# < 5)', `1', `fatal_error(`Invalid parameters ($#) to MULTI_DAI_CONFIG')', `DO_MULTI_DAI_CONFIG($@)')'
)

dnl DAI_ADD(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period , priority, core, time_domain,
dnl     channels, rate, dynamic_pipe)
define(`DAI_ADD',
`undefine(`PIPELINE_ID')'
`undefine(`DAI_TYPE')'
`undefine(`DAI_INDEX')'
`undefine(`DAI_BE')'
`undefine(`DAI_BUF')'
`undefine(`DAI_PERIODS')'
`undefine(`DAI_FORMAT')'
`undefine(`SCHEDULE_PERIOD')'
`undefine(`SCHEDULE_PRIORITY')'
`undefine(`SCHEDULE_CORE')'
`undefine(`SCHEDULE_TIME_DOMAIN')'
`undefine(`DAI_CHANNELS')'
`undefine(`DAI_RATE')'
`undefine(`DYNAMIC_PIPE')'
`define(`PIPELINE_ID', $2)'
`define(`DAI_TYPE', STR($3))'
`define(`DAI_INDEX', STR($4))'
`define(`DAI_BE', $5)'
`define(`DAI_BUF', $6)'
`define(`DAI_NAME', $3$4)'
`define(`DAI_PERIODS', $7)'
`define(`DAI_FORMAT', $8)'
`define(`SCHEDULE_PERIOD', $9)'
`define(`SCHEDULE_PRIORITY', $10)'
`define(`SCHEDULE_CORE', $11)'
`define(`SCHEDULE_TIME_DOMAIN', $12)'
`define(`DAI_CHANNELS', $13)'
`define(`DAI_RATE', $14)'
`define(`DYNAMIC_PIPE', $15)'
`include($1)'
`DEBUG_DAI($3, $4)'
)

# DAI_ADD_SCHED can be used for adding a DAI with sched_comp
dnl DAI_ADD_SCHED(pipeline,
dnl     pipe id, dai type, dai_index, dai_be,
dnl     buffer, periods, format,
dnl     period , priority, core, time_domain, sched_comp)
define(`DAI_ADD_SCHED',
`undefine(`SCHED_COMP')'
`define(`SCHED_COMP', $13)'
`DAI_ADD($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)'
)
divert(0)dnl
