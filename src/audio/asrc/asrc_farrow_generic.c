// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2012-2019 Intel Corporation. All rights reserved.

#include <sof/audio/asrc/asrc_config.h>

#if ASRC_GENERIC == 1

#include <sof/audio/asrc/asrc_farrow.h>
#include <sof/audio/format.h>

LOG_MODULE_DECLARE(asrc, CONFIG_SOF_LOG_LEVEL);

void asrc_fir_filter16(struct asrc_farrow *src_obj, int16_t **output_buffers,
		       int index_output_frame)
{
	int64_t prod;
	int32_t prod32;
	int16_t prod16;
	int32_t *filter_p;
	int16_t *buffer_p;
	int ch;
	int n;
	int i;

	if (src_obj->output_format == ASRC_IOF_INTERLEAVED)
		i = src_obj->num_channels * index_output_frame;
	else
		i = index_output_frame;

	/* Iterate over each channel */
	for (ch = 0; ch < src_obj->num_channels; ch++) {
		/* Pointer to the beginning of the impulse response */
		filter_p = &src_obj->impulse_response[0];

		/* Pointer to the buffered input data */
		buffer_p = &src_obj->ring_buffers16[ch]
			[src_obj->buffer_write_position];

		/* Initialise the accumulator */
		prod = 0;

		/* Iterate over the filter bins.
		 * Data is Q1.15, coefficients are Q1.30. Prod will be Qx.45.
		 */
		for (n = 0; n < src_obj->filter_length; n++)
			prod += (int64_t)(*buffer_p--) * (*filter_p++);

		/* Shift left after accumulation, because interim
		 * results might saturate during filtering prod = prod
		 * << 1; will shift after last addition
		 */
		prod32 = sat_int32(Q_SHIFT(prod, 45, 31));

		/* Round 'prod' to 16 bit and store it in
		 * (de-)interleaved format in the output buffers
		 */
		prod16 = sat_int16(Q_SHIFT_RND(prod32, 31, 15));
		output_buffers[ch][i] = prod16;
	}
}

void asrc_fir_filter32(struct asrc_farrow *src_obj, int32_t **output_buffers,
		       int index_output_frame)
{
	int64_t prod;
	int32_t prod32;
	const int32_t *filter_p;
	int32_t *buffer_p;
	int ch;
	int n;
	int i;

	if (src_obj->output_format == ASRC_IOF_INTERLEAVED)
		i = src_obj->num_channels * index_output_frame;
	else
		i = index_output_frame;

	/* Iterate over each channel */
	for (ch = 0; ch < src_obj->num_channels; ch++) {
		/* Pointer to the beginning of the impulse response */
		filter_p = &src_obj->impulse_response[0];

		/* Pointer to the buffered input data */
		buffer_p = &src_obj->ring_buffers32[ch]
			[src_obj->buffer_write_position];

		/* Initialise the accumulator */
		prod = 0;

		/* Iterate over the filter bins. Data is Q1.31, coefficients
		 * are Q1.22. They are down scaled by 1 shift. In addition
		 * there C is implementation specific right shift by 8. It
		 * gives headroom to calculate up to 256 taps FIR. The use
		 * of 24 bits of 32 bits is not a practical limitation for
		 * quality. The product is Qx.54.
		 */
		for (n = 0; n < src_obj->filter_length; n++)
			prod += (int64_t)(*buffer_p--) * (*filter_p++ >> 8);

		/* Shift left after accumulation, because interim
		 * results might saturate during filtering prod = prod
		 * << 1; will shift after last addition
		 */
		prod32 = sat_int32(Q_SHIFT(prod, 53, 31));

		/* Store 'prod' in (de-)interleaved format in the output
		 * buffers
		 */
		output_buffers[ch][i] = prod32;
	}
}

/* + ALGORITHM SPECIFIC FUNCTIONS */

void asrc_calc_impulse_response_n4(struct asrc_farrow *src_obj)
{
	int32_t time;
	int32_t accum20l; /* Coefficient for ^2 and ^0, l matches HiFi3 ver */
	int32_t accum20h; /* Coefficient for ^2 and ^0, h matches HiFi3 ver */
	int32_t accum31l; /* Coefficient for ^3 and ^1 polynomial terms */
	int32_t accum31h; /* Coefficient for ^3 and ^1 polynomial terms */
	const int32_t *filter_P;
	int32_t *result_P;
	int index_filter;
	int index_limit;

	/* Set the pointer tot the polyphase filters */
	filter_P = &src_obj->polyphase_filters[0];

	/*
	 * Set the pointer to the impulse response.
	 * This is where the result is stored.
	 */
	result_P = &src_obj->impulse_response[0];

	/* Get the current fractional time */
	time = sat_int32(((int64_t)src_obj->time_value) << 4);

	/*
	 * Generates two impulse response bins per iterations.
	 * 'index_limit' is therefore stored to reduce redundant
	 * calculations.
	 */
	index_limit = src_obj->filter_length >> 1;
	for (index_filter = 0; index_filter < index_limit; index_filter++) {
		/*
		 * The polyphase filters lie in storage as follows
		 * (For N = 4, M = 64):
		 * [g3,0][g3,1][g2,0][g2,1]
		 * ...[g0,0][g0,1][g3,2][g3,3]...[g0,62][g0,63].
		 *
		 * Since the polyphase filter coefficients are stored
		 * in an appropriate order, we can just load them up,
		 * one after another.
		 */

		/* Load two coefficients of the 4th polyphase filter */
		accum31l = *filter_P++;
		accum31h = *filter_P++;

		/* Load two coefficients of the 3rd polyphase filter */
		accum20l = *filter_P++;
		accum20h = *filter_P++;

		/*
		 * Use the 'Horner's Method' to calculate the result
		 * in a numerically stable and efficient way:
		 *
		 * Example for one coefficient (N = 4):
		 * g_out,m = ((g3,m*t + g2,m)*t + g1,m)*t + g0,m
		 *
		 * Q1.31 x Q1.31 -> Q2.62
		 *
		 */
		accum20l += q_multsr_sat_32x32(accum31l, time, 62 - 31);
		accum20h += q_multsr_sat_32x32(accum31h, time, 62 - 31);

		/* Load two coefficients of the second polyphase filter */
		accum31l = *filter_P++;
		accum31h = *filter_P++;

		/* Multiply and accumulate */
		accum31l += q_multsr_sat_32x32(accum20l, time, 62 - 31);
		accum31h += q_multsr_sat_32x32(accum20h, time, 62 - 31);

		/* Load two coefficients of the first polyphase filter */
		accum20l = *filter_P++;
		accum20h = *filter_P++;

		/* Multiply and accumulate */
		accum20l += q_multsr_sat_32x32(accum31l, time, 62 - 31);
		accum20h += q_multsr_sat_32x32(accum31h, time, 62 - 31);

		/* Store the result */
		*result_P++ = accum20l;
		*result_P++ = accum20h;
	}
}

void asrc_calc_impulse_response_n5(struct asrc_farrow *src_obj)
{
	/*
	 * See 'calc_impulse_response_n4' for a detailed description
	 * of the algorithm and data handling
	 */
	int32_t time;
	int32_t accum31l;
	int32_t accum31h;
	int32_t accum420l;
	int32_t accum420h;
	const int32_t *filter_P;
	int32_t *result_P;
	int index_filter;
	int index_limit;

	filter_P = &src_obj->polyphase_filters[0];
	result_P = &src_obj->impulse_response[0];
	time = sat_int32(((int64_t)src_obj->time_value) << 4);

	index_limit = src_obj->filter_length >> 1;
	for (index_filter = 0; index_filter < index_limit; index_filter++) {
		accum420l = *filter_P++;
		accum420h = *filter_P++;
		accum31l = *filter_P++;
		accum31h = *filter_P++;
		accum31l += q_multsr_sat_32x32(accum420l, time, 62 - 31);
		accum31h += q_multsr_sat_32x32(accum420h, time, 62 - 31);

		accum420l = *filter_P++;
		accum420h = *filter_P++;
		accum420l += q_multsr_sat_32x32(accum31l, time, 62 - 31);
		accum420h += q_multsr_sat_32x32(accum31h, time, 62 - 31);

		accum31l = *filter_P++;
		accum31h = *filter_P++;
		accum31l += q_multsr_sat_32x32(accum420l, time, 62 - 31);
		accum31h += q_multsr_sat_32x32(accum420h, time, 62 - 31);

		accum420l = *filter_P++;
		accum420h = *filter_P++;
		accum420l += q_multsr_sat_32x32(accum31l, time, 62 - 31);
		accum420h += q_multsr_sat_32x32(accum31h, time, 62 - 31);

		*result_P++ = accum420l;
		*result_P++ = accum420h;
	}
}

void asrc_calc_impulse_response_n6(struct asrc_farrow *src_obj)
{
	/*
	 * See 'calc_impulse_response_n4' for a detailed description
	 * of the algorithm and data handling
	 */
	int32_t time;
	int32_t accum531l;
	int32_t accum531h;
	int32_t accum420l;
	int32_t accum420h;
	const int32_t *filter_P;
	int32_t *result_P;
	int index_filter;
	int index_limit;

	filter_P = &src_obj->polyphase_filters[0];
	result_P = &src_obj->impulse_response[0];
	time = sat_int32(((int64_t)src_obj->time_value) << 4);

	index_limit = src_obj->filter_length >> 1;
	for (index_filter = 0; index_filter < index_limit; index_filter++) {
		accum531l = *filter_P++;
		accum531h = *filter_P++;
		accum420l = *filter_P++;
		accum420h = *filter_P++;
		accum420l += q_multsr_sat_32x32(accum531l, time, 62 - 31);
		accum420h += q_multsr_sat_32x32(accum531h, time, 62 - 31);

		accum531l = *filter_P++;
		accum531h = *filter_P++;
		accum531l += q_multsr_sat_32x32(accum420l, time, 62 - 31);
		accum531h += q_multsr_sat_32x32(accum420h, time, 62 - 31);

		accum420l = *filter_P++;
		accum420h = *filter_P++;
		accum420l += q_multsr_sat_32x32(accum531l, time, 62 - 31);
		accum420h += q_multsr_sat_32x32(accum531h, time, 62 - 31);

		accum531l = *filter_P++;
		accum531h = *filter_P++;
		accum531l += q_multsr_sat_32x32(accum420l, time, 62 - 31);
		accum531h += q_multsr_sat_32x32(accum420h, time, 62 - 31);

		accum420l = *filter_P++;
		accum420h = *filter_P++;
		accum420l += q_multsr_sat_32x32(accum531l, time, 62 - 31);
		accum420h += q_multsr_sat_32x32(accum531h, time, 62 - 31);

		*result_P++ = accum420l;
		*result_P++ = accum420h;
	}
}

void asrc_calc_impulse_response_n7(struct asrc_farrow *src_obj)
{
	/*
	 * See 'calc_impulse_response_n4' for a detailed description
	 * of the algorithm and data handling
	 */
	int32_t time;
	int32_t accum6420l;
	int32_t accum6420h;
	int32_t accum531l;
	int32_t accum531h;
	const int32_t *filter_P;
	int32_t *result_P;
	int index_filter;
	int index_limit;

	filter_P = &src_obj->polyphase_filters[0];
	result_P = &src_obj->impulse_response[0];
	time = sat_int32(((int64_t)src_obj->time_value) << 4);

	index_limit = src_obj->filter_length >> 1;
	for (index_filter = 0; index_filter < index_limit; index_filter++) {
		accum6420l = *filter_P++;
		accum6420h = *filter_P++;
		accum531l = *filter_P++;
		accum531h = *filter_P++;
		accum531l += q_multsr_sat_32x32(accum6420l, time, 62 - 31);
		accum531h += q_multsr_sat_32x32(accum6420h, time, 62 - 31);

		accum6420l = *filter_P++;
		accum6420h = *filter_P++;
		accum6420l += q_multsr_sat_32x32(accum531l, time, 62 - 31);
		accum6420h += q_multsr_sat_32x32(accum531h, time, 62 - 31);

		accum531l = *filter_P++;
		accum531h = *filter_P++;
		accum531l += q_multsr_sat_32x32(accum6420l, time, 62 - 31);
		accum531h += q_multsr_sat_32x32(accum6420h, time, 62 - 31);

		accum6420l = *filter_P++;
		accum6420h = *filter_P++;
		accum6420l += q_multsr_sat_32x32(accum531l, time, 62 - 31);
		accum6420h += q_multsr_sat_32x32(accum531h, time, 62 - 31);

		accum531l = *filter_P++;
		accum531h = *filter_P++;
		accum531l += q_multsr_sat_32x32(accum6420l, time, 62 - 31);
		accum531h += q_multsr_sat_32x32(accum6420h, time, 62 - 31);

		accum6420l = *filter_P++;
		accum6420h = *filter_P++;
		accum6420l += q_multsr_sat_32x32(accum531l, time, 62 - 31);
		accum6420h += q_multsr_sat_32x32(accum531h, time, 62 - 31);

		*result_P++ = accum6420l;
		*result_P++ = accum6420h;
	}
}

#endif /* ASRC_GENERIC */
