# Copyright(c) 2025 Dolby Laboratories.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_DOLBY_DAX_AUDIO_PROCESSING_MOCK)
	sof_llext_build("dolby_dax_audio_processing"
		SOURCES ../dax.c
			../dax_mock.c
		INCLUDES ${sof_top_dir}/third_party/include
	)
else()
	sof_llext_build("dolby_dax_audio_processing"
		SOURCES ../dax.c ../llext-wrap.c
		INCLUDES ${sof_top_dir}/third_party/include
		LIBS_PATH ${sof_top_dir}/third_party/lib/
		LIBS dax m c gcc
	)
endif()
