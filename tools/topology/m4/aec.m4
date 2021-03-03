divert(-1)

dnl Define macro for AEC effect widget
DECLARE_SOF_RT_UUID("aec", aec_uuid, 0x2ca424c0, 0x7e1c, 0x4b0a,
		    0xaf, 0xca, 0x43, 0xde, 0x94, 0x47, 0x05, 0xd3)

dnl AEC(name)
define(`N_AEC', `AEC'PIPELINE_ID`.'$1)

dnl W_AEC(name, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_AEC',
`SectionVendorTuples."'N_AEC($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(aec_uuid)
`	}'
`}'
`SectionData."'N_AEC($1)`_data_uuid" {'
`	tuples "'N_AEC($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_AEC($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_AEC($1)`_data_w" {'
`	tuples "'N_AEC($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_AEC($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_AEC($1)`_data_str" {'
`	tuples "'N_AEC($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_AEC($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"AEC"
`	}'
`}'
`SectionData."'N_AEC($1)`_data_str_type" {'
`	tuples "'N_AEC($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_AEC($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_AEC($1)`_data_uuid"'
`		"'N_AEC($1)`_data_w"'
`		"'N_AEC($1)`_data_str"'
`		"'N_AEC($1)`_data_str_type"'
`	]'
`	bytes ['
		$6
`	]'
`}')

divert(0)dnl
