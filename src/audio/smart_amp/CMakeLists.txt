# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof smart_amp_maxim_dsm.c smart_amp.c smart_amp_generic.c)
target_include_directories(sof PUBLIC ${PROJECT_SOURCE_DIR}/src/audio/smart_amp/dsm_api/inc)