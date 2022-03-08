divert(-1)

dnl Define macro for RTC Processing effect widget
DECLARE_SOF_RT_UUID("google-rtc-audio-processing", google_rtc_audio_processing_uuid,
0xb780a0a6, 0x269f, 0x466f, 0xb4, 0x77, 0x23, 0xdf, 0xa0, 0x5a, 0xf7, 0x58)

dnl N_GOOGLE_RTC_AUDIO_PROCESSING(name)
define(`N_GOOGLE_RTC_AUDIO_PROCESSING', `GOOGLE_RTC_AUDIO_PROCESSING'PIPELINE_ID`.'$1)

dnl W_GOOGLE_RTC_AUDIO_PROCESSING(name, format, periods_sink, periods_source, core, mixer_kcontrols_list, byte_kcontrols_list)
define(`W_GOOGLE_RTC_AUDIO_PROCESSING',
`SectionVendorTuples."'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(google_rtc_audio_processing_uuid)
`	}'
`}'
`SectionData."'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_data_uuid" {'
`	tuples "'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_data_w" {'
`	tuples "'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_data_str" {'
`	tuples "'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_tuples_str"'
`}'
`SectionVendorTuples."'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_tuples_str_type" {'
`	tokens "sof_process_tokens"'
`	tuples."string" {'
`		SOF_TKN_PROCESS_TYPE'	"GRTCP"
`	}'
`}'
`SectionData."'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_data_str_type" {'
`	tuples "'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_tuples_str_type"'
`}'
`SectionWidget."'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "effect"'
`	no_pm "true"'
`	data ['
`		"'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_data_uuid"'
`		"'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_data_w"'
`		"'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_data_str"'
`		"'N_GOOGLE_RTC_AUDIO_PROCESSING($1)`_data_str_type"'
`	]'
`	mixer ['
		$6
`	]'
`	bytes ['
		$7
`	]'
`}')

divert(0)dnl
