divert(-1)

dnl Define the macro for PCM playback/capture/capabilities
DECLARE_SOF_RT_UUID("host", host_uuid, 0x8b9d100c, 0x6d78, 0x418f,
		 0x90, 0xa3, 0xe0, 0xe8, 0x05, 0xd0, 0x85, 0x2b)

dnl N_PCM(name)
define(`N_PCMP', `PCM'$1`P')
define(`N_PCMC', `PCM'$1`C')

dnl W_PCM_PLAYBACK(pcm, stream, periods_sink, periods_source, core)
dnl  PCM platform configuration
define(`W_PCM_PLAYBACK',
`SectionVendorTuples."'N_PCMP($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(host_uuid)
`	}'
`}'
`SectionData."'N_PCMP($1)`_data_uuid" {'
`	tuples "'N_PCMP($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_PCMP($1)`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_PCMP($1)`_data_w_comp" {'
`	tuples "'N_PCMP($1)`_tuples_w_comp"'
`}'
`SectionWidget."'N_PCMP($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "aif_in"'
`	no_pm "true"'
`	stream_name "'$2` '$1`"'
`	data ['
`		"'N_PCMP($1)`_data_uuid"'
`		"'N_PCMP($1)`_data_w_comp"'
`	]'
`}')


dnl W_PCM_CAPTURE(pcm, stream, periods_sink, periods_source, core)
define(`W_PCM_CAPTURE',
`SectionVendorTuples."'N_PCMC($1)`_tuples_uuid" {'
`	tokens "sof_comp_tokens"'
`	tuples."uuid" {'
`		SOF_TKN_COMP_UUID'		STR(host_uuid)
`	}'
`}'
`SectionData."'N_PCMC($1)`_data_uuid" {'
`	tuples "'N_PCMC($1)`_tuples_uuid"'
`}'
`SectionVendorTuples."'N_PCMC($1)`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_CORE_ID'			STR($5)
`	}'
`}'
`SectionData."'N_PCMC($1)`_data_w_comp" {'
`	tuples "'N_PCMC($1)`_tuples_w_comp"'
`}'
`SectionWidget."'N_PCMC($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "aif_out"'
`	no_pm "true"'
`	stream_name "'$2` '$1`"'
`	data ['
`		"'N_PCMC($1)`_data_uuid"'
`		"'N_PCMC($1)`_data_w_comp"'
`	]'
`}')

dnl PCM_CAPABILITIES(name, formats, rate_min, rate_max, channels_min, channels_max, periods_min, periods_max, period_size_min, period_size_max, buffer_size_min, buffer_size_max)
define(`PCM_CAPABILITIES',
`SectionPCMCapabilities.STR($1) {'
`'
`	formats "$2"'
`	rate_min STR($3)'
`	rate_max STR($4)'
`	channels_min STR($5)'
`	channels_max STR($6)'
`	periods_min STR($7)'
`	periods_max STR($8)'
`	period_size_min	STR($9)'
`	period_size_max	STR($10)'
`	buffer_size_min	STR($11)'
`	buffer_size_max	STR($12)'
`}')

dnl PCM_PLAYBACK_ADD(name, pcm_id, playback)
define(`PCM_PLAYBACK_ADD',
`ifelse(`$#', `3',
PCM_PLAYBACK_ADD_COMMON($1, $2, $3, false),
`fatal_error(`Invalid parameters ($#) to PCM_PLAYBACK_ADD')')'
)

dnl COMPR_PLAYBACK_ADD(name, pcm_id, playback)
define(`COMPR_PLAYBACK_ADD',
`ifelse(`$#', `3',
PCM_PLAYBACK_ADD_COMMON($1, $2, $3, true),
`fatal_error(`Invalid parameters ($#) to PCM_PLAYBACK_COMPR_ADD')')'
)

dnl PCM_PLAYBACK_ADD_COMMON(name, pcm_id, playback, compress)
define(`PCM_PLAYBACK_ADD_COMMON',
`ifelse(`$#', `4',
`SectionPCM.STR($1) {'
`'
`	# used for binding to the PCM'
`	id STR($2)'
`'
`	dai.STR($1 $2) {'
`		id STR($2)'
`	}'
`'
`	pcm."playback" {'
`'
`		capabilities STR($3)'
`	}'
`'
`	compress STR($4)'
`}', `fatal_error(`Invalid parameters ($#) to PCM_PLAYBACK_ADD_COMMON')')'
)

dnl PCM_CAPTURE_ADD_COMMON(name, pcm_id, capture, lp, compress)
define(`PCM_CAPTURE_ADD_COMMON',
`ifelse(`$4', `1',
`SectionVendorTuples."$1_tuples_w" {'
`       tokens "sof_stream_tokens"'
`       tuples."bool" {'
`               SOF_TKN_STREAM_CAPTURE_COMPATIBLE_D0I3        "true"'
`       }'
`}'
`'
`SectionData."$1_data_w" {'
`       tuples "$1_tuples_w"'
`}', `')'
`'
`SectionPCM.STR($1) {'
`'
`	# used for binding to the PCM'
`	id STR($2)'
`'
`	dai.STR($1 $2) {'
`		id STR($2)'
`	}'
`'
`	pcm."capture" {'
`'
`		capabilities STR($3)'
`	}'
`'
`	compress STR($5)'
`ifelse(`$4', `1',
`       data ['
`               "$1_data_w"'
`       ]', `')'
`}'
)

dnl PCM_CAPTURE_ADD(name, pcm_id, capture)
define(`PCM_CAPTURE_ADD',
`ifelse(`$#', `3',
PCM_CAPTURE_ADD_COMMON($1, $2, $3, 0, false),
`fatal_error(`Invalid parameters ($#) to PCM_CAPTURE_ADD')')'
)

dnl COMPR_CAPTURE_ADD(name, pcm_id, capture)
define(`COMPR_CAPTURE_ADD',
`ifelse(`$#', `3',
PCM_CAPTURE_ADD_COMMON($1, $2, $3, 0, true),
`fatal_error(`Invalid parameters ($#) to COMPR_CAPTURE_ADD')')'
)

dnl PCM_CAPTURE_LP_ADD(name, pcm_id, capture)
define(`PCM_CAPTURE_LP_ADD',
`ifelse(`$#', `3',
PCM_CAPTURE_ADD_COMMON($1, $2, $3, 1, false),
`fatal_error(`Invalid parameters ($#) to PCM_CAPTURE_LP_ADD')')'
)

dnl PCM_DUPLEX_ADD(name, pcm_id, playback, capture)
define(`PCM_DUPLEX_ADD',
`ifelse(`$#', `4',
`SectionPCM.STR($1) {'
`'
`	# used for binding to the PCM'
`	id STR($2)'
`'
`	dai.STR($1 $2) {'
`		id STR($2)'
`	}'
`'
`	pcm."capture" {'
`'
`		capabilities STR($4)'
`	}'
`'
`	pcm."playback" {'
`'
`		capabilities STR($3)'
`	}'
`}', `fatal_error(`Invalid parameters ($#) to PCM_DUPLEX_ADD')')'
)

divert(0)dnl
