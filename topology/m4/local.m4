divert(-1)

define(`concat',`$1$2')

define(`STR', `"'$1`"')

dnl Argument iterator.
define(`argn', `ifelse(`$1', 1, ``$2'',
       `argn(decr(`$1'), shift(shift($@)))')')

dnl Defines a list of items from a variable number of params.
dnl Use as last argument in a macro.
dnl The first argument specifies the number of tabs to be added for formatting
define(`LIST_LOOP', `argn(j,$@)
$1ifelse(i,`2', `', `define(`i', decr(i))define(`j', incr(j))$0($@)')')

define(`LIST', `pushdef(`i', $#)pushdef(`j', `2')LIST_LOOP($@)popdef(i)popdef(j)')

dnl Sums a list of variable arguments. Use as last argument in macro.
define(`SUM_LOOP', `eval(argn(j,$@)
		ifelse(i,`1', `', `define(`i', decr(i)) define(`j', incr(j)) + $0($@)'))')

dnl Memory capabilities
define(`MEMCAPS', `pushdef(`i', $#) pushdef(`j', `1') SUM_LOOP($@)')

dnl create direct DAPM/pipeline link between 2 widgets)
define(`dapm', `"$1, , $2"')

dnl SRC name)
define(`N_SRC', `SRC'PIPELINE_ID`.'$1)

dnl W_SRC(name, format, periods_sink, periods_source, data, preload)
define(`W_SRC',
`SectionVendorTuples."'N_SRC($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($6)
`	}'
`}'
`SectionData."'N_SRC($1)`_data_w" {'
`	tuples "'N_SRC($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_SRC($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_SRC($1)`_data_str" {'
`	tuples "'N_SRC($1)`_tuples_str"'
`}'
`SectionWidget."'N_SRC($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "src"'
`	no_pm "true"'
`	data ['
`		"'N_SRC($1)`_data_w"'
`		"'N_SRC($1)`_data_str"'
`		"'$5`"'
`	]'
`}')

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

dnl PCM name)
define(`N_PCMP', `PCM'PCM_ID`P')
define(`N_PCMC', `PCM'PCM_ID`C')

dnl W_PCM_PLAYBACK(stream, dmac, dmac_chan, periods_sink, periods_source, preload)
dnl  PCM platform configuration
define(`W_PCM_PLAYBACK',
`SectionVendorTuples."'N_PCMP($1)`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($4)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($5)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($6)
`	}'
`}'
`SectionData."'N_PCMP($1)`_data_w_comp" {'
`	tuples "'N_PCMP($1)`_tuples_w_comp"'
`}'
`SectionVendorTuples."'N_PCMP($1)`_tuples" {'
`	tokens "sof_pcm_tokens"'
`	tuples."word" {'
`		SOF_TKN_PCM_DMAC'	STR($2)
`		SOF_TKN_PCM_DMAC_CHAN'	STR($3)
`	}'
`}'
`SectionData."'N_PCMP($1)`_data" {'
`	tuples "'N_PCMP($1)`_tuples"'
`}'
`SectionWidget."'N_PCMP`" {'
`	index "'PIPELINE_ID`"'
`	type "aif_in"'
`	no_pm "true"'
`	stream_name "'$1`"'
`	data ['
`		"'N_PCMP($1)`_data"'
`		"'N_PCMP($1)`_data_w_comp"'
`	]'
`}')


dnl W_PCM_PLAYBACK(stream, dmac, dmac_chan, periods_sink, periods_source, preload)
define(`W_PCM_CAPTURE',
`SectionVendorTuples."'N_PCMC($1)`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($4)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($5)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($6)
`	}'
`}'
`SectionData."'N_PCMC($1)`_data_w_comp" {'
`	tuples "'N_PCMC($1)`_tuples_w_comp"'
`}'
`SectionVendorTuples."'N_PCMC($1)`_tuples" {'
`	tokens "sof_pcm_tokens"'
`	tuples."word" {'
`		SOF_TKN_PCM_DMAC'	STR($2)
`		SOF_TKN_PCM_DMAC_CHAN'	STR($3)
`	}'
`}'
`SectionData."'N_PCMC($1)`_data" {'
`	tuples "'N_PCMC($1)`_tuples"'
`}'
`SectionWidget."'N_PCMC`" {'
`	index "'PIPELINE_ID`"'
`	type "aif_out"'
`	no_pm "true"'
`	stream_name "'$1`"'
`	data ['
`		"'N_PCMC($1)`_data"'
`		"'N_PCMC($1)`_data_w_comp"'
`	]'
`}')

dnl PGA name)
define(`N_PGA', `PGA'PIPELINE_ID`.'$1)

dnl W_PGA(name, format, periods_sink, periods_source, preload, kcontrol0. kcontrol1...etc)
define(`W_PGA',
`SectionVendorTuples."'N_PGA($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($5)
`	}'
`}'
`SectionData."'N_PGA($1)`_data_w" {'
`	tuples "'N_PGA($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_PGA($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_PGA($1)`_data_str" {'
`	tuples "'N_PGA($1)`_tuples_str"'
`}'
`SectionWidget."'N_PGA($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "pga"'
`	no_pm "true"'
`	data ['
`		"'N_PGA($1)`_data_w"'
`		"'N_PGA($1)`_data_str"'
`	]'
`	mixer ['
		$6
`	]'
`}')

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


dnl Tone name)
define(`N_TONE', `TONE'PIPELINE_ID`.'$1)

dnl W_TONE(name, format, periods_sink, periods_source, preload, kcontrols_list)
define(`W_TONE',
`SectionVendorTuples."'N_TONE($1)`_tuples_w" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($3)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($4)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($5)
`	}'
`}'
`SectionData."'N_TONE($1)`_data_w" {'
`	tuples "'N_TONE($1)`_tuples_w"'
`}'
`SectionVendorTuples."'N_TONE($1)`_tuples_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($2)
`	}'
`}'
`SectionData."'N_TONE($1)`_data_str" {'
`	tuples "'N_TONE($1)`_tuples_str"'
`}'
`SectionWidget."'N_TONE($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "siggen"'
`	no_pm "true"'
`	data ['
`		"'N_TONE($1)`_data_w"'
`		"'N_TONE($1)`_data_str"'
`	]'
`	mixer ['
		$6
`	]'
`}')

dnl DAI name)
define(`N_DAI', DAI_NAME)
define(`N_DAI_OUT', DAI_NAME`.OUT')
define(`N_DAI_IN', DAI_NAME`.IN')

dnl W_DAI_OUT(type, index, format, periods_sink, periods_source, preload, data)
define(`W_DAI_OUT',
`SectionVendorTuples."'N_DAI_OUT($2)`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($4)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($5)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($6)
`	}'
`}'
`SectionData."'N_DAI_OUT($2)`_data_w_comp" {'
`	tuples "'N_DAI_OUT($2)`_tuples_w_comp"'
`}'
`SectionVendorTuples."'N_DAI_OUT($2)`_tuples_w" {'
`	tokens "sof_dai_tokens"'
`	tuples."word" {'
`		SOF_TKN_DAI_INDEX'	$2
`	}'
`}'
`SectionData."'N_DAI_OUT($2)`_data_w" {'
`	tuples "'N_DAI_OUT($2)`_tuples_w"'
`}'
`SectionVendorTuples."'N_DAI_OUT($2)`_tuples_str" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'	$1
`	}'
`}'
`SectionData."'N_DAI_OUT($2)`_data_str" {'
`	tuples "'N_DAI_OUT($2)`_tuples_str"'
`}'
`SectionVendorTuples."'N_DAI_OUT($2)`_tuples_comp_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($3)
`	}'
`}'
`SectionData."'N_DAI_OUT($2)`_data_comp_str" {'
`	tuples "'N_DAI_OUT($2)`_tuples_comp_str"'
`}'
`SectionWidget."'N_DAI_OUT`" {'
`	index "'PIPELINE_ID`"'
`	type "dai_in"'
`	no_pm "true"'
`	data ['
`		"'N_DAI_OUT($2)`_data_w"'
`		"'N_DAI_OUT($2)`_data_w_comp"'
`		"'N_DAI_OUT($2)`_data_str"'
`		"'N_DAI_OUT($2)`_data_comp_str"'
`		"'$7`"'
`	]'
`}')

dnl W_DAI_IN(type, index, format, periods_sink, periods_source, preload, data)
define(`W_DAI_IN',
`SectionVendorTuples."'N_DAI_IN($2)`_tuples_w_comp" {'
`	tokens "sof_comp_tokens"'
`	tuples."word" {'
`		SOF_TKN_COMP_PERIOD_SINK_COUNT'		STR($4)
`		SOF_TKN_COMP_PERIOD_SOURCE_COUNT'	STR($5)
`		SOF_TKN_COMP_PRELOAD_COUNT'		STR($6)
`	}'
`}'
`SectionData."'N_DAI_IN($2)`_data_w_comp" {'
`	tuples "'N_DAI_IN($2)`_tuples_w_comp"'
`}'
`SectionVendorTuples."'N_DAI_IN($2)`_tuples_w" {'
`	tokens "sof_dai_tokens"'
`	tuples."word" {'
`		SOF_TKN_DAI_INDEX'	$2
`	}'
`}'
`SectionData."'N_DAI_IN($2)`_data_w" {'
`	tuples "'N_DAI_IN($2)`_tuples_w"'
`}'
`SectionVendorTuples."'N_DAI_IN($2)`_tuples_str" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'	$1
`	}'
`}'
`SectionData."'N_DAI_IN($2)`_data_str" {'
`	tuples "'N_DAI_IN($2)`_tuples_str"'
`}'
`SectionVendorTuples."'N_DAI_IN($2)`_tuples_comp_str" {'
`	tokens "sof_comp_tokens"'
`	tuples."string" {'
`		SOF_TKN_COMP_FORMAT'	STR($3)
`	}'
`}'
`SectionData."'N_DAI_IN($2)`_data_comp_str" {'
`	tuples "'N_DAI_IN($2)`_tuples_comp_str"'
`}'
`SectionWidget."'N_DAI_IN`" {'
`	index "'PIPELINE_ID`"'
`	type "dai_out"'
`	no_pm "true"'
`	data ['
`		"'N_DAI_IN($2)`_data_w"'
`		"'N_DAI_IN($2)`_data_w_comp"'
`		"'N_DAI_IN($2)`_data_str"'
`		"'N_DAI_IN($2)`_data_comp_str"'
`		"'$7`"'
`	]'
`}')

dnl Pipe Buffer name in pipeline (pipeline, buffer)
define(`NPIPELINE_BUFFER', `BUF'$1`.'$2)

dnl Pipeline name)
define(`N_PIPELINE', `PIPELINE.'PIPELINE_ID`.'$1)

dnl W_PIPELINE(stream, deadline, priority, frames, core, timer, platform)
define(`W_PIPELINE',
`SectionVendorTuples."'N_PIPELINE($1)`_tuples" {'
`	tokens "sof_sched_tokens"'
`	tuples."word" {'
`		SOF_TKN_SCHED_DEADLINE'		STR($2)
`		SOF_TKN_SCHED_PRIORITY'		STR($3)
`		SOF_TKN_SCHED_CORE'		STR($5)
`		SOF_TKN_SCHED_FRAMES'		STR($4)
`		SOF_TKN_SCHED_TIMER'		STR($6)
`	}'
`}'
`SectionData."'N_PIPELINE($1)`_data" {'
`	tuples "'N_PIPELINE($1)`_tuples"'
`}'
`SectionWidget."'N_PIPELINE($1)`" {'
`	index "'PIPELINE_ID`"'
`	type "scheduler"'
`	no_pm "true"'
`	stream_name "'$1`"'
`	data ['
`		"'N_PIPELINE($1)`_data"'
`		"'$7`"'
`	]'
`}')

dnl D_DAI(id, playback, capture, data))
define(`D_DAI', `SectionDAI."'N_DAI`" {'
`	index "'PIPELINE_ID`"'
`	id "'$1`"'
`	playback "'$2`"'
`	capture "'$3`"'
`}')

dnl DAI_CLOCK(clock, freq, codec_master)
define(`DAI_CLOCK',
	$1		STR($3)
	$1_freq	STR($2))


dnl DAI_TDM(slots, width, tx_mask, rx_mask)
define(`DAI_TDM',
`	tdm_slots	'STR($1)
`	tdm_slot_width	'STR($2)
`	tx_slots	'STR($3)
`	rx_slots	'STR($4)
)

dnl Pipeline name)
define(`N_DAI_CONFIG', `DAICONFIG.'$1)

dnl DAI_CONFIG(type, idx, name, format, valid bits, mclk, bclk, fsync, tdm)
define(`DAI_CONFIG',
`SectionHWConfig."'$1$2`" {'
`'
`	id		"'$2`"'
`	format		"'$4`"'
`'
`	'$6
`	'$7
`	'$8
`	'$9
`}'
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples_str" {'
`	tokens "sof_dai_tokens"'
`	tuples."string" {'
`		SOF_TKN_DAI_TYPE'		STR($1)
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data_str" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples_str"'
`}'
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_tuples" {'
`	tokens "sof_dai_tokens"'
`	tuples."word" {'
`		SOF_TKN_DAI_SAMPLE_BITS'	STR($5)
`	}'
`}'
`SectionData."'N_DAI_CONFIG($1$2)`_data" {'
`	tuples "'N_DAI_CONFIG($1$2)`_tuples"'
`}'
`'
`SectionBE."'$3`" {'
`	index "0"'
`	default_hw_conf_id	"'$2`"'
`'
`	hw_configs ['
`		"'$1$2`"'
`	]'
`	data ['
`		"'N_DAI_CONFIG($1$2)`_data"'
`		"'N_DAI_CONFIG($1$2)`_data_str"'
`	]'
`}')

dnl COMP_SAMPLE_SIZE(FMT)
define(`COMP_SAMPLE_SIZE',
`ifelse(
	$1, `s16le', `2',
	$1, `s24_4le', `4',
	$1, `s32le', `4',
	$1, `float', `4',
	`4')')


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

dnl PCM_CAPTURE_ADD(name, pipeline, pcm_id, dai_id, capture)
define(`PCM_CAPTURE_ADD',
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
`	pcm."capture" {'
`'
`		capabilities STR($5)'
`	}'
`}')

dnl PCM_DUPLEX_ADD(name, pipeline, pcm_id, dai_id, playback, capture)
define(`PCM_DUPLEX_ADD',
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
`	pcm."capture" {'
`'
`		capabilities STR($6)'
`	}'
`'
`	pcm."playback" {'
`'
`		capabilities STR($5)'
`	}'
`}')

dnl KCONTROL_CHANNEL(name, reg, shift)
define(`KCONTROL_CHANNEL',
`channel.STR($1) {'
`		reg STR($2)'
`		shift STR($3)'
`	}')

dnl CONTROLMIXER_MAX(comment, value)
define(`CONTROLMIXER_MAX',
`#$1'
`	max STR($2)')

dnl CONTROLMIXER_TLV(comment, value)
define(`CONTROLMIXER_TLV',
`#$1'
`	tlv STR($2)')

dnl CONTROLMIXER_OPS(info, comment, get, put)
define(`CONTROLMIXER_OPS',
`ops."ctl" {'
`		info STR($1)'
`		#$2'
`		get STR($3)'
`		put STR($4)'
`	}')

dnl C_CONTROLMIXER(name, index, ops, max, invert, tlv, KCONTROL_CHANNELS)
define(`C_CONTROLMIXER',
`SectionControlMixer.STR($1) {'
`'
`	# control belongs to this index group'
`	index STR($2)'
`'
`	#$7'
`	$8'
`	# control uses bespoke driver get/put/info ID'
`	$3'
`'
`	$4'
`	invert STR($5)'
`	$6'
`}')

dnl P_GRAPH(name, CONNECTIONS)
define(`P_GRAPH',
`SectionGraph.STR($1) {'
`	index STR($2)'
`'
`	lines ['
`		$3'
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

dnl W_VENDORTUPLES(name, tokens, RATE_OUT)
define(`W_VENDORTUPLES',
`SectionVendorTuples.STR($1) {'
`	tokens STR($2)'
`'
`	tuples."word" {'
`		$3'
`	}'
`}')

divert(0) dnl


