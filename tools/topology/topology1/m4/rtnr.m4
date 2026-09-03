divert(-1)

dnl Define macro for RTNR effect widget
DECLARE_SOF_RT_UUID("rtnr", rtnr_uuid, 0x5c7ca334, 0xe15d, 0x11eb, 0xba, 0x80,
		    0x02, 0x42, 0xac, 0x13, 0x00, 0x04)

dnl RTNR(name)
define(`N_RTNR', `RTNR'PIPELINE_ID`.'$1)

dnl W_RTNR(name, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_RTNR',
`SectionVendorTuples."'N_RTNR($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(rtnr_uuid)
`	}'
`}'
`SectionData."'N_RTNR($1)`_data_uuid" {'
`	tuples "'N_RTNR($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_RTNR($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_RTNR($1)`_data_w" {'
`	tuples "'N_RTNR($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_RTNR($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_RTNR($1)`_data_str" {'
`	tuples "'N_RTNR($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_RTNR($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"RTNR"
`	}'
`}'
`SectionData."'N_RTNR($1)`_data_str_type" {'
`	tuples "'N_RTNR($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_RTNR($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_RTNR($1)`_data_uuid"'
`		"'N_RTNR($1)`_data_w"'
`		"'N_RTNR($1)`_data_str"'
`		"'N_RTNR($1)`_data_str_type"'
`	]'
`	bytes ['
		$6
`	]'
`	mixer ['
		$7
`	]'
`}')

divert(0)dnl
