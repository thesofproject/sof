divert(-1)

dnl Define macro for byte control

dnl CONTROLBYTES_MAX(comment, value)
define(`CONTROLBYTES_MAX',
`#$1'
`	max STR($2)')

dnl CONTROLBYTES_TLV(comment, value)
define(`CONTROLBYTES_TLV',
`#$1'
`	tlv STR($2)')

dnl CONTROLBYTES_OPS(info, comment, get, put)
define(`CONTROLBYTES_OPS',
`ops."ctl" {'
`		info STR($1)'
`		#$2'
`	}')

dnl CONTROLBYTES_OPS(info, comment, get, put)
define(`CONTROLBYTES_EXTOPS',
`extops."extctl" {'
`		#$1'
`		get STR($2)'
`		put STR($3)'
`	}')

# Readonly byte control to read the actual value from DSP
dnl CONTROLBYTES_EXTOPS_READONLY(info, comment, get)
define(`CONTROLBYTES_EXTOPS_READONLY',
`extops."extctl" {'
`		#$1'
`		get STR($2)'
`	}')

# Writeonly byte control
dnl CONTROLBYTES_EXTOPS_WRITEONLY(info, comment, put)
define(`CONTROLBYTES_EXTOPS_WRITEONLY',
`extops."extctl" {'
`		#$1'
`		put STR($2)'
`	}')

define(`CONTROLBYTES_PRIV',
`SectionData.STR($1) {'
`	$2'
`}')

dnl C_CONTROLBYTES(name, index, ops, base, num_regs, mask, max, tlv, priv)
define(`C_CONTROLBYTES',
`SectionControlBytes.STR($1) {'
`'
`	# control belongs to this index group'
`	index STR($2)'
`'
`	# control uses bespoke driver get/put/info ID for io ops'
`	$3'
`	# control uses bespoke driver get/put/info ID for ext ops'
`	$4'
`'
`	base STR($5)'
`	num_regs STR($6)'
`	mask STR($7)'
`	$8'
`	$9'
`	access ['
`		tlv_write'
`		tlv_read'
`		tlv_callback'
`	]'
`	data ['
`		$10'
`	]'
`}')

# Readonly byte control to read the actual value from DSP
dnl C_CONTROLBYTES_VOLATILE_READONLY(name, index, ops, base, num_regs, mask, max, tlv, priv)
define(`C_CONTROLBYTES_VOLATILE_READONLY',
`SectionControlBytes.STR($1) {'
`'
`	# control belongs to this index group'
`	index STR($2)'
`'
`	# control uses bespoke driver get/put/info ID for io ops'
`	$3'
`	# control uses bespoke driver get/put/info ID for ext ops'
`	$4'
`'
`	base STR($5)'
`	num_regs STR($6)'
`	mask STR($7)'
`	$8'
`	$9'
`	access ['
`		tlv_read'
`		tlv_callback'
`		volatile'
`	]'
`	data ['
`		$10'
`	]'
`}')

# Writeonly byte control
dnl C_CONTROLBYTES_WRITEONLY(name, index, ops, base, num_regs, mask, max, tlv, priv)
define(`C_CONTROLBYTES_WRITEONLY',
`SectionControlBytes.STR($1) {'
`'
`	# control belongs to this index group'
`	index STR($2)'
`'
`	# control uses bespoke driver get/put/info ID for io ops'
`	$3'
`	# control uses bespoke driver get/put/info ID for ext ops'
`	$4'
`'
`	base STR($5)'
`	num_regs STR($6)'
`	mask STR($7)'
`	$8'
`	$9'
`	access ['
`		tlv_write'
`		tlv_callback'
`	]'
`	data ['
`		$10'
`	]'
`}')

# Read-write volatile byte control
dnl C_CONTROLBYTES_VOLATILE_RW(name, index, ops, base, num_regs, mask, max, tlv, priv)
define(`C_CONTROLBYTES_VOLATILE_RW',
`SectionControlBytes.STR($1) {'
`'
`	# control belongs to this index group'
`	index STR($2)'
`'
`	# control uses bespoke driver get/put/info ID for io ops'
`	$3'
`	# control uses bespoke driver get/put/info ID for ext ops'
`	$4'
`'
`	base STR($5)'
`	num_regs STR($6)'
`	mask STR($7)'
`	$8'
`	$9'
`	access ['
`		tlv_read'
`		tlv_write'
`		tlv_callback'
`		volatile'
`	]'
`	data ['
`		$10'
`	]'
`}')

divert(0)dnl
