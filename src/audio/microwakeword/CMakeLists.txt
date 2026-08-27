# Copyright (c) 2026 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

# Newer xt-clang LLVMs accept this to keep literals in .rodata; standard
# Clang or older xt-clang rejects the sub-option. Detect it.
set(MWW_TEXT_SECTION_LITERALS_FALSE_FLAG "")
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-mllvm --text-section-literals=false"
    MWW_HAS_MLLVM_TEXT_SECTION_LITERALS_FALSE)
  if(MWW_HAS_MLLVM_TEXT_SECTION_LITERALS_FALSE)
    set(MWW_TEXT_SECTION_LITERALS_FALSE_FLAG -mllvm --text-section-literals=false)
    add_compile_options(${MWW_TEXT_SECTION_LITERALS_FALSE_FLAG})
  endif()
endif()

# are we building the llext module ?
if(CONFIG_COMP_MWW STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/mww_llext)
  add_dependencies(app mww)
  return()
endif()

# Own dedicated static-build targets (mww_tflm_lib/mww_nn_hifi_lib), distinct
# from src/audio/tensorflow/'s tflm_lib/nn_hifi_lib -- this model needs a
# materially different TFLM kernel set (resource variables, CALL_ONCE,
# Concatenation, StridedSlice, Logistic, Quantize) that tflmcly's build does
# not compile in, so the two components do not share libraries yet (see
# plan Stage 10 for a deferred shared-library refactor).
set(MWW_HAVE_NNLIB_HIFI4 FALSE)
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" AND CONFIG_XTENSA_HIFI4)
  set(MWW_HAVE_NNLIB_HIFI4 TRUE)
endif()

set(NN_HIFI_PATH ${sof_top_dir}/../nnlib-hifi4/xa_nnlib)

# paths for dependencies
set(TFLM_PATH ${sof_top_dir}/../tflite-micro)
set(FLATBUFFERS_PATH ${sof_top_dir}/../flatbuffers)
set(GEMMLOWP_PATH ${sof_top_dir}/../gemmlowp)
set(RUY_PATH ${sof_top_dir}/../ruy)

set(MWW_TOOLCHAIN_INCLUDE_ROOT
	${ZEPHYR_SDK_INSTALL_DIR}/gnu/xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf/xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf/include)
set(MWW_HAL_SOC_PATH ${sof_top_dir}/../modules/hal/xtensa/zephyr/soc/${SOC_TOOLCHAIN_NAME})

if(MWW_HAVE_NNLIB_HIFI4)

add_library(mww_nn_hifi_lib STATIC
	${NN_HIFI_PATH}/algo/common/src/xa_nnlib_common_api.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_32_32.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_f32_f32.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/nanf_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/inff_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/pow2f_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/expf_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/scl_sigmoidf_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_sigmoidf_hifi4.c

	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x8_8_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_8x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_circ_buf.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_16x16_16_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_circ_buf.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x16_16_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_8x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_16x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/fc/hifi4/xa_nn_fully_connected.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_inv_256_tbl.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_transpose_8.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_pad_8.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_stride_slice_int8.c
)

target_include_directories(mww_nn_hifi_lib PRIVATE
	${NN_HIFI_PATH}/include
	${NN_HIFI_PATH}/algo/common/include/
	${NN_HIFI_PATH}/include/nnlib/
	${NN_HIFI_PATH}/algo/ndsp/hifi4/include
	${MWW_TOOLCHAIN_INCLUDE_ROOT}
	${MWW_HAL_SOC_PATH}
)

if(MWW_HAS_MLLVM_TEXT_SECTION_LITERALS_FALSE)
  target_compile_options(mww_nn_hifi_lib PRIVATE
    ${MWW_TEXT_SECTION_LITERALS_FALSE_FLAG}
  )
endif()

target_compile_definitions(mww_nn_hifi_lib PRIVATE
	-DHIFI4=1
	-DHAVE_VFPU=1
	-DHAVE_VFPU_SINGLE_PRECISION=1
	-DXCHAL_HAVE_HIFI4=1
	-DXCHAL_HAVE_HIFI4_VFPU=1

	__xtensa__=1
	__XTENSA__=1
	__XCC__
	__XCC_CLANG__
	"XT_MAX(a,b)=((a)>(b)?(a):(b))"
	"XT_MIN(a,b)=((a)<(b)?(a):(b))"
	"AE_MOVINT16_FROMINT32(v)=((ae_int16)(v))"
	"AE_CVT64F32_H(v)=((ae_int64)(int64_t)(int32_t)AE_MOVAD32_H(v))"
	"AE_CVT64F32_L(v)=((ae_int64)(int64_t)(int32_t)AE_MOVAD32_L(v))"
)

target_compile_options(mww_nn_hifi_lib PRIVATE
	-fsigned-char
	-fno-exceptions
	-mlongcalls
	-fno-zero-initialized-in-bss
	-Wsign-compare
	-DMODEL_INT16
	-DNNLIB_V2
	-Dhifi4
	-DTFLITE_SINGLE_ROUNDING=1
	-mcpu=${SOC_TOOLCHAIN_NAME}
	"SHELL:-include xtensahifiintrin.h"
	"SHELL:-include ${NN_HIFI_PATH}/algo/common/include/xa_nnlib_hifi_isa_compat.h"
)

endif() # MWW_HAVE_NNLIB_HIFI4

# TFLM kernel sources beyond what tensorflow/CMakeLists.txt currently builds:
# the hey_jarvis.tflite MixConv streaming graph uses CALL_ONCE, VAR_HANDLE,
# READ_VARIABLE, ASSIGN_VARIABLE, CONCATENATION, STRIDED_SLICE, LOGISTIC and
# QUANTIZE in addition to CONV_2D/DEPTHWISE_CONV_2D/FULLY_CONNECTED/RESHAPE
# (confirmed by direct flatbuffer inspection, see plan Stage 1).
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
	# reference kernel implementations for the ops above
	${TFLM_PATH}/tensorflow/lite/micro/kernels/conv.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/depthwise_conv.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/fully_connected.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/logistic.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/quantize.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/reshape.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/strided_slice.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/dequantize.cc
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
	${TFLM_PATH}/tensorflow/lite/micro/test_helper_custom_ops.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_context.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_log.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_profiler.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_time.cc
	# debug_log.cc is replaced by our printk-backed DebugLog() in mww.c.
	#${TFLM_PATH}/tensorflow/lite/micro/debug_log.cc
	${TFLM_PATH}/tensorflow/lite/micro/test_helpers.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_op_resolver.cc
	${TFLM_PATH}/tensorflow/lite/micro/recording_micro_allocator.cc
	${TFLM_PATH}/tensorflow/lite/micro/memory_helpers.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_allocation_info.cc
	${TFLM_PATH}/tensorflow/lite/micro/memory_planner/greedy_memory_planner.cc
	${TFLM_PATH}/tensorflow/lite/micro/memory_planner/non_persistent_buffer_planner_shim.cc
	${TFLM_PATH}/tensorflow/lite/micro/memory_planner/linear_memory_planner.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_utils.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_interpreter.cc
	mww_model_data.cc
	mww_model.cc
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
if(MWW_HAVE_NNLIB_HIFI4)
target_include_directories(mww_tflm_lib PRIVATE
	${NN_HIFI_PATH}
	${NN_HIFI_PATH}/include
)
endif()

if(MWW_HAVE_NNLIB_HIFI4)
target_compile_definitions(mww_tflm_lib PRIVATE
	-DHIFI4=1
	-DHAVE_VFPU=1
	-DHAVE_VFPU_SINGLE_PRECISION=1
	-DXCHAL_HAVE_HIFI4=1
	-DXCHAL_HAVE_HIFI4_VFPU=1

	__xtensa__=1
	__XTENSA__=1
	__XCC__
	__XCC_CLANG__
	"XT_MAX(a,b)=((a)>(b)?(a):(b))"
	"XT_MIN(a,b)=((a)<(b)?(a):(b))"
	"AE_MOVINT16_FROMINT32(v)=((ae_int16)(v))"
	"AE_CVT64F32_H(v)=((ae_int64)(int64_t)(int32_t)AE_MOVAD32_H(v))"
	"AE_CVT64F32_L(v)=((ae_int64)(int64_t)(int32_t)AE_MOVAD32_L(v))"
)
target_compile_options(mww_tflm_lib PRIVATE
	-DHIFI4
	-DKERNELS_OPTIMIZED_FOR_SPEED
	-DNNLIB_V2
)
endif() # MWW_HAVE_NNLIB_HIFI4

target_compile_options(mww_tflm_lib PRIVATE
	-std=c++17
	-fno-rtti
	-fno-exceptions
	-fno-threadsafe-statics
	-Wnon-virtual-dtor
	-fno-unwind-tables
	-fmessage-length=0
	-DTF_LITE_STATIC_MEMORY
	-DTF_LITE_DISABLE_X86_NEON
	-Wsign-compare
	-Wdouble-promotion
	-Wunused-variable
	-Wswitch
	-Wvla
	-Wall
	-Wextra
	-Wmissing-field-initializers
	-Wstrict-aliasing
	-Wno-unused-parameter
	-DXTENSA
	-DTF_LITE_MCU_DEBUG_LOG
	-DTF_LITE_USE_CTIME
	-mlongcalls
	-Wno-shadow
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|Xtensa" OR CMAKE_C_COMPILER MATCHES "xt-clang")
	target_compile_options(mww_tflm_lib PRIVATE
		-stdlib=libc++
		"SHELL:-include xtensahifiintrin.h"
		-fno-vectorize
		-fno-slp-vectorize
	)
else()
	target_compile_options(mww_tflm_lib PRIVATE
		-fno-tree-vectorize
		-fno-tree-slp-vectorize
	)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Xtensa")
	target_compile_options(mww_tflm_lib PRIVATE
		--xtensa-core=ace10_LX7HiFi4_2022_10
		-mcoproc
	)
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
	target_compile_options(mww_tflm_lib PRIVATE
		-mcpu=${SOC_TOOLCHAIN_NAME}
	)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	target_include_directories(mww_tflm_lib SYSTEM PRIVATE
		${MWW_TOOLCHAIN_INCLUDE_ROOT}/c++/14.3.0
		${MWW_TOOLCHAIN_INCLUDE_ROOT}/c++/14.3.0/xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf
		${MWW_TOOLCHAIN_INCLUDE_ROOT}
	)
endif()

add_local_sources(sof mww.c)

if(MWW_HAVE_NNLIB_HIFI4)
  zephyr_link_libraries(mww_tflm_lib mww_nn_hifi_lib)
else()
  zephyr_link_libraries(mww_tflm_lib)
endif()

# Same libc-shim rationale as src/audio/tensorflow/CMakeLists.txt: SOF's
# default CONFIG_MINIMAL_LIBC is missing abs()/libm symbols TFLM needs in a
# statically-linked (non-LLEXT) image; extract just those archive members
# from the toolchain's libc.a into a small private archive.
if(NOT CONFIG_COMP_MWW STREQUAL "m")
  set(MWW_TOOLCHAIN_LIBC_ARCHIVE
    ${ZEPHYR_SDK_INSTALL_DIR}/gnu/xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf/xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf/lib/libc.a)
  set(MWW_LIBC_SHIM_MEMBERS
    libc_stdlib_abs.c.o
    libm_math_s_floor.c.o
    libm_math_sf_exp.c.o
    libm_math_sf_log.c.o
    libm_math_sf_ceil.c.o
    libm_math_s_frexp.c.o
    libm_common_s_round.c.o
    libm_common_sf_fmax.c.o
    libm_common_sf_fmin.c.o
    libm_common_sf_round.c.o
    libm_common_sf_isnan.c.o
    libm_common_sf_issignaling.c.o
    libm_common_math_errf_uflowf.c.o
    libm_common_math_errf_oflowf.c.o
    libm_common_math_errf_divzerof.c.o
    libm_common_math_errf_invalidf.c.o
  )
  set(MWW_LIBC_SHIM_DIR ${CMAKE_CURRENT_BINARY_DIR}/mww_libc_shim)
  set(MWW_LIBC_SHIM_ARCHIVE ${CMAKE_CURRENT_BINARY_DIR}/libmww_libc_shim.a)
  file(MAKE_DIRECTORY ${MWW_LIBC_SHIM_DIR})
  add_custom_command(
    OUTPUT ${MWW_LIBC_SHIM_ARCHIVE}
    COMMAND ${CMAKE_AR} x ${MWW_TOOLCHAIN_LIBC_ARCHIVE} ${MWW_LIBC_SHIM_MEMBERS}
    COMMAND ${CMAKE_AR} rcs ${MWW_LIBC_SHIM_ARCHIVE} ${MWW_LIBC_SHIM_MEMBERS}
    WORKING_DIRECTORY ${MWW_LIBC_SHIM_DIR}
    DEPENDS ${MWW_TOOLCHAIN_LIBC_ARCHIVE}
    COMMENT "Extracting abs()/libm members TFLM needs from the toolchain libc.a"
  )
  add_custom_target(mww_libc_shim_gen DEPENDS ${MWW_LIBC_SHIM_ARCHIVE})
  add_library(mww_libc_shim STATIC IMPORTED GLOBAL)
  set_target_properties(mww_libc_shim PROPERTIES IMPORTED_LOCATION ${MWW_LIBC_SHIM_ARCHIVE})
  add_dependencies(mww_libc_shim mww_libc_shim_gen)
  zephyr_link_libraries(mww_libc_shim)

  target_compile_definitions(mww_tflm_lib PRIVATE NDEBUG)
endif()
zephyr_include_directories(${TFLM_PATH})
zephyr_include_directories(${FLATBUFFERS_PATH}/include)
zephyr_include_directories(${GEMMLOWP_PATH})
zephyr_include_directories(${RUY_PATH})
if(MWW_HAVE_NNLIB_HIFI4)
zephyr_include_directories(${NN_HIFI_PATH}/algo/kernels/include)
zephyr_include_directories(${NN_HIFI_PATH}/include)
endif()
target_include_directories(modules_sof SYSTEM PRIVATE
	${MWW_TOOLCHAIN_INCLUDE_ROOT}/c++/14.3.0
	${MWW_TOOLCHAIN_INCLUDE_ROOT}/c++/14.3.0/xtensa-${SOC_TOOLCHAIN_NAME}_zephyr-elf
	${MWW_TOOLCHAIN_INCLUDE_ROOT}
)
