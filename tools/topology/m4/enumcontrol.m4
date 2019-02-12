divert(-1)

dnl Define macro for mux control

dnl KCONTROL_ENUM_CHANNEL(name, reg, shift)
define(`KCONTROL_ENUM_CHANNEL',
`channel.STR($1) {'
`		reg STR($2)'
`		shift STR($3)'
`	}')

dnl C_CONTROLENUM_TEXT(name, list of values)
define(`C_CONTROLENUM_TEXT',
`SectionText."$1" {'
`	values ['
`		$2'
`	]'
`}')


dnl CONTROLENUM_OPS(info, comment, get, put)
define(`CONTROLENUM_OPS',
`ops."ctl" {'
`		info STR($1)'
`		#$2'
`		get STR($3)'
`		put STR($4)'
`	}')

dnl C_CONTROLENUM(name, index, ops, list of KCONTROL_ENUM_CHANNEL, enum text section name)
define(`C_CONTROLENUM',
`SectionControlEnum."$1 PIPELINE_ID" {'
`'
`	# control belongs to this index group'
`	index STR($2)'
`'
`	$4'
`	# control uses bespoke driver get/put/info ID'
`	$3'
`'
`	text ['
`	 STR($5)'
`	]'
`}')

divert(0)dnl
