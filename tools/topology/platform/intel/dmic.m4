divert(-1)

dnl  DMIC related macros

dnl PDM_TUPLES(pdm ctrl id, mic_a_enable, mic_b_enable, polarity_a, polarity_b,
dnl	       clk_egde, skew)
define(`PDM_TUPLES',
`	tuples."short.pdm$1" {'
`		SOF_TKN_INTEL_DMIC_PDM_CTRL_ID'		STR($1)
`		SOF_TKN_INTEL_DMIC_PDM_MIC_A_Enable'	STR($2)
`		SOF_TKN_INTEL_DMIC_PDM_MIC_B_Enable'	STR($3)
`		SOF_TKN_INTEL_DMIC_PDM_POLARITY_A'	STR($4)
`		SOF_TKN_INTEL_DMIC_PDM_POLARITY_B'	STR($5)
`		SOF_TKN_INTEL_DMIC_PDM_CLK_EDGE'	STR($6)
`		SOF_TKN_INTEL_DMIC_PDM_SKEW'		STR($7)
`	}'
)

dnl PDM_CONFIG(type, idx, num pdm active, pdm tuples list)
define(`PDM_CONFIG',
`		SOF_TKN_INTEL_DMIC_NUM_PDM_ACTIVE'	STR($3)
`	}'
`}'
`SectionVendorTuples."'N_DAI_CONFIG($1$2)`_pdm_tuples" {'
`	tokens "sof_dmic_pdm_tokens"'
$4
`}'
)

dnl DMIC currently only supports 16 bit or 32-bit word length
dnl DMIC_WORD_LENGTH(frame format)
define(`DMIC_WORD_LENGTH',
`ifelse($1, `s16le', 16, $1, `s32le', 32, `')')

dnl DMIC_CONFIG(driver_version, clk_min, clk_mac, duty_min, duty_max,
dnl		sample_rate,
dnl		fifo word length, type, idx, pdm controller config)
define(`DMIC_CONFIG',
`SectionVendorTuples."'N_DAI_CONFIG($8$9)`_dmic_tuples" {'
`	tokens "sof_dmic_tokens"'
`	tuples."word" {'
`		SOF_TKN_INTEL_DMIC_DRIVER_VERSION'	STR($1)
`		SOF_TKN_INTEL_DMIC_CLK_MIN'		STR($2)
`		SOF_TKN_INTEL_DMIC_CLK_MAX'		STR($3)
`		SOF_TKN_INTEL_DMIC_DUTY_MIN'		STR($4)
`		SOF_TKN_INTEL_DMIC_DUTY_MAX'		STR($5)
`		SOF_TKN_INTEL_DMIC_SAMPLE_RATE'		STR($6)
`		SOF_TKN_INTEL_DMIC_FIFO_WORD_LENGTH'	STR($7)
dnl PDM config for the number of active PDM controllers
$10
`SectionData."'N_DAI_CONFIG($8$9)`_pdm_data" {'
`	tuples "'N_DAI_CONFIG($8$9)`_pdm_tuples"'
`}'
`SectionData."'N_DAI_CONFIG($8$9)`_data" {'
`	tuples "'N_DAI_CONFIG($8$9)`_dmic_tuples"'

`}'
)

dnl DMIC PDM configurations
dnl macros to get the number of active pdm's and their config
define(`MONO_PDM0_MICA', `1, LIST(`', PDM_TUPLES(0, 1, 0, 0, 0, 0, 0))')
define(`MONO_PDM0_MICB', `1, LIST(`', PDM_TUPLES(0, 0, 1, 0, 0, 0, 0))')
define(`STEREO_PDM0', `1, LIST(`', PDM_TUPLES(0, 1, 1, 0, 0, 0, 0))')
define(`STEREO_PDM1', `1, LIST(`', PDM_TUPLES(1, 1, 1, 0, 0, 0, 0))')
define(`FOUR_CH_PDM0_PDM1',
	`2, LIST(`', PDM_TUPLES(0, 1, 1, 0, 0, 0, 0), PDM_TUPLES(1, 1, 1, 0, 0, 0, 0))')

divert(0)dnl
