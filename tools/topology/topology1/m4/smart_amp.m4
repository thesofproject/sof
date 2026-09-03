divert(-1)

dnl Define macro for smart_amp(Smart Amplifier) widget

dnl SMART_AMP name)
define(`N_SMART_AMP', `SMART_AMP'PIPELINE_ID`.'$1)

dnl W_SMART_AMP(name, uuid, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_SMART_AMP',
`SectionVendorTuples."'N_SMART_AMP($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR($2)
`	}'
`}'
`SectionData."'N_SMART_AMP($1)`_data_uuid" {'
`	tuples "'N_SMART_AMP($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_SMART_AMP($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($4)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($5)
`               SOF_TKN_COMP_CORE_ID'                   STR($6)
`	}'
`}'
`SectionData."'N_SMART_AMP($1)`_data_w" {'
`	tuples "'N_SMART_AMP($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_SMART_AMP($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($3)
`	}'
`}'
`SectionVendorTuples."'N_SMART_AMP($1)`_process_tuples_str" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"SMART_AMP"
`	}'
`}'
`SectionData."'N_SMART_AMP($1)`_data_str" {'
`	tuples "'N_SMART_AMP($1)`_tuples_str"'
`	tuples "'N_SMART_AMP($1)`_process_tuples_str"'
`}'
`SectionWidget."'N_SMART_AMP($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_SMART_AMP($1)`_data_uuid"'
`		"'N_SMART_AMP($1)`_data_w"'
`		"'N_SMART_AMP($1)`_data_str"'
`	]'
`	bytes ['
		$7
`	]'
`}')

divert(0)dnl
