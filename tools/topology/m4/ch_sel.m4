divert(-1)

dnl Define macro for channel selector widget
DECLARE_SOF_RT_UUID("selector", selector_uuid, 0x55a88ed5, 0x3d18, 0x46ca,
		 0x88, 0xf1, 0x0e, 0xe6, 0xea, 0xe9, 0x93, 0x0f)

dnl N_SELECTOR(name)
define(`N_SELECTOR', `SELECTOR'PIPELINE_ID`.'$1)

dnl W_SELECTOR(name, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_SELECTOR',
`SectionVendorTuples."'N_SELECTOR($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(selector_uuid)
`	}'
`}'
`SectionData."'N_SELECTOR($1)`_data_uuid" {'
`	tuples "'N_SELECTOR($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_SELECTOR($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_SELECTOR($1)`_data_w" {'
`	tuples "'N_SELECTOR($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_SELECTOR($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionVendorTuples."'N_SELECTOR($1)`_process_tuples_str" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"CHAN_SELECTOR"
`	}'
`}'
`SectionData."'N_SELECTOR($1)`_data_str" {'
`	tuples "'N_SELECTOR($1)`_tuples_str"'
`	tuples "'N_SELECTOR($1)`_process_tuples_str"'
`}'
`SectionWidget."'N_SELECTOR($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_SELECTOR($1)`_data_uuid"'
`		"'N_SELECTOR($1)`_data_w"'
`		"'N_SELECTOR($1)`_data_str"'
`	]'
`	bytes ['
		$6
`	]'
`}')

divert(0)dnl
