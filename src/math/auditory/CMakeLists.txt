# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof auditory.c)

if(CONFIG_MATH_16BIT_MEL_FILTERBANK)
        add_local_sources(sof mel_filterbank_16.c)
endif()

if(CONFIG_MATH_32BIT_MEL_FILTERBANK)
        add_local_sources(sof mel_filterbank_32.c)
endif()
