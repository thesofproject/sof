divert(-1)

dnl Define macro for mux control

dnl KCONTROL_MUX_ENUM(name, reg, shift)
define(`KCONTROL_MUX_ENUM',
`channel.STR($1) {'
`		reg STR($2)'
`		shift STR($3)'
`	}')

dnl CONTROLMIXER_ITEMS(comment, value)
define(`CONTROLMIXER_ITEMS',
`#$1'
`	max STR($2)')


dnl CONTROLMUX_OPS(info, comment, get, put)
define(`CONTROLMUX_OPS',
`ops."ctl" {'
`		info STR($1)'
`		#$2'
`		get STR($3)'
`		put STR($4)'
`	}')

dnl C_CONTROLMIXER(name, index, ops, items, KCONTROL_MUX_ENUM, KCONTROL_MUX_ITEM)
define(`C_CONTROLMUX',
`SectionControlEnum."$1 PIPELINE_ID" {'
`'
`	# control belongs to this index group'
`	index STR($2)'
`'
`	$5'
`	# control uses bespoke driver get/put/info ID'
`	$3'
`'
`	$4'
`	texts ['
`		$6'
`	]'

`}')

divert(0)dnl
