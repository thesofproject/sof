# SPDX-License-Identifier: BSD-3-Clause

set(base_files auditory.c)

if(CONFIG_MATH_16BIT_MEL_FILTERBANK)
  list(APPEND base_files mel_filterbank_16.c)
endif()

if(CONFIG_MATH_32BIT_MEL_FILTERBANK)
  list(APPEND base_files mel_filterbank_32.c)
endif()

is_zephyr(it_is)
if(it_is) ###  Zephyr ###

  zephyr_library_sources(
    ${base_files}
  )

else() ### XTOS ###

  add_local_sources(sof ${base_files})

endif()
