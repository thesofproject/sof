# SPDX-License-Identifier: BSD-3-Clause

add_local_sources(sof aw_smart_amp_wapper.c aw_smart_amp.c)
target_include_directories(sof PUBLIC ${PROJECT_SOURCE_DIR}/src/audio/smart_amp_aw/aw_api/inc)
