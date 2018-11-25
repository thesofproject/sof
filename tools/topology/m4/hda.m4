dnl HDA_DAI_CONFIG(idx, link_id, name)
define(`HDA_DAI_CONFIG',
`SectionVendorTuples."'N_DAI_CONFIG(HDA$1)`_tuples_common" {'
`       tokens "sof_dai_tokens"'
`       tuples."string" {'
`               SOF_TKN_DAI_TYPE                "HDA"'
`       }'
`       tuples."word" {'
`               SOF_TKN_DAI_INDEX'              STR($1)
`       }'
`}'
`SectionData."'N_DAI_CONFIG(HDA$1)`_data_common" {'
`       tuples "'N_DAI_CONFIG(HDA$1)`_tuples_common"'
`}'
`'
`SectionBE."'$3`" {'
`       id "'$2`"'
`       index "0"'
`       data ['
`               "'N_DAI_CONFIG(HDA$1)`_data_common"'
`       ]'
`}')
