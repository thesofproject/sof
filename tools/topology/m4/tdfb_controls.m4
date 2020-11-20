# TDFB Bytes control with max value of 255
C_CONTROLBYTES(DEF_TDFB_BYTES, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(, 4096),
	,
	DEF_TDFB_PRIV)

# TDFB enum list
CONTROLENUM_LIST(DEF_TDFB_ONOFF, LIST(`	', `"beam on"', `"beam off"'))

# TDFB enum control
C_CONTROLENUM(DEF_TDFB_ONOFF_ENUM, PIPELINE_ID,
	DEF_TDFB_ONOFF,
	LIST(`	', ENUM_CHANNEL(FC, 3, 0)),
	CONTROLENUM_OPS(enum,
		257 binds the mixer control to enum get/put handlers,
		257, 257))

# TDFB enum list
CONTROLENUM_LIST(DEF_TDFB_BEAMN, LIST(`	', `"0deg"', `"30deg"', `"60deg"', `"90deg"', `"120deg"', `"150deg"', `"180deg"', `"210deg"', `"240deg"', `"270deg"', `"300deg"', `"330deg"'))

# TDFB enum control
C_CONTROLENUM(DEF_TDFB_DIRECTION_ENUM, PIPELINE_ID,
	DEF_TDFB_BEAMN,
	LIST(`	', ENUM_CHANNEL(FC, 3, 0)),
	CONTROLENUM_OPS(enum,
		257 binds the mixer control to enum get/put handlers,
		257, 257))
