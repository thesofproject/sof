divert(-1)

dnl Define macro for Eq effect widget

dnl EQ name)
define(`N_EQ', `EQ'PIPELINE_ID`.'$1)

dnl W_EQ(name, format, periods_sink, periods_source, preload, kcontrols_list)
define(`W_EQ',
`SectionVendorTuples."'N_EQ($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($5)
`	}'
`}'
`SectionData."'N_EQ($1)`_data_w" {'
`	tuples "'N_EQ($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_EQ($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_EQ($1)`_data_str" {'
`	tuples "'N_EQ($1)`_tuples_str"'
`}'
`SectionWidget."'N_EQ($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_EQ($1)`_data_w"'
`		"'N_EQ($1)`_data_str"'
`	]'
`	bytes ['
		$6
`	]'
`}')

divert(0)dnl
