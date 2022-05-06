divert(-1)

dnl Define macro for example Amp widget

dnl fd48f000-c316-4ec2-9ff8-edb4efa3f52c
DECLARE_SOF_RT_UUID("google-audio-post-processing", gapp_uuid, 0xfd48f000, 0xc316, 0x4ec2, 0x9f,
		0xf8, 0xed, 0xb4, 0xef, 0xa3, 0xf5, 0x2c);

dnl GAPP(name)
define(`N_GAPP', `GAPP'PIPELINE_ID`.'$1)

dnl W_GAPP(name, format, periods_sink, periods_source, kcontrols_list)
define(`W_GAPP',
`SectionVendorTuples."'N_GAPP($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'	  STR(gapp_uuid)
`	}'
`}'
`SectionData."'N_GAPP($1)`_data_uuid" {'
`	tuples "'N_GAPP($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_GAPP($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_GAPP($1)`_data_w" {'
`	tuples "'N_GAPP($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_GAPP($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_GAPP($1)`_data_str" {'
`	tuples "'N_GAPP($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_GAPP($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'   "GAPP"
`	}'
`}'
`SectionData."'N_GAPP($1)`_data_str_type" {'
`	tuples "'N_GAPP($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_GAPP($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_GAPP($1)`_data_uuid"'
`		"'N_GAPP($1)`_data_w"'
`		"'N_GAPP($1)`_data_str"'
`		"'N_GAPP($1)`_data_str_type"'
`	]'
`	mixer ['
		$6
`	]'
`	bytes ['
		$7
`	]'
`}')

divert(0)dnl
