divert(-1)

dnl Define macro for example Amp widget

dnl AMP name)
define(`N_AMP', `AMP'PIPELINE_ID`.'$1)

dnl W_AMP(name, format, periods_sink, periods_source, kcontrols_list)
define(`W_AMP',
`SectionVendorTuples."'N_AMP($1)`_tuples_w" {'
`    tokens "sof_comp_tokens"'
`    tuples."word" {'
`            SOF_TKN_COMP_PERIOD_SINK_COUNT'         STR($3)
`            SOF_TKN_COMP_PERIOD_SOURCE_COUNT'       STR($4)
`    }'
`}'
`SectionData."'N_AMP($1)`_data_w" {'
`    tuples "'N_AMP($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_AMP($1)`_tuples_str" {'
`    tokens "sof_comp_tokens"'
`    tuples."string" {'
`            SOF_TKN_COMP_FORMAT'    STR($2)
`    }'
`}'
`SectionData."'N_AMP($1)`_data_str" {'
`    tuples "'N_AMP($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_AMP($1)`_tuples_str_type" {'
`    tokens "sof_process_tokens"'
`    tuples."string" {'
`            SOF_TKN_PROCESS_TYPE'   "AMP"
`    }'
`}'
`SectionData."'N_AMP($1)`_data_str_type" {'
`    tuples "'N_AMP($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_AMP($1)`" {'
`    index "'PIPELINE_ID`"'
`    type "effect"'
`    no_pm "true"'
`    data ['
`            "'N_AMP($1)`_data_w"'
`            "'N_AMP($1)`_data_str"'
`            "'N_AMP($1)`_data_str_type"'
`    ]'
`    bytes ['
             $5
`    ]'
`}')

divert(0)dnl
