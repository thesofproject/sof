divert(-1)

dnl Defines the macro for SRC widget

dnl SRC name)
define(`N_SRC', `SRC'PIPELINE_ID`.'$1)

dnl W_SRC(name, format, periods_sink, periods_source, data, preload)
define(`W_SRC',
`SectionVendorTuples."'N_SRC($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($6)
`	}'
`}'
`SectionData."'N_SRC($1)`_data_w" {'
`	tuples "'N_SRC($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_SRC($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_SRC($1)`_data_str" {'
`	tuples "'N_SRC($1)`_tuples_str"'
`}'
`SectionWidget."'N_SRC($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "src"'
`	no_pm "true"'
`	data ['
`		"'N_SRC($1)`_data_w"'
`		"'N_SRC($1)`_data_str"'
`		"'$5`"'
`	]'
`}')

divert(0)dnl
