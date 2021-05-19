MEMORY
{
  error_infra_log_entries_seg :         org = 0x20000000, len = 0xFFFFF
  error_hal_log_entries_seg :           org = 0x20100000, len = 0xFFFFF
  error_module_log_entries_seg :        org = 0x20200000, len = 0xFFFFF
  error_audio_log_entries_seg :         org = 0x20300000, len = 0xFFFFF
  error_sensing_log_entries_seg :       org = 0x20400000, len = 0xFFFFF
  error_ulp_infra_log_entries_seg :     org = 0x20500000, len = 0xFFFFF
  error_ulp_module_log_entries_seg :    org = 0x20600000, len = 0xFFFFF

  warning_infra_log_entries_seg :       org = 0x21000000, len = 0xFFFFF
  warning_hal_log_entries_seg :         org = 0x21100000, len = 0xFFFFF
  warning_module_log_entries_seg :      org = 0x21200000, len = 0xFFFFF
  warning_audio_log_entries_seg :       org = 0x21300000, len = 0xFFFFF
  warning_sensing_log_entries_seg :     org = 0x21400000, len = 0xFFFFF
  warning_ulp_infra_log_entries_seg :   org = 0x21500000, len = 0xFFFFF
  warning_ulp_module_log_entries_seg :  org = 0x21600000, len = 0xFFFFF

  med_infra_log_entries_seg :           org = 0x22000000, len = 0xFFFFF
  med_hal_log_entries_seg :             org = 0x22100000, len = 0xFFFFF
  med_module_log_entries_seg :          org = 0x22200000, len = 0xFFFFF
  med_audio_log_entries_seg :           org = 0x22300000, len = 0xFFFFF
  med_sensing_log_entries_seg :         org = 0x22400000, len = 0xFFFFF
  med_ulp_infra_log_entries_seg :       org = 0x22500000, len = 0xFFFFF
  med_ulp_module_log_entries_seg :      org = 0x22600000, len = 0xFFFFF

  info_infra_log_entries_seg :          org = 0x23000000, len = 0xFFFFF
  info_hal_log_entries_seg :            org = 0x23100000, len = 0xFFFFF
  info_module_log_entries_seg :         org = 0x23200000, len = 0xFFFFF
  info_audio_log_entries_seg :          org = 0x23300000, len = 0xFFFFF
  info_sensing_log_entries_seg :        org = 0x23400000, len = 0xFFFFF
  info_ulp_infra_log_entries_seg :      org = 0x23500000, len = 0xFFFFF
  info_ulp_module_log_entries_seg :     org = 0x23600000, len = 0xFFFFF

  dbg_infra_log_entries_seg :           org = 0x24000000, len = 0xFFFFF
  dbg_hal_log_entries_seg :             org = 0x24100000, len = 0xFFFFF
  dbg_module_log_entries_seg :          org = 0x24200000, len = 0xFFFFF
  dbg_audio_log_entries_seg :           org = 0x24300000, len = 0xFFFFF
  dbg_sensing_log_entries_seg :         org = 0x24400000, len = 0xFFFFF
  dbg_ulp_infra_log_entries_seg :       org = 0x24500000, len = 0xFFFFF
  dbg_ulp_module_log_entries_seg :      org = 0x24600000, len = 0xFFFFF

  function_strings_seg :                org = 0x25000000, len = 0x100000
  asserts_desc_seg :                    org = 0x26000000, len = 0x100000
}

PHDRS
{
  static_log_entries_phdr PT_NOTE;
  function_strings_phdr PT_NOTE;
  asserts_desc_phdr PT_NOTE;
}

SECTIONS
{
  .error_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_ERROR.L_INFRA)
    *(*.static_log.*L_CRITICAL.L_INFRA)
  } >error_infra_log_entries_seg :static_log_entries_phdr

  .error_hal_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_ERROR.L_HAL)
    *(*.static_log.*L_CRITICAL.L_HAL)
  } >error_hal_log_entries_seg :static_log_entries_phdr

  .error_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_ERROR.L_MODULE)
    *(*.static_log.*L_CRITICAL.L_MODULE)
  } >error_module_log_entries_seg :static_log_entries_phdr

  .error_audio_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_ERROR.L_AUDIO)
    *(*.static_log.*L_CRITICAL.L_MODULE)
  } >error_audio_log_entries_seg :static_log_entries_phdr

  .error_sensing_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_ERROR.L_SENSING)
    *(*.static_log.*L_CRITICAL.L_MODULE)
  } >error_sensing_log_entries_seg :static_log_entries_phdr

  .error_ulp_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_ERROR.L_ULP_INFRA)
    *(*.static_log.*L_CRITICAL.L_ULP_INFRA)
  } >error_ulp_infra_log_entries_seg :static_log_entries_phdr

  .error_ulp_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_ERROR.L_ULP_MODULE)
    *(*.static_log.*L_CRITICAL.L_ULP_MODULE)
  } >error_ulp_module_log_entries_seg :static_log_entries_phdr

  .warning_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_HIGH.L_INFRA)
    *(*.static_log.*L_WARNING.L_INFRA)
  } >warning_infra_log_entries_seg :static_log_entries_phdr

  .warning_hal_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_HIGH.L_HAL)
    *(*.static_log.*L_WARNING.L_HAL)
  } >warning_hal_log_entries_seg :static_log_entries_phdr

  .warning_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_HIGH.L_MODULE)
    *(*.static_log.*L_WARNING.L_MODULE)
  } >warning_module_log_entries_seg :static_log_entries_phdr

  .warning_audio_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_HIGH.L_AUDIO)
    *(*.static_log.*L_WARNING.L_AUDIO)
  } >warning_audio_log_entries_seg :static_log_entries_phdr

  .warning_sensing_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_HIGH.L_SENSING)
    *(*.static_log.*L_WARNING.L_SENSING)
  } >warning_sensing_log_entries_seg :static_log_entries_phdr

  .warning_ulp_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_HIGH.L_ULP_INFRA)
    *(*.static_log.*L_WARNING.L_ULP_INFRA)
  } >warning_ulp_infra_log_entries_seg :static_log_entries_phdr

  .warning_ulp_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_HIGH.L_ULP_MODULE)
    *(*.static_log.*L_WARNING.L_ULP_MODULE)
  } >warning_ulp_module_log_entries_seg :static_log_entries_phdr

  .med_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_MEDIUM.L_INFRA)
  } >med_infra_log_entries_seg :static_log_entries_phdr

  .med_hal_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_MEDIUM.L_HAL)
  } >med_hal_log_entries_seg :static_log_entries_phdr

  .med_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_MEDIUM.L_MODULE)
  } >med_module_log_entries_seg :static_log_entries_phdr

  .med_audio_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_MEDIUM.L_AUDIO)
  } >med_audio_log_entries_seg :static_log_entries_phdr

  .med_sensing_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_MEDIUM.L_SENSING)
  } >med_sensing_log_entries_seg :static_log_entries_phdr

  .med_ulp_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_MEDIUM.L_ULP_INFRA)
  } >med_ulp_infra_log_entries_seg :static_log_entries_phdr

  .med_ulp_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_MEDIUM.L_ULP_MODULE)
  } >med_ulp_module_log_entries_seg :static_log_entries_phdr

  .info_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_LOW.L_INFRA)
    *(*.static_log.*L_INFO.L_INFRA)
  } >info_infra_log_entries_seg :static_log_entries_phdr

  .info_hal_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_LOW.L_HAL)
    *(*.static_log.*L_INFO.L_HAL)
  } >info_hal_log_entries_seg :static_log_entries_phdr

  .info_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_LOW.L_MODULE)
    *(*.static_log.*L_INFO.L_MODULE)
  } >info_module_log_entries_seg :static_log_entries_phdr

  .info_audio_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_LOW.L_AUDIO)
    *(*.static_log.*L_INFO.L_AUDIO)
  } >info_audio_log_entries_seg :static_log_entries_phdr

  .info_sensing_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_LOW.L_SENSING)
    *(*.static_log.*L_INFO.L_SENSING)
  } >info_sensing_log_entries_seg :static_log_entries_phdr

  .info_ulp_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_LOW.L_ULP_INFRA)
    *(*.static_log.*L_INFO.L_ULP_INFRA)
  } >info_ulp_infra_log_entries_seg :static_log_entries_phdr

  .info_ulp_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_LOW.L_ULP_MODULE)
    *(*.static_log.*L_INFO.L_ULP_MODULE)
  } >info_ulp_module_log_entries_seg :static_log_entries_phdr

  .dbg_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_VERBOSE.L_INFRA)
    *(*.static_log.*L_DEBUG.L_INFRA)
  } >dbg_infra_log_entries_seg :static_log_entries_phdr

  .dbg_hal_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_VERBOSE.L_HAL)
    *(*.static_log.*L_DEBUG.L_HAL)
  } >dbg_hal_log_entries_seg :static_log_entries_phdr

  .dbg_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_VERBOSE.L_MODULE)
    *(*.static_log.*L_DEBUG.L_MODULE)
  } >dbg_module_log_entries_seg :static_log_entries_phdr

  .dbg_audio_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_VERBOSE.L_AUDIO)
    *(*.static_log.*L_DEBUG.L_AUDIO)
  } >dbg_audio_log_entries_seg :static_log_entries_phdr

  .dbg_sensing_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_VERBOSE.L_SENSING)
    *(*.static_log.*L_DEBUG.L_SENSING)
  } >dbg_sensing_log_entries_seg :static_log_entries_phdr

  .dbg_ulp_infra_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_VERBOSE.L_ULP_INFRA)
    *(*.static_log.*L_DEBUG.L_ULP_INFRA)
  } >dbg_ulp_infra_log_entries_seg :static_log_entries_phdr

  .dbg_ulp_module_log_entries : ALIGN(64)
  {
    *(*.static_log.*L_VERBOSE.L_ULP_MODULE)
    *(*.static_log.*L_DEBUG.L_ULP_MODULE)
  } >dbg_ulp_module_log_entries_seg :static_log_entries_phdr

  .function_strings : ALIGN(4)
  {
    *(*.function_strings)
  } > function_strings_seg :function_strings_phdr

  .asserts_desc : ALIGN(4)
  {
    *(*.assert_entries)
  } > asserts_desc_seg :asserts_desc_phdr

}
