divert(-1)

dnl Define macro for generic detection widget

dnl Detect name)
define(`N_DETECT', `DETECT'PIPELINE_ID`.'$1)

dnl W_DETECT(name, format, periods_sink, periods_source, detect_type, stream_name, kcontrols_list)
define(`W_DETECT',
`SectionVendorTuples."'N_DETECT($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`	}'
`}'
`SectionVendorTuples."'N_DETECT($1)`_detect_process_tuples_str" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	STR(concat($5, _DETECT))
`	}'
`}'
`SectionData."'N_DETECT($1)`_data_w" {'
`	tuples "'N_DETECT($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_DETECT($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_DETECT($1)`_data_str" {'
`	tuples "'N_DETECT($1)`_tuples_str"'
`	tuples "'N_DETECT($1)`_detect_process_tuples_str"'
`}'
`SectionWidget."'N_DETECT($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	stream_name' STR($6)
`	no_pm "true"'
`	event_flags	"15"' # trapping PRE/POST_PMU/PMD events
`	event_type	"1"' # 1 for DAPM event for detect component
`	data ['
`		"'N_DETECT($1)`_data_w"'
`		"'N_DETECT($1)`_data_str"'
`	]'
`       bytes ['
                $7
`       ]'
`}')

divert(0)dnl
