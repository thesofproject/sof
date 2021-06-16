divert(-1)

dnl Define macro for Eq effect widget
DECLARE_SOF_RT_UUID("eq-fir", eq_fir_uuid, 0x43a90ce7, 0xf3a5, 0x41df,
		 0xac, 0x06, 0xba, 0x98, 0x65, 0x1a, 0xe6, 0xa3)

dnl N_EQ_FIR(name)
define(`N_EQ_FIR', `EQFIR'PIPELINE_ID`.'$1)

dnl W_EQ(name, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_EQ_FIR',
`SectionVendorTuples."'N_EQ_FIR($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(eq_fir_uuid)
`	}'
`}'
`SectionData."'N_EQ_FIR($1)`_data_uuid" {'
`	tuples "'N_EQ_FIR($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_EQ_FIR($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_EQ_FIR($1)`_data_w" {'
`	tuples "'N_EQ_FIR($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_EQ_FIR($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_EQ_FIR($1)`_data_str" {'
`	tuples "'N_EQ_FIR($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_EQ_FIR($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"EQFIR"
`	}'
`}'
`SectionData."'N_EQ_FIR($1)`_data_str_type" {'
`	tuples "'N_EQ_FIR($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_EQ_FIR($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_EQ_FIR($1)`_data_uuid"'
`		"'N_EQ_FIR($1)`_data_w"'
`		"'N_EQ_FIR($1)`_data_str"'
`		"'N_EQ_FIR($1)`_data_str_type"'
`	]'
`	bytes ['
		$6
`	]'
`}')

divert(0)dnl
