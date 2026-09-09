# Copyright (c) 2025 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

# -mllvm --text-section-literals=false is a Clang-only flag; guard it
if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-mllvm --text-section-literals=false)
endif()

# are we building the llext module ?
if(CONFIG_COMP_TENSORFLOW STREQUAL "m" AND DEFINED CONFIG_LLEXT)
  add_subdirectory(llext ${PROJECT_BINARY_DIR}/tflm_llext)
  add_dependencies(app tflm)
  return()
endif()

# TODO: detect Hifi4/5 for NN lib kernels
set(NN_HIFI_PATH ${sof_top_dir}/../nnlib-hifi4/xa_nnlib)

# paths for dependencies
set(TFLM_PATH ${sof_top_dir}/../tflite-micro)
set(FLATBUFFERS_PATH ${sof_top_dir}/../flatbuffers)
set(GEMMLOWP_PATH ${sof_top_dir}/../gemmlowp)
set(RUY_PATH ${sof_top_dir}/../ruy)

# nnlib-hifi4 requires Cadence XCC/Clang-specific TIE headers (xtensa/tie/xt_hifi2.h)
# and therefore can ONLY be built with Clang. Skip entirely when using gcc.
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")

add_library(nn_hifi_lib STATIC
	${NN_HIFI_PATH}/algo/common/src/xa_nnlib_common_api.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_32_32.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_f32_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_add_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_sub_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_mul_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_abs_f32.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/nanf_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/inff_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/pow2f_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/expf_tbl.c

	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x8_8_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_8x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_circ_buf.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_16x16_16_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_circ_buf.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x16_16_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_8x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_8x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_16x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_circ_buf.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x16_16_circ_nb.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_16x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x8_8_circ_nb.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/fc/hifi4/xa_nn_fully_connected.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_inv_256_tbl.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_transpose_8.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_pad_8.c
)

target_include_directories(nn_hifi_lib PRIVATE
	${NN_HIFI_PATH}/include
	${NN_HIFI_PATH}/algo/common/include/
	${NN_HIFI_PATH}/include/nnlib/
	${NN_HIFI_PATH}/algo/ndsp/hifi4/include
	/home/lrg/zephyr-sdk-1.0.1/gnu/xtensa-intel_ace15_mtpm_zephyr-elf/xtensa-intel_ace15_mtpm_zephyr-elf/include
	${sof_top_dir}/../modules/hal/xtensa/zephyr/soc/intel_ace15_mtpm
)

target_compile_options(nn_hifi_lib PRIVATE
  -mllvm --text-section-literals=false
)

endif() # CMAKE_C_COMPILER_ID STREQUAL "Clang"


if(CMAKE_C_COMPILER_ID STREQUAL "Clang")

# TODO: Need to detect and add mul16/32 options.
target_compile_definitions(nn_hifi_lib PRIVATE
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

target_compile_options(nn_hifi_lib PRIVATE
	-fsigned-char
	-fno-exceptions
	-mlongcalls
	-fno-zero-initialized-in-bss
	-Wsign-compare
	-DMODEL_INT16
	-DNNLIB_V2
	-Dhifi4
	-DTFLITE_SINGLE_ROUNDING=1
)

target_compile_definitions(nn_hifi_lib PRIVATE
	__XCC__
	__XCC_CLANG__
)
target_compile_options(nn_hifi_lib PRIVATE
	-mcpu=intel_ace15_mtpm
	"SHELL:-include xtensahifiintrin.h"
	"SHELL:-include ${NN_HIFI_PATH}/algo/common/include/xa_nnlib_hifi_isa_compat.h"
)

endif() # CMAKE_C_COMPILER_ID STREQUAL "Clang" (nn_hifi_lib defs/opts)


# TODO: complete sources have been added here from userspace build but
# not all are needed so this is a list of "needed" sources to build
# a memory and performance optimized TFLM for SOF.
# All sources have been left in for completeness now, but will be removed
# when support is mature.
add_library(tflm_lib STATIC
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
	# Stock micro kernels - not needed atm
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/activations_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/activations.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/add_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/add_n.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/arg_min_max.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/assign_variable.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/batch_matmul.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/batch_to_space_nd.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/broadcast_args.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/broadcast_to.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/call_once.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/cast.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/ceil.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/circular_buffer_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/circular_buffer.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/comparisons.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/concatenation.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/conv_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/cumsum.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/depth_to_space.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/depthwise_conv_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/dequantize_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/detection_postprocess.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/div.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/elementwise.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/elu.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/embedding_lookup.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/ethosu.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/expand_dims.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/exp.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/fill.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/floor_div.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/floor_mod.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/floor.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/fully_connected_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/gather_nd.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/gather.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/hard_swish_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/hard_swish.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/if.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/kernel_runner.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/kernel_util.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/l2norm.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/l2_pool_2d.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/leaky_relu_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/logical_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/logical.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/logistic_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/log_softmax.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/lstm_eval_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/maximum_minimum.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/micro_tensor_utils.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/mirror_pad.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/mul_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/mul.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/neg.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/pack.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/pooling_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/prelu_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/prelu.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/quantize_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/read_variable.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/reduce_common.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/reshape_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/resize_bilinear.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/resize_nearest_neighbor.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/round.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/select.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/shape.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/slice.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/softmax_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/space_to_batch_nd.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/space_to_depth.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/split.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/split_v.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/squared_difference.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/squeeze.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/strided_slice_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/sub_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/svdf_common.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/tanh.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/transpose.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/unpack.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/var_handle.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/while.cc
	#${TFLM_PATH}/tensorflow/lite/micro/kernels/zeros_like.cc
	# reference kernels for speech model ops
	${TFLM_PATH}/tensorflow/lite/micro/kernels/conv.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/depthwise_conv.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/fully_connected.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/reshape.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/softmax.cc
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
	#${TFLM_PATH}/tensorflow/lite/micro/span_test.cc
	${TFLM_PATH}/tensorflow/lite/micro/test_helper_custom_ops.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_context.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_log.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_profiler.cc
	${TFLM_PATH}/tensorflow/lite/micro/micro_time.cc
	#${TFLM_PATH}/tensorflow/lite/micro/static_vector_test.cc
	${TFLM_PATH}/tensorflow/lite/micro/debug_log.cc
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
	micro_speech_quantized_model_data.cc
	speech.cc
)

target_include_directories(tflm_lib PRIVATE
	${TFLM_PATH}
	${FLATBUFFERS_PATH}/include
	${GEMMLOWP_PATH}
	${RUY_PATH}
	${NN_HIFI_PATH}
	${NN_HIFI_PATH}/include
	${sof_top_dir}/posix/include
	${sof_top_dir}/../modules/hal/xtensa/include
	${sof_top_dir}/../modules/hal/xtensa/zephyr/soc/intel_ace15_mtpm
)

# TODO: Need to detect and add mul16/32 options.
#ifeq "$(has_mul16)" "0"
#CFLAGS += -mno-mul16
#endif
#ifeq "$(has_mul32)" "0"
#CFLAGS += -mno-mul32 -mno-div32
#endif
target_compile_definitions(tflm_lib PRIVATE
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

target_compile_options(tflm_lib PRIVATE
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
	-DKERNELS_OPTIMIZED_FOR_SPEED
	-DTF_LITE_MCU_DEBUG_LOG
	-DTF_LITE_USE_CTIME
	-DHIFI4
	-mlongcalls
	-DNNLIB_V2
	-Wno-shadow
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(tflm_lib PRIVATE
		-stdlib=libc++
		"SHELL:-include xtensahifiintrin.h"
		-fno-vectorize
		-fno-slp-vectorize
	)
else()
	# gcc equivalents for vectorize disabling
	target_compile_options(tflm_lib PRIVATE
		-fno-tree-vectorize
		-fno-tree-slp-vectorize
	)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "Xtensa")
	target_compile_options(tflm_lib PRIVATE
		--xtensa-core=ace10_LX7HiFi4_2022_10
		-mcoproc
	)
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
	target_compile_options(tflm_lib PRIVATE
		-mcpu=intel_ace15_mtpm
	)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	target_include_directories(tflm_lib SYSTEM PRIVATE
		/home/lrg/zephyr-sdk-1.0.1/gnu/xtensa-intel_ace15_mtpm_zephyr-elf/xtensa-intel_ace15_mtpm_zephyr-elf/include/c++/14.3.0
		/home/lrg/zephyr-sdk-1.0.1/gnu/xtensa-intel_ace15_mtpm_zephyr-elf/xtensa-intel_ace15_mtpm_zephyr-elf/include/c++/14.3.0/xtensa-intel_ace15_mtpm_zephyr-elf
		/home/lrg/zephyr-sdk-1.0.1/gnu/xtensa-intel_ace15_mtpm_zephyr-elf/xtensa-intel_ace15_mtpm_zephyr-elf/include
	)
endif()

add_local_sources(sof tflm-classify.c speech.cc micro_speech_quantized_model_data.cc llext-wrap.c math_stubs.c)

# Link nnlib only when Clang is available (nnlib requires XCC/Clang TIE headers)
if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
  zephyr_link_libraries(tflm_lib nn_hifi_lib)
else()
  zephyr_link_libraries(tflm_lib)
endif()
zephyr_include_directories(${TFLM_PATH})
zephyr_include_directories(${FLATBUFFERS_PATH}/include)
zephyr_include_directories(${GEMMLOWP_PATH})
zephyr_include_directories(${RUY_PATH})
zephyr_include_directories(${NN_HIFI_PATH}/algo/kernels/include)
zephyr_include_directories(${NN_HIFI_PATH}/include)
target_include_directories(modules_sof SYSTEM PRIVATE
	/home/lrg/zephyr-sdk-1.0.1/gnu/xtensa-intel_ace15_mtpm_zephyr-elf/xtensa-intel_ace15_mtpm_zephyr-elf/include/c++/14.3.0
	/home/lrg/zephyr-sdk-1.0.1/gnu/xtensa-intel_ace15_mtpm_zephyr-elf/xtensa-intel_ace15_mtpm_zephyr-elf/include/c++/14.3.0/xtensa-intel_ace15_mtpm_zephyr-elf
	/home/lrg/zephyr-sdk-1.0.1/gnu/xtensa-intel_ace15_mtpm_zephyr-elf/xtensa-intel_ace15_mtpm_zephyr-elf/include
)

