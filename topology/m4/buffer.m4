divert(-1)

dnl Define the macro for buffer widget

dnl Buffer name)
define(`N_BUFFER', `BUF'PIPELINE_ID`.'$1)

dnl W_BUFFER(name, size, capabilities)
define(`W_BUFFER',
`SectionVendorTuples."'N_BUFFER($1)`_tuples" {'
`	tokens "sof_buffer_tokens"'
`	tuples."word" {'
`		SOF_TKN_BUF_SIZE'	STR($2)
`		SOF_TKN_BUF_CAPS'	STR($3)
`	}'
`}'
`SectionData."'N_BUFFER($1)`_data" {'
`	tuples "'N_BUFFER($1)`_tuples"'
`}'
`SectionWidget."'N_BUFFER($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "buffer"'
`	no_pm "true"'
`	data ['
`		"'N_BUFFER($1)`_data"'
`	]'
`}')

dnl COMP_BUFFER_SIZE( num_periods, sample_size, channels, fmames)
define(`COMP_BUFFER_SIZE', `eval(`$1 * $2 * $3 * $4')')

dnl PCM_PLAYBACK_ADD(name, pipeline, pcm_id, dai_id, playback)
define(`PCM_PLAYBACK_ADD',
`SectionPCM.STR($1) {'
`'
`	index STR($2)'
`'
`	# used for binding to the PCM'
`	id STR($3)'
`'
`	dai.STR($1 $3) {'
`		id STR($4)'
`	}'
`'
`	pcm."playback" {'
`'
`		capabilities STR($5)'
`	}'
`}')

divert(0)dnl
