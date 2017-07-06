define(`concat',`$1$2')

define(`STR', `"'$1`"')

#create direct DAPM/pipeline link between 2 widgets)
define(`dapm', `"$1, , $2"')

#SRC name)
define(`N_SRC', `SRC'PIPELINE_ID`.'$1)

#W_SRC(name, data))
define(`W_SRC', `SectionWidget."'N_SRC($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "src"'
`	no_pm "true"'
`	data ['
`		"'$2`"'
`	]'
`}')

#Buffer name)
define(`N_BUFFER', `BUF'PIPELINE_ID`.'$1)

#W_BUFFER(name, size))
define(`W_BUFFER',
`SectionVendorTuples."'N_BUFFER($1)`_tuples" {'
`	tokens "sof_buffer_tokens"'
`	tuples."word" {'
`		SOF_TKN_BUF_SIZE'	$2
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

#PCM name)
define(`N_PCM', `PCM'PCM_ID)

#W_PCM_PLAYBACK(stream))
define(`W_PCM_PLAYBACK', `SectionWidget."'N_PCM`" {'
`	index "'PIPELINE_ID`"'
`	type "aif_out"'
`	no_pm "true"'
`	stream_name "'$1`"'
`}')


#W_PCM_CAPTURE(stream))
define(`W_PCM_CAPTURE', `SectionWidget."'N_PCM`" {'
`	index "'PIPELINE_ID`"'
`	type "aif_out"'
`	no_pm "true"'
`	stream_name "'$1`"'
`}')

#PGA name)
define(`N_PGA', `PGA'PIPELINE_ID`.'$1)

#W_PGA(name))
define(`W_PGA', `SectionWidget."'N_PGA($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "pga"'
`	no_pm "true"'
`}')

#Mixer Name)
define(`N_MIXER', `MIXER'PIPELINE_ID`.'$1)

#Pipe Buffer name in pipeline (pipeline, buffer)
define(`NPIPELINE_MIXER', `MIXER'$1`.'$2)

#W_MIXER(name))
define(`W_MIXER', `SectionWidget."'N_MIXER($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "mixer"'
`	no_pm "true"'
`}')


#Tone name)
define(`N_TONE', `TONE'PIPELINE_ID`.'$1)

#W_TONE(name))
define(`W_TONE', `SectionWidget."'N_TONE($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "siggen"'
`	no_pm "true"'
`}')

#DAI name)
define(`N_DAI', DAI_NAME)
define(`N_DAI_OUT', DAI_NAME`.OUT')
define(`N_DAI_IN', DAI_NAME`.IN')

#W_DAI_OUT(stream, data))
define(`W_DAI_OUT', `SectionWidget."'N_DAI_OUT`" {'
`	index "'PIPELINE_ID`"'
`	type "dai_out"'
`	no_pm "true"'
`	stream_name "'$1`"'
`	data ['
`		"'$2`"'
`	]'
`}')

#W_DAI_IN(stream, data))
define(`W_DAI_IN', `SectionWidget."'N_DAI_IN`" {'
`	index "'PIPELINE_ID`"'
`	type "dai_in"'
`	no_pm "true"'
`	stream_name "'$1`"'
`	data ['
`		"'$2`"'
`	]'
`}')

#Pipe Buffer name in pipeline (pipeline, buffer)
define(`NPIPELINE_BUFFER', `BUF'$1`.'$2)

#Pipeline name)
define(`N_PIPELINE', `PIPELINE.'PIPELINE_ID)

#W_PIPELINE(stream, deadline, platform))
define(`W_PIPELINE',
`SectionVendorTuples."'N_PIPELINE`_tuples" {'
`	tokens "sof_sched_tokens"'
`	tuples."word" {'
`		SOF_TKN_SCHED_DEADLINE'		$2
`	}'
`}'
`SectionData."'N_PIPELINE`_data" {'
`	tuples "'N_PIPELINE`_tuples"'
`}'
`SectionWidget."'N_PIPELINE`" {'
`	index "'PIPELINE_ID`"'
`	type "scheduler"'
`	no_pm "true"'
`	stream_name "'$1`"'
`	data ['
`		"'N_PIPELINE`_data"'
`		"'$3`"'
`	]'
`}')

#D_DAI(id, playback, capture, data))
define(`D_DAI', `SectionDAI."'N_DAI`" {'
`	index "'PIPELINE_ID`"'
`	id "'$1`"'
`	playback "'$2`"'
`	capture "'$3`"'
`}')
