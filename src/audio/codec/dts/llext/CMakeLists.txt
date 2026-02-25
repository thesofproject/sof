# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_DTS_CODEC_STUB)
	sof_llext_build("dts"
		SOURCES ../dts.c
			../dts_stub.c
	)
	target_include_directories(dts_llext_lib PRIVATE
		"../../../../../third_party/include"
	)
elseif(CONFIG_DTS_CODEC STREQUAL "m")
	sof_llext_build("dts"
		SOURCES ../dts.c
		INCLUDES  "${sof_top_dir}/third_party/include"
		LIBS_PATH "${sof_top_dir}/third_party/lib/"
		LIBS dts-sof-interface-i32 m c gcc
	)
else()
message(FATAL_ERROR "Add library linking support in src/audio/codec/dts/llext/CMakeLists.txt")
endif()
