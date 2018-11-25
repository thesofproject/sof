divert(-1)

dnl Define macro for Mixer widget

dnl Mixer Name)
define(`N_MIXER', `MIXER'PIPELINE_ID`.'$1)

dnl Pipe Buffer name in pipeline (pipeline, buffer)
define(`NPIPELINE_MIXER', `MIXER'$1`.'$2)

dnl W_MIXER(name, format, periods_sink, periods_source, preload)
define(`W_MIXER',
`SectionVendorTuples."'N_MIXER($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($5)
`	}'
`}'
`SectionData."'N_MIXER($1)`_data_w" {'
`	tuples "'N_MIXER($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_MIXER($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_MIXER($1)`_data_str" {'
`	tuples "'N_MIXER($1)`_tuples_str"'
`}'
`SectionWidget."'N_MIXER($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "mixer"'
`	no_pm "true"'
`	data ['
`		"'N_MIXER($1)`_data_w"'
`		"'N_MIXER($1)`_data_str"'
`	]'
`}')

divert(0)dnl
