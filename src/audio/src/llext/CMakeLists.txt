# Copyright (c) 2024 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COMP_SRC_LITE)
sof_llext_build("src"
	SOURCES ../src_hifi2ep.c
		../src_generic.c
		../src_hifi3.c
		../src_hifi4.c
		../src.c
		../src_common.c
		../src_ipc4.c
		../src_lite.c
	LIB openmodules
)
else()
sof_llext_build("src"
	SOURCES ../src_hifi2ep.c
		../src_generic.c
		../src_hifi3.c
		../src_hifi4.c
		../src.c
		../src_common.c
		../src_ipc4.c
	LIB openmodules
)
endif()
