divert(-1)

dnl Define macro for EAP (Essential Audio Processing) widget
DECLARE_SOF_RT_UUID("eap", eap_uuid, 0x127f4eec, 0x23fa, 0x11f0,
                    0xa4, 0xa6, 0xbf, 0x0c, 0xd6, 0xb4, 0x58, 0x3b)

dnl EAP(name)
define(`N_EAP', `EAP'PIPELINE_ID`.'$1)

dnl W_EAP(name, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_EAP',
`SectionVendorTuples."'N_EAP($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(eap_uuid)
`	}'
`}'
`SectionData."'N_EAP($1)`_data_uuid" {'
`	tuples "'N_EAP($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_EAP($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_EAP($1)`_data_w" {'
`	tuples "'N_EAP($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_EAP($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_EAP($1)`_data_str" {'
`	tuples "'N_EAP($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_EAP($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"EAP"
`	}'
`}'
`SectionData."'N_EAP($1)`_data_str_type" {'
`	tuples "'N_EAP($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_EAP($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_EAP($1)`_data_uuid"'
`		"'N_EAP($1)`_data_w"'
`		"'N_EAP($1)`_data_str"'
`		"'N_EAP($1)`_data_str_type"'
`	]'
`	enum ['
		$6
		$7
`	]'
`	bytes ['
		$8
`	]'
`}')

divert(0)dnl
