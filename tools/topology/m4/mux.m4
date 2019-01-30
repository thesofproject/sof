divert(-1)

dnl Define macro for Mux widget

dnl Mux Name)
define(`N_MUX', `MUX'PIPELINE_ID`.'$1)

dnl Pipe Buffer name in pipeline (pipeline, buffer)
define(`NPIPELINE_MUX', `MUX'$1`.'$2)

dnl W_MUX(name, format, periods_sink, periods_source, enum_list)
define(`W_MUX',
`SectionVendorTuples."'N_MUX($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`	}'
`}'
`SectionData."'N_MUX($1)`_data_w" {'
`	tuples "'N_MUX($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_MUX($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_MUX($1)`_data_str" {'
`	tuples "'N_MUX($1)`_tuples_str"'
`}'
`SectionWidget."'N_MUX($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "mux"'
`	no_pm "true"'
`	data ['
`		"'N_MUX($1)`_data_w"'
`		"'N_MUX($1)`_data_str"'
`	]'
`	enum ['
`		$5'
`	]'
`}')

dnl W_MUX_SMART(name, format, periods_sink, periods_source, flavour, enum_list)
define(`W_MUX_SMART',
`SectionVendorTuples."'N_MUX($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_FLAVOUR'				STR($5)
`	}'
`}'
`SectionData."'N_MUX($1)`_data_w" {'
`	tuples "'N_MUX($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_MUX($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_MUX($1)`_data_str" {'
`	tuples "'N_MUX($1)`_tuples_str"'
`}'
`SectionWidget."'N_MUX($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "mux"'
`	no_pm "true"'
`	data ['
`		"'N_MUX($1)`_data_w"'
`		"'N_MUX($1)`_data_str"'
`	]'
`	enum ['
`		$6'
`	]'
`}')

divert(0)dnl
