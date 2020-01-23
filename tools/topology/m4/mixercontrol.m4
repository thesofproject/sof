divert(-1)

dnl Define macro for mixer control

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

dnl N_CONTROLMIXER(name)
define(`N_CONTROLMIXER', PIPELINE_ID $1)

dnl C_CONTROLMIXER(name, ops, max, invert, tlv, comment, KCONTROL_CHANNELS, useleds, ledsdir)
define(`C_CONTROLMIXER',
`ifelse(`$#', `9',
`SectionVendorTuples."'$1`_tuples_w" {'
`	tokens "sof_led_tokens"'
`	tuples."word" {'
`		SOF_TKN_MUTE_LED_USE'		$8
`		SOF_TKN_MUTE_LED_DIRECTION'	$9
`	}'
`}'
`SectionData."'$1`_data_w" {'
`	tuples "'$1`_tuples_w"'
`}'
,` ')'
`SectionControlMixer."'$1`" {'
`'
`	# control belongs to this index group'
`	index "PIPELINE_ID"'
`'
`	#$6'
`	$7'
`	# control uses bespoke driver get/put/info ID'
`	$2'
`'
`	$3'
`	invert STR($4)'
`	$5'
`ifelse(`$#', `9',
`	data ['
`		"'$1`_data_w"'
`	]'
,` ')'
`}')

divert(0)dnl
