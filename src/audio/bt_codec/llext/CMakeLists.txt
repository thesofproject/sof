# SPDX-License-Identifier: BSD-3-Clause

set(BT_CODEC_LLEXT_CFLAGS
	-DLC3_PLUS=0
	-DLC3_PLUS_HR=0
	${BT_CODEC_COMPILE_OPTIONS}
	"SHELL:-include ${CMAKE_CURRENT_LIST_DIR}/../lc3_math_shim.h"
)

if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CONFIG_FPU)
	list(APPEND BT_CODEC_LLEXT_CFLAGS
		"SHELL:-Xclang -target-feature -Xclang -fp"
	)
endif()

sof_llext_build("bt_codec"
	SOURCES ../bt_codec.c
		../bt_codec_algos.c
		${LIBLC3_SOURCES}
	INCLUDES "${LIBLC3_DIR}/include"
		 "${LIBLC3_DIR}/src"
		 "${CMAKE_CURRENT_LIST_DIR}/.."
	CFLAGS ${BT_CODEC_LLEXT_CFLAGS}
	LIB openmodules
)
