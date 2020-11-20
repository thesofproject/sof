divert(-1)

dnl Define macro for TDFB (time domain fixed beamformer) widget
DECLARE_SOF_RT_UUID("tdfb", tdfb_uuid, 0xdd511749, 0xd9fa, 0x455c,
                    0xb3, 0xa7, 0x13, 0x58, 0x56, 0x93, 0xf1, 0xaf)

dnl TDFB(name)
define(`N_TDFB', `TDFB'PIPELINE_ID`.'$1)

dnl W_TDFB(name, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_TDFB',
`SectionVendorTuples."'N_TDFB($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(tdfb_uuid)
`	}'
`}'
`SectionData."'N_TDFB($1)`_data_uuid" {'
`	tuples "'N_TDFB($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_TDFB($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_TDFB($1)`_data_w" {'
`	tuples "'N_TDFB($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_TDFB($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_TDFB($1)`_data_str" {'
`	tuples "'N_TDFB($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_TDFB($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"TDFB"
`	}'
`}'
`SectionData."'N_TDFB($1)`_data_str_type" {'
`	tuples "'N_TDFB($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_TDFB($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_TDFB($1)`_data_uuid"'
`		"'N_TDFB($1)`_data_w"'
`		"'N_TDFB($1)`_data_str"'
`		"'N_TDFB($1)`_data_str_type"'
`	]'
`	bytes ['
		$6
`	]'
`	enum ['
		$7
		$8
`	]'
`}')

divert(0)dnl
