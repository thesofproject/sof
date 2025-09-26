# Copyright (c) 2025 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0

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

add_library(nn_hifi_lib STATIC
	${NN_HIFI_PATH}/algo/common/src/xa_nnlib_common_api.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_pad_8.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_stride_slice_int16.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_batch_to_space_nd_8.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_transpose_8.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_depth_to_space_8.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_pad_16.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_stride_slice_int32.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_space_to_batch_nd_8.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_pad_32.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_stride_slice_int8.c
	${NN_HIFI_PATH}/algo/kernels/reorg/hifi4/xa_nn_space_to_depth_8.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_32_32.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_f32_f32.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_sym16_sym16.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_softmax_asym8_asym8.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_32_16.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_16_16.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_8_8.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_asym16_asym16.c
	${NN_HIFI_PATH}/algo/kernels/activations/hifi4/xa_nn_activations_32_8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_16x16_16_circ_nb.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_16x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_16x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_transpose_conv_circ_buf.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_group_sym8sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_asym8xasym8_asym8_circ_nb.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_sym8sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x8_8_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_sym8sxsym16s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_8x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_circ_buf.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_16x16_16_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_transpose_conv_f32.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_f32.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_f32_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_circ_buf.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_transpose_conv_sym8sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x16_16_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_sym8sxsym16s_sym16s_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_8x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_sym8sxsym16s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_f32.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_f32_circ_nb.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_sym8sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_asym8xasym8_asym8_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_sym8sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_transpose_conv_sym8sxsym16s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_f32.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_8x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_8x8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_16x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_sym8sxasym8s_asym8s_circ.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_pointwise_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_circ_buf.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x16_16_circ_nb.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_16x16.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_matXvec_8x8_8_circ_nb.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_std_sym4sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv1d_std_f32.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_sym8sxsym16s.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/cnn/hifi4/xa_nn_conv2d_depthwise_8x16.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matmul_f32.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_asym8xasym8_batch.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matmul_sym8sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matmul_8x8.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matmul_8x16.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_asym4sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matmul_sym8sxsym16s.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_f32_batch.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_f32.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_16x16_batch.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_sym8sxsym16s.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matmul_asym8sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_16x8.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_8x16.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_8x8.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_asym8sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matmul_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matmul_16x16.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_8x16_batch.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_16x16.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_asym8xasym8.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_sym8sxasym8s.c
	${NN_HIFI_PATH}/algo/kernels/matXvec/hifi4/xa_nn_matXvec_8x8_batch.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_maxpool_f32.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_maxpool_8.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_maxpool_asym8_nhwc.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_avgpool_f32_nhwc.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_maxpool_16_nhwc.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_avgpool_16.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_avgpool_16_nhwc.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_avgpool_asym8.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_avgpool.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_inv_256_tbl.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_maxpool.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_maxpool_8_nhwc.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_maxpool_16.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_avgpool_8.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_maxpool_f32_nhwc.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_avgpool_asym8_nhwc.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_avgpool_f32.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_maxpool_asym8.c
	${NN_HIFI_PATH}/algo/kernels/pool/hifi4/xa_nn_avgpool_8_nhwc.c
	${NN_HIFI_PATH}/algo/kernels/norm/hifi4/xa_nn_l2_norm_asym8s.c
	${NN_HIFI_PATH}/algo/kernels/norm/hifi4/xa_nn_l2_norm_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_sqrt_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_mul_16x16.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_add_quant8.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_mul_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_lstm_utils.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_dot_prod_16x16.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_sub_quant16.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_round_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_mul_acc_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_cosine_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_reduce_asym8s_asym8s.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_memset_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_floor_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_squared_diff_quant8.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_add_quant16.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_reduce_asym16s_asym16s.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_minmax_8.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_logn_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_div_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_sub_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_compare_quant8.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_logical_bool.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_broadcast_8_8.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_quantize.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_neg_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_mul_quant16.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_add_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_vec_interpolation_q15.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_square_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_sine_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_abs_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_memmove.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_mul_quant8.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_squared_diff_quant16.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_memmove_16.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_sub_quant8.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_ceil_f32.c
	${NN_HIFI_PATH}/algo/kernels/basic/hifi4/xa_nn_elm_rsqrt_f32.c
	${NN_HIFI_PATH}/algo/kernels/fc/hifi4/xa_nn_fully_connected.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_tanh32x32_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/scl_sigmoidf_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/inv2pif_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/nanf_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/pow2f_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_sigmoidf_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_reluf_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/expf_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_tanhf_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_sigmoid32x32_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/lognf_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/sinf_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_cosinef_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_sinef_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_softmaxf_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/scl_tanhf_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_softmax32x32_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_lognf_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/sqrt2f_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/inff_tbl.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_alognf_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/vec_relu32x32_hifi4.c
	${NN_HIFI_PATH}/algo/ndsp/hifi4/src/tanhf_tbl.c
)

target_include_directories(nn_hifi_lib PRIVATE
	${NN_HIFI_PATH}/include
	${NN_HIFI_PATH}/algo/common/include/
	${NN_HIFI_PATH}/include/nnlib/
	${NN_HIFI_PATH}/algo/ndsp/hifi4/include
)

# TODO: Need to detect and add mul16/32 options.
#ifeq "$(has_mul16)" "0"
#CFLAGS += -mno-mul16
#endif
#ifeq "$(has_mul32)" "0"
#CFLAGS += -mno-mul32 -mno-div32
#endif
target_compile_options(nn_hifi_lib PRIVATE
	-fsigned-char
	-fno-exceptions
	-mlongcalls
	-INLINE:requested
	-mcoproc
	-fno-zero-initialized-in-bss
	-mtext-section-literals
	-Wsign-compare
	-m32
	-DMODEL_INT16
	-DNNLIB_V2
	-Dhifi4
	-DTFLITE_SINGLE_ROUNDING=1
)

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
	# xtensa kernels
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/sub.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/conv_hifi.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/pooling_vision.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/softmax_vision.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/unidirectional_sequence_lstm.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/transpose_conv.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/fully_connected_common_xtensa.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/lstm_eval_hifi.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/add_vision.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/depthwise_conv.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/reshape.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/softmax.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/depthwise_conv_hifi.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/conv_int16_reference.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/conv_int8_reference.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/fully_connected_int8.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/pad_vision.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/fully_connected_vision.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/conv_int8_int16.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/conv.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/softmax_int8_int16.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/logistic.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/reduce.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/lstm_eval.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/reshape_vision.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/pad.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/reduce_vision.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/dequantize.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/add.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/pooling_int8.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/depthwise_conv_vision.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/strided_slice.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/conv_common_xtensa.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/svdf.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/fully_connected.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/leaky_relu.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/quantize.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/pooling.cc
	${TFLM_PATH}/tensorflow/lite/micro/kernels/xtensa/conv_vision.cc
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
)

# TODO: Need to detect and add mul16/32 options.
#ifeq "$(has_mul16)" "0"
#CFLAGS += -mno-mul16
#endif
#ifeq "$(has_mul32)" "0"
#CFLAGS += -mno-mul32 -mno-div32
#endif
target_compile_options(tflm_lib PRIVATE
	-std=c++17
	-stdlib=libc++
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
	--xtensa-core=ace10_LX7HiFi4_2022_10
	-mcoproc
	-DHIFI4
	-mlongcalls
	-DNNLIB_V2
	-Wno-shadow
)

add_local_sources(sof tflm-classify.c llext-wrap.c)

# Need to link libc++ and libc after tflm and nnlib
zephyr_link_libraries(tflm_lib nn_hifi_lib c++ c)
zephyr_include_directories(${TFLM_PATH})
