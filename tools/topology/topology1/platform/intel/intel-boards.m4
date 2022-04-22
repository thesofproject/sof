#
# Machine driver differentiation for dai link ID
#

# variable that need to be defined in upper m4
ifdef(`PLATFORM',`',`fatal_error(note: Need to define platform identifier for intel-boards)')
ifdef(`LINUX_MACHINE_DRIVER',`',`fatal_error(note: Need to define linux machine driver identifier for intel-boards)')

define(`NUM_DMIC_BE_LINK', `2')

ifelse(
	PLATFORM, `cml', `define(`NUM_HDMI_BE_LINK', 3)',
	PLATFORM, `jsl', `define(`NUM_HDMI_BE_LINK', 3)',
	PLATFORM, `tgl', `define(`NUM_HDMI_BE_LINK', 4)',
	PLATFORM, `adl', `define(`NUM_HDMI_BE_LINK', 4)',
	`fatal_error(note: Unknown platform to intel-boards)')

# define the ID base for backend DAI Links
ifelse(
	LINUX_MACHINE_DRIVER, `sof_rt5682', `
	define(`BOARD_HP_BE_ID', `0')
	define(`BOARD_DMIC_BE_ID_BASE', eval(BOARD_HP_BE_ID + 1))
	define(`BOARD_HDMI_BE_ID_BASE', eval(BOARD_DMIC_BE_ID_BASE + NUM_DMIC_BE_LINK))
	ifdef(`NO_AMP', `', `define(`BOARD_SPK_BE_ID', eval(BOARD_HDMI_BE_ID_BASE + NUM_HDMI_BE_LINK))')
	ifdef(`NO_AMP', `define(`BOARD_BT_BE_ID', eval(BOARD_HDMI_BE_ID_BASE + NUM_HDMI_BE_LINK))', `define(`BOARD_BT_BE_ID', eval(BOARD_SPK_BE_ID + 1))')',
	LINUX_MACHINE_DRIVER, `sof_ssp_amp', `
	define(`BOARD_SPK_BE_ID', `0')
	ifdef(`NO_DMICS', `', `define(`BOARD_DMIC_BE_ID_BASE', eval(BOARD_SPK_BE_ID + 1))')
	ifdef(`NO_DMICS', `define(`BOARD_HDMI_BE_ID_BASE', eval(BOARD_SPK_BE_ID + 1))', `define(`BOARD_HDMI_BE_ID_BASE', eval(BOARD_DMIC_BE_ID_BASE + NUM_DMIC_BE_LINK))')
	define(`BOARD_BT_BE_ID', eval(BOARD_HDMI_BE_ID_BASE + NUM_HDMI_BE_LINK))',
	`fatal_error(note: Unknown linux machine driver to intel-boards)')

undefine(`NUM_DMIC_BE_LINK')
undefine(`NUM_HDMI_BE_LINK')
