# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_GOOGLE_CTC_AUDIO_PROCESSING_MOCK)
sof_llext_build("google_ctc_audio_processing"
	SOURCES ../google_ctc_audio_processing.c
		../google_ctc_audio_processing_ipc4.c
		../google_ctc_audio_processing_mock.c
	LIB openmodules
)
target_include_directories(google_ctc_audio_processing_llext_lib PRIVATE
	"${sof_top_dir}/third_party/include"
)
else()
message(FATAL_ERROR "Add library linking support in src/audio/google/llext_ctc/CMakeLists.txt")
endif()
