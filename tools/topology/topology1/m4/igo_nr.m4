divert(-1)

dnl Define macro for IGO_NR widget
DECLARE_SOF_RT_UUID("igo-nr", igo_nr_uuid, 0x696ae2bc, 0x2877, 0x11eb, 0xad, 0xc1,
            0x02, 0x42, 0xac, 0x12, 0x00, 0x02)

dnl N_IGO_NR(name)
define(`N_IGO_NR', `IGONR'PIPELINE_ID`.'$1)

dnl W_IGO_NR(name, format, periods_sink, periods_source, core, kcontrol0. kcontrol1...etc)
define(`W_IGO_NR',
`SectionVendorTuples."'N_IGO_NR($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(igo_nr_uuid)
`	}'
`}'
`SectionData."'N_IGO_NR($1)`_data_uuid" {'
`	tuples "'N_IGO_NR($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_IGO_NR($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'				STR($5)
`	}'
`}'
`SectionData."'N_IGO_NR($1)`_data_w" {'
`	tuples "'N_IGO_NR($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_IGO_NR($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_IGO_NR($1)`_data_str" {'
`	tuples "'N_IGO_NR($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_IGO_NR($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"IGO_NR"
`	}'
`}'
`SectionData."'N_IGO_NR($1)`_data_str_type" {'
`	tuples "'N_IGO_NR($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_IGO_NR($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_IGO_NR($1)`_data_uuid"'
`		"'N_IGO_NR($1)`_data_w"'
`		"'N_IGO_NR($1)`_data_str"'
`		"'N_IGO_NR($1)`_data_str_type"'
`	]'
`	bytes ['
		$6
`	]'

`}')

divert(0)dnl
