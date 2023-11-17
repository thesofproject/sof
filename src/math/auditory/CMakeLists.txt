# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof auditory.c)

add_local_sources_ifdef(CONFIG_MATH_16BIT_MEL_FILTERBANK sof mel_filterbank_16.c)

add_local_sources_ifdef(CONFIG_MATH_32BIT_MEL_FILTERBANK sof mel_filterbank_32.c)
