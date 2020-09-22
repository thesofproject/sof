divert(-1)

dnl Define macro for Dyanamic Range Compressor widget
DECLARE_SOF_RT_UUID("drc", drc_uuid, 0xb36ee4da, 0x006f, 0x47f9,
		    0xa0, 0x6d, 0xfe, 0xcb, 0xe2, 0xd8, 0xb6, 0xce);

dnl N_DRC(name)
define(`N_DRC', `DRC'PIPELINE_ID`.'$1)

dnl W_DRC(name, format, periods_sink, periods_source, core, kcontrol_list)
define(`W_DRC',
`SectionVendorTuples."'N_DRC($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(drc_uuid)
`	}'
`}'
`SectionData."'N_DRC($1)`_data_uuid" {'
`	tuples "'N_DRC($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_DRC($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_DRC($1)`_data_w" {'
`	tuples "'N_DRC($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_DRC($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_DRC($1)`_data_str" {'
`	tuples "'N_DRC($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_DRC($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"DRC"
`	}'
`}'
`SectionData."'N_DRC($1)`_data_str_type" {'
`	tuples "'N_DRC($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_DRC($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_DRC($1)`_data_uuid"'
`		"'N_DRC($1)`_data_w"'
`		"'N_DRC($1)`_data_str"'
`		"'N_DRC($1)`_data_str_type"'
`	]'
`	bytes ['
		$6
`	]'
`}')

divert(0)dnl
