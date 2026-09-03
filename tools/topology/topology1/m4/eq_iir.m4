divert(-1)

dnl Define macro for Eq effect widget
DECLARE_SOF_RT_UUID("eq-iir", eq_iir_uuid, 0x5150c0e6, 0x27f9, 0x4ec8,
		 0x83, 0x51, 0xc7, 0x05, 0xb6, 0x42, 0xd1, 0x2f)

dnl N_EQ_IIR(name)
define(`N_EQ_IIR', `EQIIR'PIPELINE_ID`.'$1)

dnl W_EQ(name, format, periods_sink, periods_source, core, kcontrols_list)
define(`W_EQ_IIR',
`SectionVendorTuples."'N_EQ_IIR($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(eq_iir_uuid)
`	}'
`}'
`SectionData."'N_EQ_IIR($1)`_data_uuid" {'
`	tuples "'N_EQ_IIR($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_EQ_IIR($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_EQ_IIR($1)`_data_w" {'
`	tuples "'N_EQ_IIR($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_EQ_IIR($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_EQ_IIR($1)`_data_str" {'
`	tuples "'N_EQ_IIR($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_EQ_IIR($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"EQIIR"
`	}'
`}'
`SectionData."'N_EQ_IIR($1)`_data_str_type" {'
`	tuples "'N_EQ_IIR($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_EQ_IIR($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_EQ_IIR($1)`_data_uuid"'
`		"'N_EQ_IIR($1)`_data_w"'
`		"'N_EQ_IIR($1)`_data_str"'
`		"'N_EQ_IIR($1)`_data_str_type"'
`	]'
`	bytes ['
		$6
`	]'
`}')

divert(0)dnl
