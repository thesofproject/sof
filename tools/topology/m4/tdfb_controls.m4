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

dnl # TDFB enum list
dnl CONTROLENUM_LIST(DEF_TDFB_ONOFF, LIST(`	', `"beam on"', `"beam off"'))

dnl # TDFB enum control
dnl C_CONTROLENUM(DEF_TDFB_ONOFF_ENUM, PIPELINE_ID,
dnl 	DEF_TDFB_ONOFF,
dnl	LIST(`	', ENUM_CHANNEL(FC, 3, 0)),
dnl	CONTROLENUM_OPS(enum,
dnl		257 binds the mixer control to enum get/put handlers,
dnl		257, 257))

dnl # TDFB enum list
dnl CONTROLENUM_LIST(DEF_TDFB_BEAMN, LIST(`	', `"0"', `"30"', `"60"', `"90"', `"120"', `"150"', `"180"', `"210"', `"240"', `"270"', `"300"', `"330"'))

dnl # TDFB enum control
dnl C_CONTROLENUM(DEF_TDFB_DIRECTION_ENUM, PIPELINE_ID,
dnl	DEF_TDFB_BEAMN,
dnl	LIST(`	', ENUM_CHANNEL(FC, 3, 0)),
dnl	CONTROLENUM_OPS(enum,
dnl		257 binds the mixer control to enum get/put handlers,
dnl		257, 257))
