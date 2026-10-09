# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0
#
# LLEXT build path for the mww component (CONFIG_COMP_MWW=m). Structurally
# mirrors src/audio/tensorflow/llext/CMakeLists.txt, but targeted at aphid
# (ace30/PTL, Clang/LLVM toolchain) rather than that file's ace15_mtpm/Xtensa
# reference build -- see plan Stage 8. The toolchain paths below are derived
# from SOC_TOOLCHAIN_NAME/ZEPHYR_SDK_INSTALL_DIR rather than hardcoded, since
# this file is authored in the sof-tgl worktree before the aphid-specific
# build in sof-ptl/sof exists; re-verify against the real build-ptl-llvm
# toolchain paths when Stage 8 is actually run.

set(TFLM_PATH ${sof_top_dir}/../tflite-micro)
set(FLATBUFFERS_PATH ${sof_top_dir}/../flatbuffers)
set(GEMMLOWP_PATH ${sof_top_dir}/../gemmlowp)
set(RUY_PATH ${sof_top_dir}/../ruy)
set(NN_HIFI_PATH ${sof_top_dir}/../nnlib-hifi4/xa_nnlib)

set(MWW_TOOLCHAIN_ROOT "")
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
	execute_process(
		COMMAND ${CMAKE_C_COMPILER} -print-file-name=libc.a
		OUTPUT_VARIABLE MWW_RAW_LIBC_PATH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	if(EXISTS "${MWW_RAW_LIBC_PATH}")
		get_filename_component(MWW_RESOLVED_LIBC "${MWW_RAW_LIBC_PATH}" REALPATH)
		get_filename_component(MWW_TOOLCHAIN_LIB_DIR "${MWW_RESOLVED_LIBC}" DIRECTORY)
		get_filename_component(MWW_TOOLCHAIN_ROOT "${MWW_TOOLCHAIN_LIB_DIR}" DIRECTORY)
	endif()
endif()

if(NOT EXISTS "${MWW_TOOLCHAIN_ROOT}")
	if(DEFINED CMAKE_SYSROOT AND EXISTS "${CMAKE_SYSROOT}/lib")
		set(MWW_TOOLCHAIN_ROOT "${CMAKE_SYSROOT}")
	else()
		set(MWW_SDK_DIR "${ZEPHYR_SDK_INSTALL_DIR}")
		if("${MWW_SDK_DIR}" STREQUAL "" AND NOT "$ENV{ZEPHYR_SDK_INSTALL_DIR}" STREQUAL "")
			set(MWW_SDK_DIR "$ENV{ZEPHYR_SDK_INSTALL_DIR}")
		endif()
		if("${MWW_SDK_DIR}" STREQUAL "")
			file(GLOB MWW_POSSIBLE_SDKS "$ENV{HOME}/zephyr-sdk*")
			if(MWW_POSSIBLE_SDKS)
				list(REVERSE MWW_POSSIBLE_SDKS)
				list(GET MWW_POSSIBLE_SDKS 0 MWW_SDK_DIR)
			endif()
		endif()

		if(NOT "${MWW_SDK_DIR}" STREQUAL "")
			file(GLOB MWW_SDK_SYSROOT
				"${MWW_SDK_DIR}/gnu/*${SOC_TOOLCHAIN_NAME}*/*${SOC_TOOLCHAIN_NAME}*")
			if(MWW_SDK_SYSROOT)
				list(GET MWW_SDK_SYSROOT 0 MWW_TOOLCHAIN_ROOT)
			endif()
		endif()
	endif()
endif()

# Discover C++ standard library version dynamically
set(MWW_CXX_INCLUDE_DIRS "")
if(NOT "${MWW_TOOLCHAIN_ROOT}" STREQUAL "" AND EXISTS "${MWW_TOOLCHAIN_ROOT}/include")
	set(MWW_INC_ROOT "${MWW_TOOLCHAIN_ROOT}/include")
	file(GLOB MWW_CXX_VERSION_DIRS LIST_DIRECTORIES true "${MWW_INC_ROOT}/c++/*")
	foreach(ver_dir ${MWW_CXX_VERSION_DIRS})
		get_filename_component(ver_name "${ver_dir}" NAME)
		if(IS_DIRECTORY "${ver_dir}" AND NOT "${ver_name}" MATCHES "^(backward|tr1|tr2|bits|ext|debug|decimal|parallel|pstl)$")
			list(APPEND MWW_CXX_INCLUDE_DIRS "${ver_dir}")
			file(GLOB MWW_ARCH_SUBDIRS LIST_DIRECTORIES true
				"${ver_dir}/xtensa*"
				"${ver_dir}/*${SOC_TOOLCHAIN_NAME}*")
			foreach(dir ${MWW_ARCH_SUBDIRS})
				if(IS_DIRECTORY "${dir}")
					list(APPEND MWW_CXX_INCLUDE_DIRS "${dir}")
				endif()
			endforeach()
		endif()
	endforeach()
	list(APPEND MWW_CXX_INCLUDE_DIRS "${MWW_INC_ROOT}")
endif()
set(MWW_HAL_SOC_PATH ${sof_top_dir}/../modules/hal/xtensa/zephyr/soc/${SOC_TOOLCHAIN_NAME})

# As in tensorflow/llext/CMakeLists.txt, the HiFi4 nnlib optimized kernels
# are only pulled in for a genuine Xtensa (xcc) compiler; aphid's LLEXT
# module is built with Clang, so nn_hifi_lib is NOT built here and the
# reference (non-HiFi) TFLM kernel implementations are used instead. This
# mirrors the existing (arguably suboptimal, see plan/summary note)
# tflmcly behavior rather than diverging from it -- revisit in Stage 10.
set(MWW_LLEXT_KERNEL_SOURCES
	${TFLM_PATH}/tensorflow/lite/micro/kernels/call_once.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/var_handle.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/read_variable.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/assign_variable.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/concatenation.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/conv_common.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/depthwise_conv_common.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/fully_connected_common.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/logistic_common.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/quantize_common.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/reshape_common.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/strided_slice_common.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/dequantize_common.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/conv.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/depthwise_conv.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/fully_connected.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/logistic.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/quantize.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/reshape.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/strided_slice.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/dequantize.cc
)

add_library(mww_tflm_lib STATIC
	${TFLM_PATH}/tensorflow/compiler/mlir/lite/core/api/error_reporter.cc
	${TFLM_PATH}/tensorflow/compiler/mlir/lite/schema/schema_utils.cc
	${TFLM_PATH}/tensorflow/lite/core/c/common.cc
	${TFLM_PATH}/tensorflow/lite/core/api/flatbuffer_conversions.cc
	${TFLM_PATH}/tensorflow/lite/core/api/tensor_utils.cc
	${TFLM_PATH}/tensorflow/lite/kernels/internal/common.cc
	${TFLM_PATH}/tensorflow/lite/kernels/internal/tensor_utils.cc
	${TFLM_PATH}/tensorflow/lite/kernels/internal/runtime_shape.cc
	${TFLM_PATH}/tensorflow/lite/kernels/internal/quantization_util.cc
	${TFLM_PATH}/tensorflow/lite/kernels/internal/tensor_ctypes.cc
	${TFLM_PATH}/tensorflow/lite/kernels/internal/portable_tensor_utils.cc
	${TFLM_PATH}/tensorflow/lite/kernels/internal/reference/portable_tensor_utils.cc
	${TFLM_PATH}/tensorflow/lite/kernels/kernel_util.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/kernel_util.cc
	${MWW_LLEXT_KERNEL_SOURCES}
	${TFLM_PATH}/tensorflow/lite/micro/mock_micro_graph.cc
	${TFLM_PATH}/tensorflow/lite/micro/flatbuffer_utils.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_interpreter_graph.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_resource_variable.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_interpreter_context.cc
	${TFLM_PATH}/tensorflow/lite/micro/fake_micro_context.cc
	${TFLM_PATH}/tensorflow/lite/micro/arena_allocator/persistent_arena_buffer_allocator.cc
	${TFLM_PATH}/tensorflow/lite/micro/arena_allocator/recording_single_arena_buffer_allocator.cc
	${TFLM_PATH}/tensorflow/lite/micro/arena_allocator/non_persistent_arena_buffer_allocator.cc
	${TFLM_PATH}/tensorflow/lite/micro/arena_allocator/single_arena_buffer_allocator.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_allocator.cc
	${TFLM_PATH}/tensorflow/lite/micro/tflite_bridge/flatbuffer_conversions_bridge.cc
	${TFLM_PATH}/tensorflow/lite/micro/tflite_bridge/micro_error_reporter.cc
	${TFLM_PATH}/tensorflow/lite/micro/system_setup.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_context.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_log.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_profiler.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_time.cc
	# debug_log.cc is replaced by our printk-backed DebugLog() in mww.c.
	#${TFLM_PATH}/tensorflow/lite/micro/debug_log.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_op_resolver.cc
	${TFLM_PATH}/tensorflow/lite/micro/memory_helpers.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_allocation_info.cc
	${TFLM_PATH}/tensorflow/lite/micro/memory_planner/greedy_memory_planner.cc
	${TFLM_PATH}/tensorflow/lite/micro/memory_planner/non_persistent_buffer_planner_shim.cc
	${TFLM_PATH}/tensorflow/lite/micro/memory_planner/linear_memory_planner.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_utils.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_interpreter.cc
	../mww_model_data.cc
	../mww_model_data_banana.cc
	../mww_model_data_orange.cc
	../mww_model.cc
)

target_include_directories(mww_tflm_lib PRIVATE
	${TFLM_PATH}
	${FLATBUFFERS_PATH}/include
	${GEMMLOWP_PATH}
	${RUY_PATH}
	${sof_top_dir}/posix/include
	${sof_top_dir}/../modules/hal/xtensa/include
	${MWW_HAL_SOC_PATH}
)

target_compile_options(mww_tflm_lib PRIVATE
	-std=c++17
	-fno-rtti
	-fno-exceptions
	-fno-threadsafe-statics
	-fno-unwind-tables
	-fmessage-length=0
	-DTF_LITE_STATIC_MEMORY
	-DTF_LITE_DISABLE_X86_NEON
	-Wno-unused-parameter
	-DXTENSA
	-DTF_LITE_MCU_DEBUG_LOG
	-DTF_LITE_USE_CTIME
	-mlongcalls
	-fPIC
	# TF_LITE_STRIP_ERROR_STRINGS drops DebugLog()/DebugVsnprintf()'s calls
	# into vprintf/vsnprintf; NDEBUG drops libc assert(). Both chains pull
	# in libc.a objects (vfprintf/fwrite/abort) that are not built PIC-safe,
	# which GNU ld refuses ("dangerous relocation") when linking this
	# ET_DYN/PIC LLEXT.
	-DTF_LITE_STRIP_ERROR_STRINGS
	-DNDEBUG
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|Xtensa" OR CMAKE_C_COMPILER MATCHES "xt-clang")
	target_compile_options(mww_tflm_lib PRIVATE
		-stdlib=libc++
		-fno-vectorize
		-fno-slp-vectorize
	)
	if(NOT CMAKE_C_COMPILER MATCHES "xt-clang")
		target_compile_options(mww_tflm_lib PRIVATE
			-mcpu=${SOC_TOOLCHAIN_NAME}
		)
		if(MWW_CXX_INCLUDE_DIRS)
			target_include_directories(mww_tflm_lib SYSTEM PRIVATE ${MWW_CXX_INCLUDE_DIRS})
		endif()
	endif()
endif()

set(MWW_LIBS_PATHS .)
if(NOT "${MWW_TOOLCHAIN_ROOT}" STREQUAL "")
	list(APPEND MWW_LIBS_PATHS ${MWW_TOOLCHAIN_ROOT}/lib)
endif()

sof_llext_build("mww"
	SOURCES
		../mww.c
		../llext-wrap.c
	INCLUDES
		${TFLM_PATH}
		${FLATBUFFERS_PATH}/include
		${GEMMLOWP_PATH}
		${RUY_PATH}
		${sof_top_dir}/posix/include
	LIBS
		mww_tflm_lib
	LIBS_PATH
		${MWW_LIBS_PATHS}
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_C_COMPILER MATCHES "xt-clang")
	set(MWW_LD_PATH "")
	if(DEFINED CMAKE_LINKER AND EXISTS "${CMAKE_LINKER}")
		set(MWW_LD_PATH "${CMAKE_LINKER}")
	else()
		execute_process(
			COMMAND ${CMAKE_C_COMPILER} -print-prog-name=ld
			OUTPUT_VARIABLE MWW_PROG_LD
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
		if(EXISTS "${MWW_PROG_LD}")
			set(MWW_LD_PATH "${MWW_PROG_LD}")
		elseif(EXISTS "${MWW_TOOLCHAIN_ROOT}-ld")
			set(MWW_LD_PATH "${MWW_TOOLCHAIN_ROOT}-ld")
		endif()
	endif()

	if(NOT "${MWW_LD_PATH}" STREQUAL "")
		llext_link_options(mww
			--target=xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf
			--ld-path=${MWW_LD_PATH}
		)
	endif()
endif()
