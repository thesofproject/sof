# Copyright (c) 2025 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_WAVES_CODEC_STUB)
sof_llext_build("waves"
	SOURCES ../waves.c
		../maxx_stub.c
)
else()
message(FATAL_ERROR "Add library linking support in src/audio/module_adapter/module/waves/llext/CMakeLists.txt")
endif()
