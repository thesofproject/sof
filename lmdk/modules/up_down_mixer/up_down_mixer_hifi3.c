// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2022 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@intel.com>

#include <../include/up_down_mixer.h>

#include <xtensa/tie/xt_hifi3.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void upmix32bit_1_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	channel_map out_channel_map = cd->out_channel_map;

	/* Only load the channel if it's present. */
	ae_int32 *output_left = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LEFT) << 2));
	ae_int32 *output_center = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_CENTER) << 2));
	ae_int32 *output_right = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_RIGHT) << 2));
	ae_int32 *output_left_surround = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LEFT_SURROUND) << 2));
	ae_int32 *output_right_surround = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_RIGHT_SURROUND) << 2));
	ae_int32 *output_lfe = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LFE) << 2));

	ae_int32 *in_ptr = (ae_int32 *)in_data;

	for (i = 0; i < (in_size >> 2); ++i) {
		output_left[i * 6] = in_ptr[i];
		output_right[i * 6] = in_ptr[i];
		output_center[i * 6] = 0;
		output_left_surround[i * 6] = in_ptr[i];
		output_right_surround[i * 6] = in_ptr[i];
		output_lfe[i * 6] = 0;
	}
}

void upmix16bit_1_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	channel_map out_channel_map = cd->out_channel_map;

	/* Only load the channel if it's present. */
	ae_int32 *output_left = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LEFT) << 2));
	ae_int32 *output_center = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_CENTER) << 2));
	ae_int32 *output_right = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_RIGHT) << 2));
	ae_int32 *output_left_surround = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LEFT_SURROUND) << 2));
	ae_int32 *output_right_surround = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_RIGHT_SURROUND) << 2));
	ae_int32 *output_lfe = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LFE) << 2));

	ae_int16 *in_ptr = (ae_int16 *)in_data;

	for (i = 0; i < (in_size >> 1); ++i) {
		output_left[i * 6] = AE_MOVINT32_FROMINT16(in_ptr[i]) << 16;
		output_right[i * 6] = AE_MOVINT32_FROMINT16(in_ptr[i]) << 16;
		output_center[i * 6] = 0;
		output_left_surround[i * 6] = AE_MOVINT32_FROMINT16(in_ptr[i]) << 16;
		output_right_surround[i * 6] = AE_MOVINT32_FROMINT16(in_ptr[i]) << 16;
		output_lfe[i * 6] = 0;
	}
}

void upmix32bit_2_0_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	channel_map out_channel_map = cd->out_channel_map;

	const uint8_t left_slot = get_channel_location(out_channel_map, CHANNEL_LEFT);
	const uint8_t center_slot = get_channel_location(out_channel_map, CHANNEL_CENTER);
	const uint8_t right_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT);
	uint8_t left_surround_slot = get_channel_location(out_channel_map, CHANNEL_LEFT_SURROUND);
	uint8_t right_surround_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT_SURROUND);
	const uint8_t lfe_slot = get_channel_location(out_channel_map, CHANNEL_LFE);

	/* Must support also 5.1 Surround */
	if (left_surround_slot == CHANNEL_INVALID && right_surround_slot == CHANNEL_INVALID) {
		left_surround_slot = get_channel_location(out_channel_map, CHANNEL_LEFT_SIDE);
		right_surround_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT_SIDE);
	}

	ae_int32 *output_left = (ae_int32 *)(out_data + (left_slot << 2));
	ae_int32 *output_center = (ae_int32 *)(out_data + (center_slot << 2));
	ae_int32 *output_right = (ae_int32 *)(out_data + (right_slot << 2));
	ae_int32 *output_left_surround = (ae_int32 *)(out_data + (left_surround_slot << 2));
	ae_int32 *output_right_surround = (ae_int32 *)(out_data + (right_surround_slot << 2));
	ae_int32 *output_lfe = (ae_int32 *)(out_data + (lfe_slot << 2));

	ae_int32 *in_left_ptr = (ae_int32 *)in_data;
	ae_int32 *in_right_ptr = (ae_int32 *)(in_data + 4);

	for (i = 0; i < (in_size >> 3); ++i) {
		output_left[i * 6] = in_left_ptr[i * 2];
		output_right[i * 6] = in_right_ptr[i * 2];
		output_center[i * 6] = 0;
		output_left_surround[i * 6] = in_left_ptr[i * 2];
		output_right_surround[i * 6] = in_right_ptr[i * 2];
		output_lfe[i * 6] = 0;
	}
}

void upmix16bit_2_0_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	channel_map out_channel_map = cd->out_channel_map;

	const uint8_t left_slot = get_channel_location(out_channel_map, CHANNEL_LEFT);
	const uint8_t center_slot = get_channel_location(out_channel_map, CHANNEL_CENTER);
	const uint8_t right_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT);
	uint8_t left_surround_slot = get_channel_location(out_channel_map, CHANNEL_LEFT_SURROUND);
	uint8_t right_surround_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT_SURROUND);
	const uint8_t lfe_slot = get_channel_location(out_channel_map, CHANNEL_LFE);

	/* Must support also 5.1 Surround */
	if (left_surround_slot == CHANNEL_INVALID && right_surround_slot == CHANNEL_INVALID) {
		left_surround_slot = get_channel_location(out_channel_map, CHANNEL_LEFT_SIDE);
		right_surround_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT_SIDE);
	}

	ae_int32 *output_left = (ae_int32 *)(out_data + (left_slot << 2));
	ae_int32 *output_center = (ae_int32 *)(out_data + (center_slot << 2));
	ae_int32 *output_right = (ae_int32 *)(out_data + (right_slot << 2));
	ae_int32 *output_left_surround = (ae_int32 *)(out_data + (left_surround_slot << 2));
	ae_int32 *output_right_surround = (ae_int32 *)(out_data + (right_surround_slot << 2));
	ae_int32 *output_lfe = (ae_int32 *)(out_data + (lfe_slot << 2));

	ae_int16 *in_left_ptr = (ae_int16 *)in_data;
	ae_int16 *in_right_ptr = (ae_int16 *)(in_data + 2);

	for (i = 0; i < (in_size >> 2); ++i) {
		output_left[i * 6] = AE_MOVINT32_FROMINT16(in_left_ptr[i * 2]) << 16;
		output_right[i * 6] = AE_MOVINT32_FROMINT16(in_right_ptr[i * 2]) << 16;
		output_center[i * 6] = 0;
		output_left_surround[i * 6] = AE_MOVINT32_FROMINT16(in_left_ptr[i * 2]) << 16;
		output_right_surround[i * 6] = AE_MOVINT32_FROMINT16(in_right_ptr[i * 2]) << 16;
		output_lfe[i * 6] = 0;
	}
}

void upmix32bit_2_0_to_7_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	channel_map out_channel_map = cd->out_channel_map;

	/* Only load the channel if it's present. */
	ae_int32 *output_left = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LEFT) << 2));
	ae_int32 *output_center = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_CENTER) << 2));
	ae_int32 *output_right = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_RIGHT) << 2));
	ae_int32 *output_left_surround = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LEFT_SURROUND) << 2));
	ae_int32 *output_right_surround = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_RIGHT_SURROUND) << 2));
	ae_int32 *output_lfe = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LFE) << 2));
	ae_int32 *output_left_side = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_LEFT_SIDE) << 2));
	ae_int32 *output_right_side = (ae_int32 *)(out_data +
		(get_channel_location(out_channel_map, CHANNEL_RIGHT_SIDE) << 2));

	ae_int32 *in_left_ptr = (ae_int32 *)in_data;
	ae_int32 *in_right_ptr = (ae_int32 *)(in_data + 4);

	for (i = 0; i < (in_size >> 3); ++i) {
		output_left[i * 8] = in_left_ptr[i * 2];
		output_right[i * 8] = in_right_ptr[i * 2];
		output_center[i * 8] = 0;
		output_left_surround[i * 8] = in_left_ptr[i * 2];
		output_right_surround[i * 8] = in_right_ptr[i * 2];
		output_lfe[i * 8] = 0;
		output_left_side[i * 8] = 0;
		output_right_side[i * 8] = 0;
	}
}

void shiftcopy32bit_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	size_t i;

	ae_p24f *in_ptr = (ae_p24f *)in_data;
	ae_p24x2f *out_ptr = (ae_p24x2f *)out_data;

	for (i = 0; i < (in_size >> 2); ++i)
		out_ptr[i] = in_ptr[i];
}

void shiftcopy32bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	ae_p24x2f *in_ptr = (ae_p24x2f *)in_data;
	ae_p24x2f *out_ptr = (ae_p24x2f *)out_data;

	for (i = 0; i < (in_size >> 3); ++i)
		out_ptr[i] = in_ptr[i];
}

void downmix32bit_2_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_lfe;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_lfe_tmp =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_lfe = AE_SEL32_LL(P_coefficient_lfe_tmp, P_coefficient_lfe_tmp);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_lfe = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LFE) << 2));

	ae_int32 *output_left = (ae_int32 *)(out_data);
	ae_int32 *output_right = (ae_int32 *)(out_data + sizeof(ae_int32));

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_lfe;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;

	const ae_int32 *const end_input_left = input_left + (in_size / (sizeof(ae_int32)));

	while (input_left < end_input_left) {
		/* update output left channel based on input left channel */
		AE_L32_IP(P_input_left, input_left, 12);
		ae_f64 Q_tmp_left = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		/* update output right channel based on input right channel */
		AE_L32_IP(P_input_right, input_right, 12);
		ae_f64 Q_tmp_right = AE_MULF32S_LL(P_input_right, P_coefficient_left_right);

		/* update output left and right channels based on input lfe channel */
		AE_L32_IP(P_input_lfe, input_lfe, 12);
		AE_MULAF32S_LL(Q_tmp_left, P_input_lfe, P_coefficient_lfe);
		AE_MULAF32S_LL(Q_tmp_right, P_input_lfe, P_coefficient_lfe);

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right);

		AE_S32_L_IP(P_output_left, output_left, 2 * 4);
		AE_S32_L_IP(P_output_right, output_right, 2 * 4);
	}
}

void downmix32bit_3_0(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_center;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center_tmp =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_center = AE_SEL32_LL(P_coefficient_center_tmp, P_coefficient_center_tmp);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_center = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));

	ae_int32 *output_left = (ae_int32 *)(out_data);
	ae_int32 *output_right = (ae_int32 *)(out_data + sizeof(ae_int32));

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;

	const ae_int32 *const end_input_left = input_left + (in_size / (sizeof(ae_int32)));

	while (input_left < end_input_left) {
		ae_f64 Q_tmp_left;
		ae_f64 Q_tmp_right;

		/* update output left channel based on input left channel */
		AE_L32_IP(P_input_left, input_left, 3 * sizeof(ae_int32));
		Q_tmp_left = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		/* update output left and right channels based on input center channel */
		AE_L32_IP(P_input_center, input_center, 3 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp_left, P_input_center, P_coefficient_center);
		Q_tmp_right = AE_MULF32S_LH(P_input_center, P_coefficient_center);

		/* update output right channel based on input right channel */
		AE_L32_IP(P_input_right, input_right, 3 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp_right, P_input_right, P_coefficient_left_right);

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right);

		AE_S32_L_IP(P_output_left, output_left, 2 * sizeof(ae_int32));
		AE_S32_L_IP(P_output_right, output_right, 2 * sizeof(ae_int32));
	}
}

void downmix32bit_3_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_center_lfe;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_center = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_lfe = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LFE) << 2));

	ae_int32 *output_left = (ae_int32 *)(out_data);
	ae_int32 *output_right = (ae_int32 *)(out_data + sizeof(ae_int32));

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_lfe;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;

	const ae_int32 *const end_input_left = input_left + (in_size / (sizeof(ae_int32)));

	while (input_left < end_input_left) {
		ae_f64 Q_tmp_left;
		ae_f64 Q_tmp_right;

		AE_L32_IP(P_input_left, input_left, 4 * sizeof(ae_int32));
		Q_tmp_left = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		AE_L32_IP(P_input_center, input_center, 4 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp_left, P_input_center, P_coefficient_center_lfe);
		Q_tmp_right = AE_MULF32S_LH(P_input_center, P_coefficient_center_lfe);

		AE_L32_IP(P_input_right, input_right, 4 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp_right, P_input_right, P_coefficient_left_right);

		AE_L32_IP(P_input_lfe, input_lfe, 4 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp_left, P_input_lfe, P_coefficient_center_lfe);
		AE_MULAF32S_LL(Q_tmp_right, P_input_lfe, P_coefficient_center_lfe);

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right);

		AE_S32_L_IP(P_output_left, output_left, 2 * sizeof(ae_int32));
		AE_S32_L_IP(P_output_right, output_right, 2 * sizeof(ae_int32));
	}
}

void downmix32bit(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		  const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_left_s_right_s;
	ae_int32x2 P_coefficient_center_lfe;

	/* Load the downmix coefficients. */
	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_left_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SURROUND << 2);
	ae_int32x2 P_coefficient_right_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SURROUND << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);

	/*
	 * We have 6 coefficients (constant inside loop), 6 channels and only 8
	 * AE_P registers. But each of those registers is 48bit or 2x24bit wide.
	 * Also many Hifi2 operations can use either lower or higher 24 bits from
	 * those registers. By combining 6 coefficients in pairs we save 3 AE_P
	 * registers.
	 */
	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_left_s_right_s = AE_SEL32_LL(P_coefficient_left_surround,
						   P_coefficient_right_surround);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);

	/* See what channels are available. */
	bool left = (get_channel_location(cd->in_channel_map, CHANNEL_LEFT) != 0xF);
	bool center = (get_channel_location(cd->in_channel_map, CHANNEL_CENTER) != 0xF);
	bool right = (get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) != 0xF);
	bool left_surround =
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND) != 0xF);
	bool right_surround =
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SURROUND) != 0xF);
	bool lfe = (get_channel_location(cd->in_channel_map, CHANNEL_LFE) != 0xF);

	/* Downmixer single load. */
	ae_int32 *input_left = NULL;
	ae_int32 *input_center = NULL;
	ae_int32 *input_right = NULL;
	ae_int32 *input_left_surround = NULL;
	ae_int32 *input_right_surround = NULL;
	ae_int32 *input_lfe = NULL;

	/* Only load the channel if it's present. */
	if (left) {
		input_left = (ae_int32 *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	}

	if (center) {
		input_center = (ae_int32 *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	}
	if (right) {
		input_right = (ae_int32 *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	}
	if (left_surround) {
		input_left_surround = (ae_int32 *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND) << 2));
	}
	if (right_surround) {
		input_right_surround = (ae_int32 *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SURROUND) << 2));
	}
	if (lfe) {
		input_lfe = (ae_int32 *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_LFE) << 2));
	}

	/** Calculate number of samples in a single channel. */
	uint32_t number_of_samples_in_one_channel = in_size / cd->in_channel_no;

	number_of_samples_in_one_channel >>= 2;

	/* Downmixer single store. */
	ae_int32 *output_left = (ae_int32 *)(out_data);
	ae_int32 *output_right = (ae_int32 *)(out_data + sizeof(ae_int32));

	/* We will be using P & Q registers. */
	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_left_surround;
	ae_int32x2 P_input_right_surround;
	ae_int32x2 P_input_lfe;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;
	/*
	 * Calculating this outside of the loop is significant performance
	 * improvement. The value of this expression was reevaluated in each
	 * * iteration when placed inside loop.
	 */
	int sample_offset = cd->in_channel_no << 2;

	for (i = 0; i < number_of_samples_in_one_channel; i++) {
		/* Zero-out the Q register first. */
		ae_f64 Q_tmp_left = AE_ZERO64();
		ae_f64 Q_tmp_right = AE_ZERO64();

		/* Load one 24-bit value, replicate in two elements of register P. */
		if (left) {
			P_input_left = AE_L32_X(input_left, i * sample_offset);
			AE_MULAF32S_LH(Q_tmp_left, P_input_left, P_coefficient_left_right);
		}
		if (center) {
			P_input_center = AE_L32_X(input_center, i * sample_offset);
			AE_MULAF32S_LH(Q_tmp_left, P_input_center, P_coefficient_center_lfe);
			AE_MULAF32S_LH(Q_tmp_right, P_input_center, P_coefficient_center_lfe);
		}
		if (right) {
			P_input_right = AE_L32_X(input_right, i * sample_offset);
			AE_MULAF32S_LL(Q_tmp_right, P_input_right, P_coefficient_left_right);
		}
		if (left_surround) {
			P_input_left_surround = AE_L32_X(input_left_surround, i * sample_offset);
			AE_MULAF32S_LH(Q_tmp_left, P_input_left_surround,
				       P_coefficient_left_s_right_s);

			if (cd->in_channel_config == IPC4_CHANNEL_CONFIG_4_POINT_0) {
				AE_MULAF32S_LH(Q_tmp_right,
					       P_input_left_surround,
					       P_coefficient_left_s_right_s);
			}
		}
		if (right_surround) {
			P_input_right_surround =
				AE_L32_X(input_right_surround, i * sample_offset);
			AE_MULAF32S_LL(Q_tmp_right, P_input_right_surround,
				       P_coefficient_left_s_right_s);
		}
		if (lfe) {
			P_input_lfe = AE_L32_X(input_lfe, i * sample_offset);
			AE_MULAF32S_LL(Q_tmp_left, P_input_lfe, P_coefficient_center_lfe);
			AE_MULAF32S_LL(Q_tmp_right, P_input_lfe, P_coefficient_center_lfe);
		}

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right);

		output_left[i * 2] = P_output_left;
		output_right[i * 2] = P_output_right;
	}
}

void downmix32bit_4_0(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_left_s_right_s;
	ae_int32x2 P_coefficient_center_lfe;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_left_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SURROUND << 2);
	ae_int32x2 P_coefficient_right_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SURROUND << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);
	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_left_s_right_s = AE_SEL32_LL(P_coefficient_left_surround,
						   P_coefficient_right_surround);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_center = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_left_surround = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND) << 2));

	ae_int32 *output_left = (ae_int32 *)(out_data);
	ae_int32 *output_right = (ae_int32 *)(out_data + sizeof(ae_int32));

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_left_surround;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;

	const ae_int32 *const end_input_left = input_left + (in_size / (sizeof(ae_int32)));

	while (input_left < end_input_left) {
		ae_f64 Q_tmp_left;
		ae_f64 Q_tmp_right;

		AE_L32_IP(P_input_left, input_left, 4 * sizeof(ae_int32));
		Q_tmp_left = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		AE_L32_IP(P_input_center, input_center, 4 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp_left, P_input_center, P_coefficient_center_lfe);
		Q_tmp_right = AE_MULF32S_LH(P_input_center, P_coefficient_center_lfe);

		AE_L32_IP(P_input_right, input_right, 4 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp_right, P_input_right, P_coefficient_left_right);

		/* for 4.0 left surround if propagated to both left and right output channels */
		AE_L32_IP(P_input_left_surround, input_left_surround, 4 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp_left, P_input_left_surround, P_coefficient_left_s_right_s);
		AE_MULAF32S_LH(Q_tmp_right, P_input_left_surround, P_coefficient_left_s_right_s);

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right);

		AE_S32_L_IP(P_output_left, output_left, 2 * sizeof(ae_int32));
		AE_S32_L_IP(P_output_right, output_right, 2 * sizeof(ae_int32));
	}
}

void downmix32bit_5_0_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	size_t i;

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_center_cs;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_cs =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER_SURROUND << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_center_cs = AE_SEL32_LL(P_coefficient_center, P_coefficient_cs);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_center = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_cs = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER_SURROUND) << 2));

	ae_int32 *output = (ae_int32 *)(out_data);

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_cs;

	ae_int32x2 P_output;

	const uint32_t channel_no = 5;

	for (i = 0; i < in_size / (sizeof(ae_int32)); i += channel_no) {
		ae_f64 Q_tmp;

		AE_L32_IP(P_input_left, input_left, 5 * sizeof(ae_int32));
		Q_tmp = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		AE_L32_IP(P_input_center, input_center, 5 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp, P_input_center, P_coefficient_center_cs);

		AE_L32_IP(P_input_right, input_right, 5 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_right, P_coefficient_left_right);

		AE_L32_IP(P_input_cs, input_cs, 5 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_cs, P_coefficient_center_cs);

		P_output = AE_ROUND32F64SSYM(Q_tmp);

		AE_S32_L_IP(P_output, output, sizeof(ae_int32));
	}
}

void downmix32bit_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	const uint8_t left_slot =     get_channel_location(cd->in_channel_map, CHANNEL_LEFT);
	const uint8_t center_slot =   get_channel_location(cd->in_channel_map, CHANNEL_CENTER);
	const uint8_t right_slot =    get_channel_location(cd->in_channel_map, CHANNEL_RIGHT);
	uint8_t left_surround_slot =  get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND);
	uint8_t right_surround_slot = get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SURROUND);
	const uint8_t lfe_slot =      get_channel_location(cd->in_channel_map, CHANNEL_LFE);

	/* Must support also 5.1 Surround */
	const bool surround_5_1_channel_map = (left_surround_slot == CHANNEL_INVALID) &&
					      (right_surround_slot == CHANNEL_INVALID);

	if (surround_5_1_channel_map) {
		left_surround_slot = get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SIDE);
		right_surround_slot = get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SIDE);
	}

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_left_s_right_s;
	ae_int32x2 P_coefficient_center_lfe;

	/* Load the downmix coefficients. */
	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_left_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SURROUND << 2);
	ae_int32x2 P_coefficient_right_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SURROUND << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);

	if (surround_5_1_channel_map) {
		P_coefficient_left_surround  = AE_L32_X((ae_int32 *)cd->downmix_coefficients,
							CHANNEL_LEFT_SIDE << 2);
		P_coefficient_right_surround = AE_L32_X((ae_int32 *)cd->downmix_coefficients,
							CHANNEL_RIGHT_SIDE << 2);
	}

	/*
	 * We have 6 coefficients (constant inside loop), 6 channels and only 8
	 * AE_P registers. But each of those registers is 48bit or 2x24bit wide.
	 * Also many Hifi2 operations can use either lower or higher 24 bits from
	 * those registers. By combining 6 coefficients in pairs we save 3 AE_P
	 * registers.
	 */
	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_left_s_right_s = AE_SEL32_LL(P_coefficient_left_surround,
						   P_coefficient_right_surround);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);

	const ae_int32 *input_left =           (ae_int32 *)(in_data + (left_slot << 2));
	const ae_int32 *input_center =         (ae_int32 *)(in_data + (center_slot << 2));
	const ae_int32 *input_right =          (ae_int32 *)(in_data + (right_slot << 2));
	const ae_int32 *input_left_surround =  (ae_int32 *)(in_data + (left_surround_slot << 2));
	const ae_int32 *input_right_surround = (ae_int32 *)(in_data + (right_surround_slot << 2));
	const ae_int32 *input_lfe =            (ae_int32 *)(in_data + (lfe_slot << 2));

	/* Downmixer single store. */
	ae_int32 *output_left = (ae_int32 *)(out_data);
	ae_int32 *output_right = (ae_int32 *)(out_data + sizeof(ae_int32));

	/* We will be using P & Q registers. */
	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_left_surround;
	ae_int32x2 P_input_right_surround;
	ae_int32x2 P_input_lfe;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;

	/*
	 * We don't need to initialize those registers in loop's body.
	 * Using non accumulating multiplication for the first left,rignt channels
	 * is more efficient.
	 * For non 5.1 version we cannot use this optimization, we don't know
	 * which channel is present and which multiplication should reset output
	 * accumulators.
	 */
	ae_f64 Q_tmp_left = AE_ZERO64();
	ae_f64 Q_tmp_right = AE_ZERO64();

	const ae_int32 *const end_input_left = input_left + (in_size / (sizeof(ae_int32)));

	while (input_left < end_input_left) {
		/* Load one 24-bit value, replicate in two elements of register P. */
		AE_L32_IP(P_input_center, input_center, 6 * sizeof(ae_int32));
		Q_tmp_left = AE_MULF32S_LH(P_input_center, P_coefficient_center_lfe);

		AE_L32_IP(P_input_lfe, input_lfe, 6 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp_left, P_input_lfe, P_coefficient_center_lfe);

		Q_tmp_right = Q_tmp_left;

		AE_L32_IP(P_input_left, input_left, 6 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp_left, P_input_left, P_coefficient_left_right);

		AE_L32_IP(P_input_right, input_right, 6 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp_right, P_input_right, P_coefficient_left_right);

		AE_L32_IP(P_input_left_surround, input_left_surround, 6 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp_left, P_input_left_surround, P_coefficient_left_s_right_s);

		AE_L32_IP(P_input_right_surround, input_right_surround, 6 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp_right, P_input_right_surround, P_coefficient_left_s_right_s);

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right);

		AE_S32_L_IP(P_output_left, output_left, 2 * sizeof(ae_int32));
		AE_S32_L_IP(P_output_right, output_right, 2 * sizeof(ae_int32));
	}
}

void downmix32bit_7_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_left_s_right_s;
	ae_int32x2 P_coefficient_center_lfe;
	ae_int32x2 P_coefficient_left_S_right_S;

	/* Load the downmix coefficients. */
	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_left_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SURROUND << 2);
	ae_int32x2 P_coefficient_right_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SURROUND << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);
	ae_int32x2 P_coefficient_left_side =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SIDE << 2);
	ae_int32x2 P_coefficient_right_side =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SIDE << 2);

	/*
	 * We have 6 coefficients (constant inside loop), 6 channels and only 8
	 * AE_P registers. But each of those registers is 48bit or 2x24bit wide.
	 * Also many Hifi2 operations can use either lower or higher 24 bits from
	 * those registers. By combining 6 coefficients in pairs we save 3 AE_P
	 * registers.
	 */
	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_left_s_right_s = AE_SEL32_LL(P_coefficient_left_surround,
						   P_coefficient_right_surround);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);
	P_coefficient_left_S_right_S = AE_SEL32_LL(P_coefficient_left_side,
						   P_coefficient_right_side);

	/* Only load the channel if it's present. */
	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_center = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_left_surround = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND) << 2));
	const ae_int32 *input_right_surround = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SURROUND) << 2));
	const ae_int32 *input_lfe = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LFE) << 2));
	const ae_int32 *input_left_side = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SIDE) << 2));
	const ae_int32 *input_right_side = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SIDE) << 2));

	/* Downmixer single store. */
	ae_int32 *output_left = (ae_int32 *)(out_data);
	ae_int32 *output_right = (ae_int32 *)(out_data + sizeof(ae_int32));

	/* We will be using P & Q registers. */
	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_left_surround;
	ae_int32x2 P_input_right_surround;
	ae_int32x2 P_input_lfe;
	ae_int32x2 P_input_left_side;
	ae_int32x2 P_input_right_side;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;

	/*
	 * We don't need to initialize those registers in loop's body.
	 * Using non accumulating multiplication for the first left,rignt channels
	 * is more efficient.
	 * For non 5.1 version we cannot use this optimization, we don't know
	 * which channel is present and which multiplication should reset output
	 * accumulators.
	 */
	ae_f64 Q_tmp_left = AE_ZERO64();
	ae_f64 Q_tmp_right = AE_ZERO64();

	const ae_int32 *const end_input_left = input_left + (in_size / (sizeof(ae_int32)));
	const char cs = 8 * sizeof(ae_int32);

	while (input_left < end_input_left) {
		/* Load one 24-bit value, replicate in two elements of register P. */
		AE_L32_XP(P_input_center, input_center, cs);
		Q_tmp_left = AE_MULF32S_LH(P_input_center, P_coefficient_center_lfe);

		AE_L32_XP(P_input_lfe, input_lfe, cs);
		AE_MULAF32S_LL(Q_tmp_left, P_input_lfe, P_coefficient_center_lfe);

		Q_tmp_right = Q_tmp_left;

		AE_L32_XP(P_input_left, input_left, cs);
		AE_MULAF32S_LH(Q_tmp_left, P_input_left, P_coefficient_left_right);

		AE_L32_XP(P_input_right, input_right, cs);
		AE_MULAF32S_LL(Q_tmp_right, P_input_right, P_coefficient_left_right);

		AE_L32_XP(P_input_left_surround, input_left_surround, cs);
		AE_MULAF32S_LH(Q_tmp_left, P_input_left_surround, P_coefficient_left_s_right_s);

		AE_L32_XP(P_input_right_surround, input_right_surround, cs);
		AE_MULAF32S_LL(Q_tmp_right, P_input_right_surround, P_coefficient_left_s_right_s);

		AE_L32_XP(P_input_left_side, input_left_side, cs);
		AE_MULAF32S_LH(Q_tmp_left, P_input_left_side, P_coefficient_left_S_right_S);

		AE_L32_XP(P_input_right_side, input_right_side, cs);
		AE_MULAF32S_LL(Q_tmp_right, P_input_right_side, P_coefficient_left_S_right_S);

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right);

		AE_S32_L_IP(P_output_left, output_left, 2 * sizeof(ae_int32));
		AE_S32_L_IP(P_output_right, output_right, 2 * sizeof(ae_int32));
	}
}

void shiftcopy16bit_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	ae_int16 *in_ptrs = (ae_int16 *)in_data;
	ae_int32x2 *out_ptrs = (ae_int32x2 *)out_data;

	for (i = 0; i < (in_size >> 1); ++i)
		out_ptrs[i] = AE_MOVINT32_FROMINT16(in_ptrs[i]) << 16;
}

void shiftcopy16bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	ae_p16x2s *in_ptrs = (ae_p16x2s *)in_data;
	ae_p24x2f *out_ptrs = (ae_p24x2f *)out_data;
	ae_p24x2s in_regs = AE_ZEROP48();

	for (i = 0; i < (in_size >> 2); ++i) {
		in_regs = in_ptrs[i];
		AE_SP24X2F_X(in_regs, out_ptrs, i << 3);
	}
}

void downmix16bit(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		  const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_left_s_right_s;
	ae_int32x2 P_coefficient_center_lfe;

	/* Load the downmix coefficients. */
	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_left_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SURROUND << 2);
	ae_int32x2 P_coefficient_right_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SURROUND << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);

	/*
	 * We have 6 coefficients (constant inside loop), 6 channels and only 8
	 * AE_P registers. But each of those registers is 48bit or 2x24bit wide.
	 * Also many Hifi2 operations can use either lower or higher 24 bits from
	 * those registers. By combining 6 coefficients in pairs we save 3 AE_P
	 * registers.
	 */
	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_left_s_right_s = AE_SEL32_LL(P_coefficient_left_surround,
						   P_coefficient_right_surround);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);

	/* See what channels are available. */
	bool left = (get_channel_location(cd->in_channel_map, CHANNEL_LEFT) != 0xF);
	bool center = (get_channel_location(cd->in_channel_map, CHANNEL_CENTER) != 0xF);
	bool right = (get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) != 0xF);
	bool left_surround =
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND) != 0xF);
	bool right_surround =
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SURROUND) != 0xF);
	bool lfe = (get_channel_location(cd->in_channel_map, CHANNEL_LFE) != 0xF);

	/* Downmixer single load. */
	ae_p16s *input_left = NULL;
	ae_p16s *input_center = NULL;
	ae_p16s *input_right = NULL;
	ae_p16s *input_left_surround = NULL;
	ae_p16s *input_right_surround = NULL;
	ae_p16s *input_lfe = NULL;

	/* Only load the channel if it's present. */
	if (left) {
		input_left = (ae_p16s *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 1));
	}
	if (center) {
		input_center = (ae_p16s *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 1));
	}
	if (right) {
		input_right = (ae_p16s *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 1));
	}
	if (left_surround) {
		input_left_surround = (ae_p16s *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND) << 1));
	}
	if (right_surround) {
		input_right_surround = (ae_p16s *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SURROUND) << 1));
	}
	if (lfe) {
		input_lfe = (ae_p16s *)(in_data +
			(get_channel_location(cd->in_channel_map, CHANNEL_LFE) << 1));
	}

	/** Calculate number of samples in a single channel. */
	uint32_t number_of_samples_in_one_channel = in_size / cd->in_channel_no;

	number_of_samples_in_one_channel >>= 1;

	/* Downmixer single store. */
	ae_int32 *output_left = (ae_int32 *)(out_data + 0);
	ae_int32 *output_right = (ae_int32 *)(out_data + sizeof(ae_int32));

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_left_surround;
	ae_int32x2 P_input_right_surround;
	ae_int32x2 P_input_lfe;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;

	/*
	 * Calculating this outside of the loop is significant performance
	 * improvement. The value of this expression was reevaluated in each
	 * iteration when placed inside loop.
	 */
	int sample_offset = cd->in_channel_no << 1;

	for (i = 0; i < (number_of_samples_in_one_channel); i++) {
		ae_f64 Q_tmp_left = AE_ZEROQ56();
		ae_f64 Q_tmp_right = AE_ZEROQ56();

		/* Load one 24-bit value, replicate in two elements of register P. */
		if (left) {
			P_input_left = AE_L16M_X(input_left, i * sample_offset);
			AE_MULAF32S_LH(Q_tmp_left, P_input_left, P_coefficient_left_right);
		}
		if (center) {
			P_input_center = AE_L16M_X(input_center, i * sample_offset);
			AE_MULAF32S_LH(Q_tmp_left, P_input_center, P_coefficient_center_lfe);
			AE_MULAF32S_LH(Q_tmp_right, P_input_center, P_coefficient_center_lfe);
		}
		if (right) {
			P_input_right = AE_L16M_X(input_right, i * sample_offset);
			AE_MULAF32S_LL(Q_tmp_right, P_input_right, P_coefficient_left_right);
		}
		if (left_surround) {
			P_input_left_surround = AE_L16M_X(input_left_surround, i * sample_offset);
			AE_MULAF32S_LH(Q_tmp_left, P_input_left_surround,
				       P_coefficient_left_s_right_s);
			if (cd->in_channel_config == IPC4_CHANNEL_CONFIG_4_POINT_0) {
				AE_MULAF32S_LH(Q_tmp_right, P_input_left_surround,
					       P_coefficient_left_s_right_s);
			}
		}
		if (right_surround) {
			P_input_right_surround = AE_L16M_X(input_right_surround, i * sample_offset);
			AE_MULAF32S_LL(Q_tmp_right, P_input_right_surround,
				       P_coefficient_left_s_right_s);
		}
		if (lfe) {
			P_input_lfe = AE_L16M_X(input_lfe, i * sample_offset);
			AE_MULAF32S_LL(Q_tmp_left, P_input_lfe, P_coefficient_center_lfe);
			AE_MULAF32S_LL(Q_tmp_right, P_input_lfe, P_coefficient_center_lfe);
		}

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right);

		output_left[i * 2] = AE_SLAI32(P_output_left, 2 * sizeof(ae_int32));
		output_right[i * 2] = AE_SLAI32(P_output_right, 2 * sizeof(ae_int32));
	}
}

void downmix16bit_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
		      const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_left_s_right_s;
	ae_int32x2 P_coefficient_center_lfe;

	/* Load the downmix coefficients. */
	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_left_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SURROUND << 2);
	ae_int32x2 P_coefficient_right_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SURROUND << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_left_s_right_s = AE_SEL32_LL(P_coefficient_left_surround,
						   P_coefficient_right_surround);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);

	/* Downmixer single load. */
	ae_p16s *input_left = NULL;
	ae_p16s *input_center = NULL;
	ae_p16s *input_right = NULL;
	ae_p16s *input_left_surround = NULL;
	ae_p16s *input_right_surround = NULL;
	ae_p16s *input_lfe = NULL;

	/* Only load the channel if it's present. */
	input_left = (ae_p16s *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 1));
	input_center = (ae_p16s *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 1));
	input_right = (ae_p16s *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 1));
	input_left_surround = (ae_p16s *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND) << 1));
	input_right_surround = (ae_p16s *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SURROUND) << 1));
	input_lfe = (ae_p16s *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LFE) << 1));

	/** Calculate number of samples in a single channel. */
	uint32_t number_of_samples_in_one_channel = in_size / cd->in_channel_no;

	number_of_samples_in_one_channel >>= 1;

	/* Downmixer single store. */
	ae_int32 *output_left = (ae_int32 *)(out_data + 0);
	ae_int32 *output_right = (ae_int32 *)(out_data + sizeof(ae_int32));

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_left_surround;
	ae_int32x2 P_input_right_surround;
	ae_int32x2 P_input_lfe;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;

	/*
	 * We don't need to initialize those registers in loop's body.
	 * Using non accumulating multiplication for the first left,rignt channels
	 * is more efficient.
	 * For non 5.1 version we cannot use this optimization, we don't know
	 * which channel is present and which multiplication should reset output
	 * accumulators.
	 */
	ae_f64 Q_tmp_left = AE_ZERO64();
	ae_f64 Q_tmp_right = AE_ZERO64();

	/*
	 * Calculating this outside of the loop is significant performance
	 * improvement. The value of this expression was reevaluated in each
	 * iteration when placed inside loop.
	 */
	int sample_offset = cd->in_channel_no << 1;
	/* 0.011xSR(k) MIPS */
	for (i = 0; i < (number_of_samples_in_one_channel); i++) {
		/* Load one 16-bit value, replicate in two elements of register P. */
		P_input_center = AE_L16M_X(input_center, i * sample_offset);
		Q_tmp_left = AE_MULF32S_LH(P_input_center, P_coefficient_center_lfe);

		P_input_lfe = AE_L16M_X(input_lfe, i * sample_offset);
		AE_MULAF32S_LL(Q_tmp_left, P_input_lfe, P_coefficient_center_lfe);

		Q_tmp_right = Q_tmp_left;

		P_input_left = AE_L16M_X(input_left, i * sample_offset);
		AE_MULAF32S_LH(Q_tmp_left, P_input_left, P_coefficient_left_right);

		P_input_right = AE_L16M_X(input_right, i * sample_offset);
		AE_MULAF32S_LL(Q_tmp_right, P_input_right, P_coefficient_left_right);

		P_input_left_surround = AE_L16M_X(input_left_surround, i * sample_offset);
		AE_MULAF32S_LH(Q_tmp_left, P_input_left_surround, P_coefficient_left_s_right_s);

		P_input_right_surround = AE_L16M_X(input_right_surround, i * sample_offset);
		AE_MULAF32S_LL(Q_tmp_right, P_input_right_surround, P_coefficient_left_s_right_s);

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right);

		output_left[i * 2] = AE_SLAI32(P_output_left, 2 * sizeof(ae_int32));
		output_right[i * 2] = AE_SLAI32(P_output_right, 2 * sizeof(ae_int32));
	}
}

void downmix16bit_4ch_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	size_t i;

	uint32_t idx1 = get_channel_index(cd->in_channel_map, 0);
	uint32_t idx2 = get_channel_index(cd->in_channel_map, 1);
	uint32_t idx3 = get_channel_index(cd->in_channel_map, 2);
	uint32_t idx4 = get_channel_index(cd->in_channel_map, 3);

	uint16_t coeffs[4] = {cd->downmix_coefficients[idx1],
			      cd->downmix_coefficients[idx2],
			      cd->downmix_coefficients[idx3],
			      cd->downmix_coefficients[idx4]
			};

	ae_int16x4 coeff = AE_L16X4_X((ae_int16x4 *)coeffs, 0);

	const ae_int16x4 *input_data = (const ae_int16x4 *)in_data;
	ae_int16 *output_data = (ae_int16 *)out_data;

	ae_int16x4  P_input;
	ae_int16x4 P_output;

	const uint32_t channel_no = 4;

	for (i = 0; i < in_size / (sizeof(int16_t)); i += channel_no) {
		AE_L16X4_XP(P_input, input_data,  sizeof(ae_int16) * channel_no);
		ae_f32x2 Q_tmp = AE_MULF16SS_00(P_input, coeff);

		AE_MULAF16SS_11(Q_tmp, P_input, coeff);
		AE_MULAF16SS_22(Q_tmp, P_input, coeff);
		AE_MULAF16SS_33(Q_tmp, P_input, coeff);

		P_output = AE_ROUND16X4F32SSYM(Q_tmp, Q_tmp);
		AE_S16_0_IP(P_output, output_data, sizeof(ae_int16));
	}
}

void downmix32bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	uint32_t downmix_coefficient = 1073741568;

	/* Load the downmix coefficients. */
	ae_int32x2 P_coefficient_left_right = AE_L32_X((ae_int32 *)&downmix_coefficient, 0);

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_right;
	ae_int32x2 P_output;

	const ae_int32 *input_left = (ae_int32 *)in_data;
	const ae_int32 *input_right = input_left + 1;

	ae_int32 *output = (ae_int32 *)(out_data);

	for (i = 0; i < (in_size >> 3); ++i) {
		P_input_left = AE_L32_X(input_left, i * 2 * sizeof(ae_int32));
		ae_f64 Q_tmp = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		P_input_right = AE_L32_X(input_right, i * 2 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_right, P_coefficient_left_right);

		P_output = AE_ROUND32F64SSYM(Q_tmp);

		output[i] = P_output;
	}
}

void downmix16bit_stereo(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			 const uint32_t in_size, uint8_t * const out_data)
{
	size_t idx;

	/* TODO: optimize using Hifi3 */
	const uint16_t *in_data16 = (uint16_t *)in_data;
	uint16_t *out_data16 = (uint16_t *)out_data;

	for (idx = 0; idx < (in_size / 4); ++idx)
		out_data16[idx] = (in_data16[2 * idx] / 2) + (in_data16[2 * idx + 1] / 2);
}

void downmix32bit_3_1_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	size_t i;

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_center_lfe;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_center = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_lfe = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LFE) << 2));

	ae_int32 *output = (ae_int32 *)(out_data);

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_lfe;

	ae_int32x2 P_output;

	const uint32_t channel_no = 4;

	for (i = 0; i < in_size / (sizeof(ae_int32)); i += channel_no) {
		ae_f64 Q_tmp;

		AE_L32_IP(P_input_left, input_left, 4 * sizeof(ae_int32));
		Q_tmp = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		AE_L32_IP(P_input_center, input_center, 4 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp, P_input_center, P_coefficient_center_lfe);

		AE_L32_IP(P_input_right, input_right, 4 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_right, P_coefficient_left_right);

		AE_L32_IP(P_input_lfe, input_lfe, 4 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_lfe, P_coefficient_center_lfe);

		P_output = AE_ROUND32F64SSYM(Q_tmp);

		AE_S32_L_IP(P_output, output, sizeof(ae_int32));
	}
}

void downmix32bit_4_0_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	size_t i;

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_center_cs;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_cs =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER_SURROUND << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_center_cs = AE_SEL32_LL(P_coefficient_center, P_coefficient_cs);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_center = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_cs = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER_SURROUND) << 2));

	ae_int32 *output = (ae_int32 *)(out_data);

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_cs;

	ae_int32x2 P_output;

	const uint32_t channel_no = 4;

	for (i = 0; i < in_size / (sizeof(ae_int32)); i += channel_no) {
		ae_f64 Q_tmp;

		AE_L32_IP(P_input_left, input_left, 4 * sizeof(ae_int32));
		Q_tmp = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		AE_L32_IP(P_input_center, input_center, 4 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp, P_input_center, P_coefficient_center_cs);

		AE_L32_IP(P_input_right, input_right, 4 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_right, P_coefficient_left_right);

		AE_L32_IP(P_input_cs, input_cs, 4 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_cs, P_coefficient_center_cs);

		P_output = AE_ROUND32F64SSYM(Q_tmp);

		AE_S32_L_IP(P_output, output, sizeof(ae_int32));
	}
}

void downmix32bit_quatro_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			      const uint32_t in_size, uint8_t * const out_data)
{
	size_t i;

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_ls_rs;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_ls =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SURROUND << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_rs =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SURROUND << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_ls_rs = AE_SEL32_LL(P_coefficient_ls, P_coefficient_rs);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_ls = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_rs = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SURROUND) << 2));

	ae_int32 *output = (ae_int32 *)(out_data);

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_ls;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_rs;

	ae_int32x2 P_output;

	const uint32_t channel_no = 4;

	for (i = 0; i < in_size / (sizeof(ae_int32)); i += channel_no) {
		ae_f64 Q_tmp;

		AE_L32_IP(P_input_left, input_left, 4 * sizeof(ae_int32));
		Q_tmp = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		AE_L32_IP(P_input_ls, input_ls, 4 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp, P_input_ls, P_coefficient_ls_rs);

		AE_L32_IP(P_input_right, input_right, 4 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_right, P_coefficient_left_right);

		AE_L32_IP(P_input_rs, input_rs, 4 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_rs, P_coefficient_ls_rs);

		P_output = AE_ROUND32F64SSYM(Q_tmp);

		AE_S32_L_IP(P_output, output, sizeof(ae_int32));
	}
}

void downmix32bit_5_1_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	size_t i;

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_center_cs;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_cs =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER_SURROUND << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_center_cs = AE_SEL32_LL(P_coefficient_center, P_coefficient_cs);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_center = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_cs = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER_SURROUND) << 2));

	ae_int32 *output = (ae_int32 *)(out_data);

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_cs;

	ae_int32x2 P_output;

	const uint32_t channel_no = 6;

	for (i = 0; i < in_size / (sizeof(ae_int32)); i += channel_no) {
		ae_f64 Q_tmp;

		AE_L32_IP(P_input_left, input_left, 6 * sizeof(ae_int32));
		Q_tmp = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		AE_L32_IP(P_input_center, input_center, 6 * sizeof(ae_int32));
		AE_MULAF32S_LH(Q_tmp, P_input_center, P_coefficient_center_cs);

		AE_L32_IP(P_input_right, input_right, 6 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_right, P_coefficient_left_right);

		AE_L32_IP(P_input_cs, input_cs, 6 * sizeof(ae_int32));
		AE_MULAF32S_LL(Q_tmp, P_input_cs, P_coefficient_center_cs);

		P_output = AE_ROUND32F64SSYM(Q_tmp);

		AE_S32_L_IP(P_output, output, sizeof(ae_int32));
	}
}

void downmix32bit_7_1_mono(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	size_t i;

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_center_cs;

	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_cs =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER_SURROUND << 2);

	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_center_cs = AE_SEL32_LL(P_coefficient_center, P_coefficient_cs);

	const ae_int32 *input_left = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT) << 2));
	const ae_int32 *input_center = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER) << 2));
	const ae_int32 *input_right = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT) << 2));
	const ae_int32 *input_cs = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER_SURROUND) << 2));

	ae_int32 *output = (ae_int32 *)(out_data);

	ae_int32x2 P_input_left;
	ae_int32x2 P_input_center;
	ae_int32x2 P_input_right;
	ae_int32x2 P_input_cs;

	ae_int32x2 P_output;

	const uint32_t channel_no = 8;
	const size_t offset = sizeof(ae_int32) * channel_no;

	for (i = 0; i < in_size / (sizeof(ae_int32)); i += channel_no) {
		ae_f64 Q_tmp;

		AE_L32_XP(P_input_left, input_left, offset);
		Q_tmp = AE_MULF32S_LH(P_input_left, P_coefficient_left_right);

		AE_L32_XP(P_input_center, input_center, offset);
		AE_MULAF32S_LH(Q_tmp, P_input_center, P_coefficient_center_cs);

		AE_L32_XP(P_input_right, input_right, offset);
		AE_MULAF32S_LL(Q_tmp, P_input_right, P_coefficient_left_right);

		AE_L32_XP(P_input_cs, input_cs, offset);
		AE_MULAF32S_LL(Q_tmp, P_input_cs, P_coefficient_center_cs);

		P_output = AE_ROUND32F64SSYM(Q_tmp);

		AE_S32_L_IP(P_output, output, sizeof(ae_int32));
	}
}

void downmix32bit_7_1_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			     const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	channel_map out_channel_map = cd->out_channel_map;
	const uint8_t left_slot = get_channel_location(out_channel_map, CHANNEL_LEFT);
	const uint8_t center_slot = get_channel_location(out_channel_map, CHANNEL_CENTER);
	const uint8_t right_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT);
	uint8_t right_surround_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT_SURROUND);
	uint8_t left_surround_slot = get_channel_location(out_channel_map, CHANNEL_LEFT_SURROUND);
	const uint8_t lfe_slot = get_channel_location(out_channel_map, CHANNEL_LFE);

	/* Must support also 5.1 Surround */
	const bool surround_5_1_channel_map = (left_surround_slot == CHANNEL_INVALID) &&
					      (right_surround_slot == CHANNEL_INVALID);

	if (surround_5_1_channel_map) {
		left_surround_slot = get_channel_location(out_channel_map, CHANNEL_LEFT_SIDE);
		right_surround_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT_SIDE);
	}

	ae_int32 *output_left_ptr = (ae_int32 *)(out_data + (left_slot << 2));
	ae_int32 *output_center_ptr = (ae_int32 *)(out_data + (center_slot << 2));
	ae_int32 *output_right_ptr = (ae_int32 *)(out_data + (right_slot << 2));
	ae_int32 *output_side_left_ptr = (ae_int32 *)(out_data + (left_surround_slot << 2));
	ae_int32 *output_side_right_ptr = (ae_int32 *)(out_data + (right_surround_slot << 2));
	ae_int32 *output_lfe_ptr = (ae_int32 *)(out_data + (lfe_slot << 2));

	ae_int32 *in_left_ptr = (ae_int32 *)in_data;
	ae_int32 *in_center_ptr = (ae_int32 *)(in_data + 4);
	ae_int32 *in_right_ptr = (ae_int32 *)(in_data + 8);
	ae_int32 *in_lfe_ptr = (ae_int32 *)(in_data + 20);

	for (i = 0; i < (in_size >> 5); ++i) {
		output_left_ptr[i * 6] = in_left_ptr[i * 8];
		output_right_ptr[i * 6] = in_right_ptr[i * 8];
		output_center_ptr[i * 6] = in_center_ptr[i * 8];
		output_lfe_ptr[i * 6] = in_lfe_ptr[i * 8];
	}

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_left_s_right_s;
	ae_int32x2 P_coefficient_center_lfe;
	ae_int32x2 P_coefficient_left_S_right_S;

	/* Load the downmix coefficients. */
	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_left_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SURROUND << 2);
	ae_int32x2 P_coefficient_right_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SURROUND << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);
	ae_int32x2 P_coefficient_left_side =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SIDE << 2);
	ae_int32x2 P_coefficient_right_side =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SIDE << 2);

	/*
	 * We have 6 coefficients (constant inside loop), 6 channels and only 8
	 * AE_P registers. But each of those registers is 48bit or 2x24bit wide.
	 * Also many Hifi2 operations can use either lower or higher 24 bits from
	 * those registers. By combining 6 coefficients in pairs we save 3 AE_P
	 * registers.
	 */
	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_left_s_right_s = AE_SEL32_LL(P_coefficient_left_surround,
						   P_coefficient_right_surround);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);
	P_coefficient_left_S_right_S = AE_SEL32_LL(P_coefficient_left_side,
						   P_coefficient_right_side);

	const ae_int32 *input_left_surround = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SURROUND) << 2));
	const ae_int32 *input_right_surround = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SURROUND) << 2));
	const ae_int32 *input_left_side = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SIDE) << 2));
	const ae_int32 *input_right_side = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SIDE) << 2));

	/* We will be using P & Q registers. */
	ae_int32x2 P_input_left_surround;
	ae_int32x2 P_input_right_surround;
	ae_int32x2 P_input_left_side;
	ae_int32x2 P_input_right_side;

	ae_int32x2 P_output_left;
	ae_int32x2 P_output_right;

	const ae_int32 *const end_input_left = input_left_surround + (in_size / (sizeof(ae_int32)));
	const char cs = 8 * sizeof(ae_int32);

	while (input_left_surround < end_input_left) {
		ae_f64 Q_tmp_right_side;
		ae_f64 Q_tmp_left_side;

		/* Load one 24-bit value, replicate in two elements of register P. */
		AE_L32_XP(P_input_left_surround, input_left_surround, cs);
		Q_tmp_left_side = AE_MULF32S_LH(P_input_left_surround, P_coefficient_left_right);
		Q_tmp_right_side = AE_MULF32S_LL(P_input_left_surround, P_coefficient_left_S_right_S);

		AE_L32_XP(P_input_right_surround, input_right_surround, cs);
		AE_MULAF32S_LH(Q_tmp_left_side, P_input_right_surround, P_coefficient_left_S_right_S);
		AE_MULAF32S_LL(Q_tmp_right_side, P_input_right_surround, P_coefficient_left_right);

		AE_L32_XP(P_input_left_side, input_left_side, cs);
		AE_MULAF32S_LH(Q_tmp_left_side, P_input_left_side, P_coefficient_left_right);

		AE_L32_XP(P_input_right_side, input_right_side, cs);
		AE_MULAF32S_LL(Q_tmp_right_side, P_input_right_side, P_coefficient_left_right);

		P_output_left = AE_ROUND32F64SSYM(Q_tmp_left_side);
		P_output_right = AE_ROUND32F64SSYM(Q_tmp_right_side);

		AE_S32_L_IP(P_output_left, output_side_left_ptr, 6 * sizeof(ae_int32));
		AE_S32_L_IP(P_output_right, output_side_right_ptr, 6 * sizeof(ae_int32));
	}
}

void upmix32bit_4_0_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			   const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	channel_map out_channel_map = cd->out_channel_map;

	const uint8_t left_slot = get_channel_location(out_channel_map, CHANNEL_LEFT);
	const uint8_t center_slot = get_channel_location(out_channel_map, CHANNEL_CENTER);
	const uint8_t right_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT);
	uint8_t right_surround_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT_SURROUND);
	uint8_t left_surround_slot = get_channel_location(out_channel_map, CHANNEL_LEFT_SURROUND);
	const uint8_t lfe_slot = get_channel_location(out_channel_map, CHANNEL_LFE);

	/* Must support also 5.1 Surround */
	const bool surround_5_1_channel_map = (left_surround_slot == CHANNEL_INVALID) &&
		(right_surround_slot == CHANNEL_INVALID);

	if (surround_5_1_channel_map) {
		left_surround_slot = get_channel_location(out_channel_map, CHANNEL_LEFT_SIDE);
		right_surround_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT_SIDE);
	}

	ae_int32 *output_left = (ae_int32 *)(out_data + (left_slot << 2));
	ae_int32 *output_center = (ae_int32 *)(out_data + (center_slot << 2));
	ae_int32 *output_right = (ae_int32 *)(out_data + (right_slot << 2));
	ae_int32 *output_side_left = (ae_int32 *)(out_data + (left_surround_slot << 2));
	ae_int32 *output_side_right = (ae_int32 *)(out_data + (right_surround_slot << 2));
	ae_int32 *output_lfe = (ae_int32 *)(out_data + (lfe_slot << 2));

	ae_int32 *in_left_ptr = (ae_int32 *)in_data;
	ae_int32 *in_center_ptr = (ae_int32 *)(in_data + 4);
	ae_int32 *in_right_ptr = (ae_int32 *)(in_data + 8);

	for (i = 0; i < (in_size >> 4); ++i) {
		output_left[i * 6] = in_left_ptr[i * 4];
		output_right[i * 6] = in_right_ptr[i * 4];
		output_center[i * 6] = in_center_ptr[i * 4];
		output_lfe[i * 6] = 0;
	}

	ae_int32x2 P_coefficient_left_right;
	ae_int32x2 P_coefficient_left_s_right_s;
	ae_int32x2 P_coefficient_center_lfe;
	ae_int32x2 P_coefficient_left_S_right_S;

	/* Load the downmix coefficients. */
	ae_int32x2 P_coefficient_left =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT << 2);
	ae_int32x2 P_coefficient_center =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_CENTER << 2);
	ae_int32x2 P_coefficient_right =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT << 2);
	ae_int32x2 P_coefficient_left_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SURROUND << 2);
	ae_int32x2 P_coefficient_right_surround =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SURROUND << 2);
	ae_int32x2 P_coefficient_lfe =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LFE << 2);
	ae_int32x2 P_coefficient_left_side =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_LEFT_SIDE << 2);
	ae_int32x2 P_coefficient_right_side =
		AE_L32_X((ae_int32 *)cd->downmix_coefficients, CHANNEL_RIGHT_SIDE << 2);

	/*
	 * We have 6 coefficients (constant inside loop), 6 channels and only 8
	 * AE_P registers. But each of those registers is 48bit or 2x24bit wide.
	 * Also many Hifi2 operations can use either lower or higher 24 bits from
	 * those registers. By combining 6 coefficients in pairs we save 3 AE_P
	 * registers.
	 */
	P_coefficient_left_right = AE_SEL32_LL(P_coefficient_left, P_coefficient_right);
	P_coefficient_left_s_right_s = AE_SEL32_LL(P_coefficient_left_surround,
						   P_coefficient_right_surround);
	P_coefficient_center_lfe = AE_SEL32_LL(P_coefficient_center, P_coefficient_lfe);
	P_coefficient_left_S_right_S = AE_SEL32_LL(P_coefficient_left_side,
						   P_coefficient_right_side);

	const ae_int32 *input_center_surround = (ae_int32 *)(in_data +
		(get_channel_location(cd->in_channel_map, CHANNEL_CENTER_SURROUND) << 2));

	/* We will be using P & Q registers. */
	ae_int32x2 P_input_center_surround;

	ae_int32x2 P_output_left_side;
	ae_int32x2 P_output_right_side;

	/*
	 * We don't need to initialize those registers in loop's body.
	 * Using non accumulating multiplication for the first left,rignt channels
	 * is more efficient.
	 * For non 5.1 version we cannot use this optimization, we don't know
	 * which channel is present and which multiplication should reset output
	 * accumulators.
	 */
	ae_f64 Q_tmp_right_side = AE_ZERO64();
	ae_f64 Q_tmp_left_side = AE_ZERO64();

	const ae_int32 *const end_input_left = input_center_surround + (in_size / (sizeof(ae_int32)));
	const char cs = 4 * sizeof(ae_int32);

	while (input_center_surround < end_input_left) {
		/* Load one 24-bit value, replicate in two elements of register P. */
		AE_L32_XP(P_input_center_surround, input_center_surround, cs);
		Q_tmp_left_side = AE_MULF32S_LH(P_input_center_surround,
						P_coefficient_left_s_right_s);
		Q_tmp_right_side = AE_MULF32S_LL(P_input_center_surround,
						 P_coefficient_left_s_right_s);

		P_output_left_side = AE_ROUND32F64SSYM(Q_tmp_left_side);
		P_output_right_side = AE_ROUND32F64SSYM(Q_tmp_right_side);

		AE_S32_L_IP(P_output_left_side, output_side_left, 6 * sizeof(ae_int32));
		AE_S32_L_IP(P_output_right_side, output_side_right, 6 * sizeof(ae_int32));
	}
}

void upmix32bit_quatro_to_5_1(struct up_down_mixer_data *cd, const uint8_t * const in_data,
			      const uint32_t in_size, uint8_t * const out_data)
{
	uint32_t i;

	channel_map out_channel_map = cd->out_channel_map;

	const uint8_t left_slot = get_channel_location(out_channel_map, CHANNEL_LEFT);
	const uint8_t center_slot = get_channel_location(out_channel_map, CHANNEL_CENTER);
	const uint8_t right_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT);
	uint8_t right_surround_slot = get_channel_location(out_channel_map, CHANNEL_RIGHT_SURROUND);
	uint8_t left_surround_slot = get_channel_location(out_channel_map, CHANNEL_LEFT_SURROUND);
	const uint8_t lfe_slot = get_channel_location(out_channel_map, CHANNEL_LFE);

	/* Must support also 5.1 Surround */
	const bool surround_5_1_channel_map = (left_surround_slot == CHANNEL_INVALID) &&
		(right_surround_slot == CHANNEL_INVALID);

	if (surround_5_1_channel_map) {
		left_surround_slot = get_channel_location(cd->in_channel_map, CHANNEL_LEFT_SIDE);
		right_surround_slot = get_channel_location(cd->in_channel_map, CHANNEL_RIGHT_SIDE);
	}

	ae_int32 *output_left = (ae_int32 *)(out_data + (left_slot << 2));
	ae_int32 *output_center = (ae_int32 *)(out_data + (center_slot << 2));
	ae_int32 *output_right = (ae_int32 *)(out_data + (right_slot << 2));
	ae_int32 *output_side_left = (ae_int32 *)(out_data + (left_surround_slot << 2));
	ae_int32 *output_side_right = (ae_int32 *)(out_data + (right_surround_slot << 2));
	ae_int32 *output_lfe = (ae_int32 *)(out_data + (lfe_slot << 2));

	ae_int32 *in_left_ptr = (ae_int32 *)in_data;
	ae_int32 *in_right_ptr = (ae_int32 *)(in_data + 4);
	ae_int32 *in_left_sorround_ptr = (ae_int32 *)(in_data + 8);
	ae_int32 *in_right_sorround_ptr = (ae_int32 *)(in_data + 12);

	for (i = 0; i < (in_size >> 4); ++i) {
		output_left[i * 6] = in_left_ptr[i * 4];
		output_right[i * 6] = in_right_ptr[i * 4];
		output_center[i * 6] = 0;
		output_side_left[i * 6] = in_left_sorround_ptr[i * 4];
		output_side_right[i * 6] = in_right_sorround_ptr[i * 4];
		output_lfe[i * 6] = 0;
	}
}
