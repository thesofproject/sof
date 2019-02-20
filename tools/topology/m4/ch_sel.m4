divert(-1)

dnl Define macro for channel selector widget

dnl Selector name)
define(`N_SELECTOR', `SELECTOR'PIPELINE_ID`.'$1)

dnl W_SELECTOR(name, format, periods_sink, periods_source, in_chan_cnt, out_chan_cnt, sel_chan_index)
define(`W_SELECTOR',
`SectionVendorTuples."'N_SELECTOR($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`	}'
`}'
`SectionVendorTuples."'N_SELECTOR($1)`_chan_tuples_w" {'
`	tokens "sof_ch_sel_tokens"'
`	tuples."word" {'
`		SOF_TKN_CS_IN_CNT'	STR($5)
`		SOF_TKN_CS_OUT_CNT'	STR($6)
`		SOF_TKN_CS_OUT_CH_INDEX'	STR($7)
`	}'
`}'
`SectionData."'N_SELECTOR($1)`_data_w" {'
`	tuples "'N_SELECTOR($1)`_tuples_w"'
`	tuples "'N_SELECTOR($1)`_chan_tuples_w"'
`}'
`SectionVendorTuples."'N_SELECTOR($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_SELECTOR($1)`_data_str" {'
`	tuples "'N_SELECTOR($1)`_tuples_str"'
`}'
`SectionWidget."'N_SELECTOR($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_SELECTOR($1)`_data_w"'
`		"'N_SELECTOR($1)`_data_str"'
`	]'
`}')

divert(0)dnl
