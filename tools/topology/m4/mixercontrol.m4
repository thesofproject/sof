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

dnl C_CONTROLMIXER(name, index, ops, max, invert, tlv, KCONTROL_CHANNELS)
define(`C_CONTROLMIXER',
`SectionControlMixer."$2 $1" {'
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

divert(0)dnl
