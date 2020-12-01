divert(-1)

dnl Define macro for Multiband DRC widget
DECLARE_SOF_RT_UUID("multiband_drc", multiband_drc_uuid, 0x0d9f2256, 0x8e4f, 0x47b3,
		    0x84, 0x48, 0x23, 0x9a, 0x33, 0x4f, 0x11, 0x91);

dnl N_MULTIBAND_DRC(name)
define(`N_MULTIBAND_DRC', `MULTIBAND_DRC'PIPELINE_ID`.'$1)

dnl W_MULTIBAND_DRC(name, format, periods_sink, periods_source, core, kcontrol_list)
define(`W_MULTIBAND_DRC',
`SectionVendorTuples."'N_MULTIBAND_DRC($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(multiband_drc_uuid)
`	}'
`}'
`SectionData."'N_MULTIBAND_DRC($1)`_data_uuid" {'
`	tuples "'N_MULTIBAND_DRC($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_MULTIBAND_DRC($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_MULTIBAND_DRC($1)`_data_w" {'
`	tuples "'N_MULTIBAND_DRC($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_MULTIBAND_DRC($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_MULTIBAND_DRC($1)`_data_str" {'
`	tuples "'N_MULTIBAND_DRC($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_MULTIBAND_DRC($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"MULTIBAND_DRC"
`	}'
`}'
`SectionData."'N_MULTIBAND_DRC($1)`_data_str_type" {'
`	tuples "'N_MULTIBAND_DRC($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_MULTIBAND_DRC($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_MULTIBAND_DRC($1)`_data_uuid"'
`		"'N_MULTIBAND_DRC($1)`_data_w"'
`		"'N_MULTIBAND_DRC($1)`_data_str"'
`		"'N_MULTIBAND_DRC($1)`_data_str_type"'
`	]'
`	bytes ['
		$6
`	]'
`}')

divert(0)dnl
