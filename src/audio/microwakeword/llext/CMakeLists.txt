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
if(DEFINED ZEPHYR_SDK_INSTALL_DIR AND NOT "${ZEPHYR_SDK_INSTALL_DIR}" STREQUAL "")
	set(MWW_TOOLCHAIN_ROOT
		${ZEPHYR_SDK_INSTALL_DIR}/gnu/xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf/xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf)
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
	${TFLM_PATH}/tensorflow/lite/kernels/internal/reference/comparisons.cc
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
		if(NOT "${MWW_TOOLCHAIN_ROOT}" STREQUAL "")
			target_include_directories(mww_tflm_lib SYSTEM PRIVATE
				${MWW_TOOLCHAIN_ROOT}/include/c++/14.3.0
				${MWW_TOOLCHAIN_ROOT}/include/c++/14.3.0/xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf
				${MWW_TOOLCHAIN_ROOT}/include
			)
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

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT CMAKE_C_COMPILER MATCHES "xt-clang" AND NOT "${MWW_TOOLCHAIN_ROOT}" STREQUAL "")
	llext_link_options(mww
		--target=xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf
		--ld-path=${MWW_TOOLCHAIN_ROOT}-ld
	)
endif()
