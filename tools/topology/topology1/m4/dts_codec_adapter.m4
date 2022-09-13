

# DTS codec setup config
define(`CA_SETUP_CONTROLBYTES',
``      bytes "0x53,0x4f,0x46,0x00,0x00,0x00,0x00,0x00,'
`       0x0c,0x00,0x00,0x00,0x00,0x10,0x00,0x03,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,'

`       0x00,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00"''
)
define(`CA_SETUP_CONTROLBYTES_MAX', 8192)
define(`CA_SETUP_CONTROLBYTES_NAME', `DTS Codec Setup ')

define(`CA_SCHEDULE_CORE', 0)

DECLARE_SOF_RT_UUID("DTS codec", dts_uuid, 0xd95fc34f, 0x370f, 0x4ac7, 0xbc, 0x86, 0xbf, 0xdc, 0x5b, 0xe2, 0x41, 0xe6)
define(`CA_UUID', dts_uuid)


include(`codec_adapter.m4')


define(CA_SETUP_CONFIG, concat(`ca_setup_config_', PIPELINE_ID))
define(CA_SETUP_CONTROLBYTES_NAME_PIPE, concat(CA_SETUP_CONTROLBYTES_NAME, PIPELINE_ID))


# Codec adapter setup config
CONTROLBYTES_PRIV(CA_SETUP_CONFIG, CA_SETUP_CONTROLBYTES)

# Codec adapter Bytes control for setup config
C_CONTROLBYTES(CA_SETUP_CONTROLBYTES_NAME_PIPE, PIPELINE_ID,
	CONTROLBYTES_OPS(bytes),
	CONTROLBYTES_EXTOPS(void, 258, 258),
	, , ,
	CONTROLBYTES_MAX(void, CA_SETUP_CONTROLBYTES_MAX),
	,
	CA_SETUP_CONFIG)
