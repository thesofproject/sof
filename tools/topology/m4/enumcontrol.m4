dnl ENUM_CHANNEL(name, reg, shift)
define(`ENUM_CHANNEL',
`channel.STR($1) {'
`		reg STR($2)'
`		shift STR($3)'
`	}')


dnl CONTROLENUM_OPS(info, comment, get, put)
define(`CONTROLENUM_OPS',
`ops."ctl" {'
`		info STR($1)'
`		#$2'
`		get STR($3)'
`		put STR($4)'
`	}')

dnl CONTROLENUM_LIST(name, enums_list)
define(`CONTROLENUM_LIST',
`SectionText.STR($1) {'
`       values ['
`           $2'
`	    ]'
`}')

dnl C_CONTROLENUM(name, index, ops, ops, enums)
define(`C_CONTROLENUM',
`SectionControlEnum.STR($1) {'
`'
`	# control belongs to this index group'
`	index STR($2)'
`'
`	# enum values as text'
`	texts STR($3)'
`'
`	# channel register and shift for Front Left/Right'
`	$4'
`'
`	# control uses bespoke driver get/put/info ID for io ops'
`	$5'
`}')
