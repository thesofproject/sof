# SPDX-License-Identifier: BSD-3-Clause

set(LIBLC3_DIR "")
if(DEFINED ZEPHYR_LIBLC3_MODULE_DIR)
	if(EXISTS "${ZEPHYR_LIBLC3_MODULE_DIR}/include/lc3.h")
		set(LIBLC3_DIR "${ZEPHYR_LIBLC3_MODULE_DIR}")
	endif()
endif()

if("${LIBLC3_DIR}" STREQUAL "")
	cmake_path(SET LIBLC3_DIR NORMALIZE "${CMAKE_CURRENT_LIST_DIR}/../../../../modules/lib/liblc3")
endif()

zephyr_include_directories(
	${LIBLC3_DIR}/include
	${LIBLC3_DIR}/src
)

set(LIBLC3_SOURCES
	${LIBLC3_DIR}/src/attdet.c
	${LIBLC3_DIR}/src/bits.c
	${LIBLC3_DIR}/src/bwdet.c
	${LIBLC3_DIR}/src/energy.c
	${LIBLC3_DIR}/src/lc3.c
	${LIBLC3_DIR}/src/ltpf.c
	${LIBLC3_DIR}/src/mdct.c
	${LIBLC3_DIR}/src/plc.c
	${LIBLC3_DIR}/src/sns.c
	${LIBLC3_DIR}/src/spec.c
	${LIBLC3_DIR}/src/tables.c
	${LIBLC3_DIR}/src/tns.c
)

zephyr_compile_definitions(
	LC3_PLUS=0
	LC3_PLUS_HR=0
)

add_local_sources(sof
	bt_codec.c
	bt_codec_algos.c
	${LIBLC3_SOURCES}
)

set(BT_CODEC_COMPILE_OPTIONS
	-O3
	-ffast-math
	-fno-finite-math-only
	-Wno-array-bounds
	-Wno-double-promotion
	-include
	${CMAKE_CURRENT_LIST_DIR}/lc3_math_shim.h
)

# When building with Clang on platforms where hardware FPU is disabled (e.g.
# ACE 3.0 PTL / WCL with CONFIG_FPU=n), strip the Xtensa FP0 feature so liblc3
# and bt_codec emit software floating-point calls instead of hardware FP0
# instructions that fault when CPENABLE bit 0 is cleared.
if(CMAKE_C_COMPILER_ID MATCHES "Clang" AND NOT CONFIG_FPU)
	list(APPEND BT_CODEC_COMPILE_OPTIONS
		-Xclang
		-target-feature
		-Xclang
		-fp
	)
endif()

set_source_files_properties(
	${CMAKE_CURRENT_LIST_DIR}/bt_codec.c
	${CMAKE_CURRENT_LIST_DIR}/bt_codec_algos.c
	${LIBLC3_SOURCES}
	TARGET_DIRECTORY modules_sof
	PROPERTIES COMPILE_OPTIONS "${BT_CODEC_COMPILE_OPTIONS}"
)
