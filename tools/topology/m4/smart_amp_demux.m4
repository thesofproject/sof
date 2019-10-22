divert(-1)

dnl Define macro for smart amplifier demux component widget

dnl smartamp demux Name)
define(`N_SMART_AMP_DEMUX', `SMART_AMP_DEMUX'PIPELINE_ID`.'$1)

dnl W_SMART_AMP_DEMUX(name, format, periods_sink, periods_source)
define(`W_SMART_AMP_DEMUX',
`SectionVendorTuples."'N_SMART_AMP_DEMUX($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`	}'
`}'
`SectionData."'N_SMART_AMP_DEMUX($1)`_data_w" {'
`	tuples "'N_SMART_AMP_DEMUX($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_SMART_AMP_DEMUX($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionVendorTuples."'N_SMART_AMP_DEMUX($1)`_process_tuples_str" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"SMART_AMP_DEMUX"
`	}'
`}'
`SectionData."'N_SMART_AMP_DEMUX($1)`_data_str" {'
`	tuples "'N_SMART_AMP_DEMUX($1)`_tuples_str"'
`	tuples "'N_SMART_AMP_DEMUX($1)`_process_tuples_str"'
`}'
`SectionWidget."'N_SMART_AMP_DEMUX($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_SMART_AMP_DEMUX($1)`_data_w"'
`		"'N_SMART_AMP_DEMUX($1)`_data_str"'
`	]'
`}')

divert(0)dnl
