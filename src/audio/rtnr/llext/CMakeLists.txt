# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_RTNR_STUB)
sof_llext_build("rtnr"
	SOURCES ../rtnr.c
		../rtnr_stub.c
	LIB openmodules
)
else()
message(FATAL_ERROR "Add library linking support in src/audio/rtnr/llext/CMakeFiles.txt")
endif()
