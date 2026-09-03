include(`abi.h')

dnl ABI_INFO(name, major, minor, patch)
define(`ABI_INFO',
`SectionData."SOF_ABI" {'
`	bytes "$2,$3,$4"'
`}'
`SectionManifest.STR($1) {'
`	data ['
`		SOF_ABI'
`	]'
`}')

ABI_INFO(sof_manifest, SOF_ABI_MAJOR, SOF_ABI_MINOR, SOF_ABI_PATCH)
