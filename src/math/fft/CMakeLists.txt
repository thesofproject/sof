# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof fft_common.c)

if(CONFIG_MATH_16BIT_FFT)
        add_local_sources(sof fft_16.c fft_16_hifi3.c)
endif()

if(CONFIG_MATH_32BIT_FFT)
        add_local_sources(sof fft_32.c fft_32_hifi3.c)
endif()
