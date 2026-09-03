# TDFB Bytes control with max value of 255
C_CONTROLBYTES(DEF_TDFB_BYTES, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes,
		258 binds the mixer control to bytes get/put handlers,
		258, 258),
	CONTROLBYTES_EXTOPS(258 binds the mixer control to bytes get/put handlers,
		258, 258),
	, , ,
	CONTROLBYTES_MAX(, 8192),
	,
	DEF_TDFB_PRIV)

# TDFB switch control with max value of 1
define(`CONTROL_NAME', `DEF_TDFB_BEAM')
C_CONTROLMIXER(TDFB Process, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 259 binds the mixer control to switch get/put handlers, 259, 259),
	CONTROLMIXER_MAX(max 1 indicates switch type control, 1),
	false,
	,
	Channel register and shift for Front Center,
	LIST(`	', ENUM_CHANNEL(FC, 3, 0)),
	"1")
undefine(`CONTROL_NAME')

define(`CONTROL_NAME', `DEF_TDFB_DIRECTION')
C_CONTROLMIXER(TDFB Direction, PIPELINE_ID,
	CONTROLMIXER_OPS(volsw, 259 binds the mixer control to switch get/put handlers, 259, 259),
	CONTROLMIXER_MAX(max 1 indicates switch type control, 1),
	false,
	,
	Channel register and shift for Front Center,
	LIST(`	', ENUM_CHANNEL(FC, 3, 0)),
	"1")
undefine(`CONTROL_NAME')

# TDFB enum list
CONTROLENUM_LIST(DEF_TDFB_AZIMUTH_VALUES,
	LIST(`	', `"0"', `"30"', `"60"', `"90"', `"120"', `"150"', `"180"', `"210"', `"240"', `"270"', `"300"', `"330"'))

# TDFB enum control
C_CONTROLENUM(DEF_TDFB_AZIMUTH, PIPELINE_ID,
	DEF_TDFB_AZIMUTH_VALUES,
	LIST(`	', ENUM_CHANNEL(FC, 3, 0)),
	CONTROLENUM_OPS(enum,
	257 binds the mixer control to enum get/put handlers,
	257, 257))

C_CONTROLENUM(DEF_TDFB_AZIMUTH_ESTIMATE, PIPELINE_ID,
	DEF_TDFB_AZIMUTH_VALUES,
	LIST(`	', ENUM_CHANNEL(FC, 3, 0)),
	CONTROLENUM_OPS(enum,
	257 binds the mixer control to enum get/put handlers,
	257, 257))
