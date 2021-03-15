divert(-1)

dnl Define macro for Google hotword detection widget
DECLARE_SOF_RT_UUID("google-hotword-detect", ghd_uuid, 0xc3c74249, 0x058e, 0x414f,
		 0x82, 0x40, 0x4d, 0xa5, 0xf3, 0xfc, 0x23, 0x89)

dnl N_GHD(name)
define(`N_GHD', `GHD'PIPELINE_ID`.'$1)

dnl W_GHD(name, format, periods_sink, periods_source, detect_type, stream_name, core, kcontrols_list)
define(`W_GHD',
`SectionVendorTuples."'N_GHD($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(ghd_uuid)
`	}'
`}'
`SectionData."'N_GHD($1)`_data_uuid" {'
`	tuples "'N_GHD($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_GHD($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($7)
`	}'
`}'
`SectionVendorTuples."'N_GHD($1)`_detect_process_tuples_str" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	STR(concat($5, _DETECT)) # UUID overrides the type matching. See get_drv.
`	}'
`}'
`SectionData."'N_GHD($1)`_data_w" {'
`	tuples "'N_GHD($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_GHD($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_GHD($1)`_data_str" {'
`	tuples "'N_GHD($1)`_tuples_str"'
`	tuples "'N_GHD($1)`_detect_process_tuples_str"'
`}'
`SectionWidget."'N_GHD($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	stream_name' STR($6)
`	no_pm "true"'
`	event_flags	"15"' # trapping PRE/POST_PMU/PMD events
`	event_type	"1"' # 1 for DAPM event for detect component
`	data ['
`		"'N_GHD($1)`_data_uuid"'
`		"'N_GHD($1)`_data_w"'
`		"'N_GHD($1)`_data_str"'
`	]'
`	bytes ['
		$8
`	]'
`}')

divert(0)dnl
