# SPDX-License-Identifier: BSD-3-Clause

set(base_files fft_common.c)

if(CONFIG_MATH_16BIT_FFT)
  list(APPEND base_files fft_16.c fft_16_hifi3.c)
endif()

if(CONFIG_MATH_32BIT_FFT)
  list(APPEND base_files fft_32.c fft_32_hifi3.c)
endif()

is_zephyr(zephyr)
if(zephyr) ###  Zephyr ###

  zephyr_library_sources(
    ${base_files}
  )

else() ### library, e.g. testbench or plugin ###

  add_local_sources(sof ${base_files})

endif()
