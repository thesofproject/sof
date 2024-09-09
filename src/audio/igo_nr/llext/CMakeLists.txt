# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_DTS_CODEC_STUB)
sof_llext_build("igo_nr"
	SOURCES ../igo_nr.c
		../igo_nr_stub.c
)
target_include_directories(igo_nr_llext_lib PRIVATE
	"../../../../third_party/include"
)
else()
message(FATAL_ERROR "Add library linking support in src/audio/codec/igo_nr/llext/CMakeLists.txt")
endif()
