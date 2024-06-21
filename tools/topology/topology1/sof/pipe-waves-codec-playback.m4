# Low Latency Passthrough with Waves codec Pipeline and PCM
#
# Pipeline Endpoints for connection are :-
#
# host PCM_P --> B0 --> Waves codec --> B1 --> sink DAI0

ifdef(`ENDPOINT_NAME',`',`fatal_error(`Pipe requires ENDPOINT_NAME to be defined: Speakers, Headphones, etc.')')

# Waves codec setup config
define(`CA_SETUP_CONTROLBYTES',
``      bytes "0x53,0x4f,0x46,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x0C,0x00,0x00,0x00,'
`       0x00,0x10,0x00,0x03,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00,'

`       0x00,0x00,0x00,0x00,'
`       0x0c,0x00,0x00,0x00,'
`       0x00,0x00,0x00,0x00"''
)
define(`CA_SETUP_CONTROLBYTES_MAX', 8192)

ifelse(PLATFORM, `tgl', `
	define(CA_SETUP_CONTROLBYTES_NAME, `MaxxChrome Setup ')', `
	define(CA_SETUP_CONTROLBYTES_NAME, `Waves' `ENDPOINT_NAME' `Setup ')')

define(`CA_SCHEDULE_CORE', 0)

DECLARE_SOF_RT_UUID("Waves codec", waves_codec_uuid, 0xd944281a, 0xafe9,
		    0x4695, 0xa0, 0x43, 0xd7, 0xf6, 0x2b, 0x89, 0x53, 0x8e);
define(`CA_UUID', waves_codec_uuid)

# Include codec adapter playback topology
include(`sof/pipe-codec-adapter-playback.m4')
