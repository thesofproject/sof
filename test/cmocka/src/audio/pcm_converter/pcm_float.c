// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <sof/audio/pcm_converter.h>
#include <sof/audio/format.h>
#include <sof/common.h>
#include <sof/audio/buffer.h>
#include <ipc/stream.h>
#include <ipc/stream.h>

#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <cmocka.h>

#include "../../util.h"

/* used during float assertions */
#define EPSILON 0.01f

/* base number to check combinations for each data format */
#define PCM_TEST_INT_NUMBERS \
	-0, 0, 0, 0, -1, 1, -2, 2, \
	-3, 3, -4, 4, -5, 5, -6, 6, -7, 7, -25, 25, -57, 57, -100, 100
#define PCM_TEST_FLOAT_NUMBERS \
	-0, 0, 0.1, -0.1f, -0.8f, 0.8f, -1.9f, 1.9f, \
	-3, 3, -4, 4, -5, 5, -6, 6, -7, 7, -25, 25, -57, 57, -100, 100

/* generate biggest signed value on a 24 bits number */
#define INT24_MAX ((1 << (24 - 1)) - 1)

/* generate smallest signed value on a 24 bits number */
#define INT24_MIN (-INT24_MAX - 1)

/* conversion ration */
const float ratio16 = 1.f / (1u << 15);
const float ratio24 = 1.f / (1u << 23);
const float ratio32 = 1.f / (1u << 31);

/*
 * Function design to help debugging conversion between fixed and float numbers.
 *
 * Usage:
 * add declaration of this function for example to pcm_converter_generic.c
 * And feel free to use pcm_float_print_values in functions like
 * _pcm_convert_i_to_f or _pcm_convert_f_to_i during debugging
 *
 * param it int number to show
 * param fl pointer to float number to show
 * param ex expected number
 */
static void pcm_float_print_values(int32_t it, int32_t *fl, double ep,
				   const char *func_name)
{
	const int32_t mantissa_mask = (1 << 23) - 1;
	int32_t ex = ((*fl >> 23) & 0xFF) - 127;
	int32_t mt = *fl & mantissa_mask;
	float mantissa_value = mt * (1.0f / mantissa_mask) + 1.f;

	print_message("%s: 0x%08Xf => %.3f * 2**%3d = %10.3e f <=> %7d d \t(expected: %10.3e)\n",
		      func_name, *fl, mantissa_value, ex, *((float *)fl), it,
		      ep);
}

static struct comp_buffer *_test_pcm_convert(enum sof_ipc_frame frm_in, enum sof_ipc_frame frm_out,
					     int samples, const void *data)
{
	/* create buffers - output buffer may be too big for test */
	struct comp_buffer *source;
	struct comp_buffer *sink;
	pcm_converter_func fun;
	const uint8_t fillval = 0xAB;
	const int inbytes = samples * get_sample_bytes(frm_in);
	const int outbytes = (samples + 1) * get_sample_bytes(frm_out);

	/* create buffers */
	source = create_test_source(NULL, 0, frm_in, 1, inbytes);
	sink = create_test_sink(NULL, 0, frm_out, 1, outbytes);

	/* fill source */
	memcpy_s(source->stream.w_ptr, source->stream.size, data, inbytes);
	audio_stream_produce(&source->stream, inbytes);

	/* fill sink memory - to validate last value */
	memset(sink->stream.w_ptr, fillval, outbytes);

	/* run conversion */
	fun = pcm_get_conversion_function(frm_in, frm_out);
	assert_non_null(fun);
	fun(&source->stream, 0, &sink->stream, 0, samples);

	/* assert last value in sink is untouched */
	assert_int_equal(((uint8_t *)sink->stream.w_ptr)[outbytes - 1], fillval);

	/* free source and return sink */
	free_test_source(source);
	return sink;
}

static void mask_array(int32_t mask, int32_t *parray, int cnt)
{
	assert_non_null(parray);
	while (cnt--)
		parray[cnt] &= mask;
}

static void scale_array(float ratio, float *parray, int cnt)
{
	assert_non_null(parray);
	while (cnt--)
		parray[cnt] *= ratio;
}

#if CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S16LE
static void test_pcm_convert_s16_to_f(void **state)
{
	typedef int16_t Tin;
	typedef float Tout;
	static const Tin source_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
	};
	static Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	assert_int_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio16, expected_buf, N);

	/* run test */
	sink = _test_pcm_convert(SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_FLOAT,
				 N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		print_message("%2d/%02d ", i + 1, N);
		pcm_float_print_values(source_buf[i], (int32_t *)read_val,
				       expected_buf[i], __func__);
		assert_float_equal(*read_val, expected_buf[i], EPSILON);
	}

	/* free memory */
	free_test_sink(sink);
}

static void test_pcm_convert_f_to_s16(void **state)
{
	typedef float Tin;
	typedef int16_t Tout;
	static Tin source_buf[] = {
		PCM_TEST_FLOAT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
	};
	static const Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	assert_int_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio16, source_buf, N);

	/* run test */
	sink = _test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S16_LE,
				 N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		print_message("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, (int32_t *)&source_buf[i],
				       (float)expected_buf[i], __func__);
		assert_int_equal(*read_val, expected_buf[i]);
	}

	/* free memory */
	free_test_sink(sink);
}
#endif /* CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S24LE
static void test_pcm_convert_s24_in_s32_to_f(void **state)
{
	typedef int32_t Tin;
	typedef float Tout;
	static const Tin source_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
	};
	static Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	assert_int_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio24, expected_buf, N);

	/* run test */
	sink = _test_pcm_convert(SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_FLOAT,
				 N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		print_message("%2d/%02d ", i + 1, N);
		pcm_float_print_values(source_buf[i], (int32_t *)read_val,
				       expected_buf[i], __func__);
		assert_float_equal(*read_val, expected_buf[i], EPSILON);
	}

	/* free memory */
	free_test_sink(sink);
}

static void test_pcm_convert_s24_to_f(void **state)
{
	typedef int32_t Tin;
	typedef float Tout;
	static Tin source_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
	};
	static Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	assert_int_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	mask_array(MASK(24, 0), source_buf, N);
	scale_array(ratio24, expected_buf, N);

	/* run test */
	sink = _test_pcm_convert(SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_FLOAT,
				 N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		print_message("%2d/%02d ", i + 1, N);
		pcm_float_print_values(sign_extend_s24(source_buf[i]),
				       (int32_t *)read_val, expected_buf[i],
				       __func__);
		assert_float_equal(*read_val, expected_buf[i], EPSILON);
	}

	/* free memory */
	free_test_sink(sink);
}

static void test_pcm_convert_f_to_s24(void **state)
{
	typedef float Tin;
	typedef int32_t Tout;
	static Tin source_buf[] = {
		PCM_TEST_FLOAT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
		INT24_MIN - 1, INT24_MAX + 1,
	};
	static const Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
		INT24_MIN, INT24_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	assert_int_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio24, source_buf, N);

	/* run test */
	sink = _test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S24_4LE,
				 N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		print_message("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, (int32_t *)&source_buf[i],
				       (float)expected_buf[i], __func__);
		assert_int_equal(*read_val, expected_buf[i]);
	}

	/* free memory */
	free_test_sink(sink);
}
#endif /* CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S32LE
#define S24TOS32MULT (1 << (32 - 24))
static void test_pcm_convert_s32_to_f(void **state)
{
	typedef int32_t Tin;
	typedef float Tout;

	static const Tin source_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
		INT32_MIN + 1, INT32_MIN,
		INT32_MAX - 1, INT32_MAX,
	};
	static Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
		INT24_MIN*S24TOS32MULT, INT24_MIN * S24TOS32MULT,
		INT24_MAX*S24TOS32MULT, INT24_MAX * S24TOS32MULT,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	assert_int_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio32, expected_buf, N);

	/* run test */
	sink = _test_pcm_convert(SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_FLOAT,
				 N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		print_message("%2d/%02d ", i + 1, N);
		pcm_float_print_values(source_buf[i], (int32_t *)read_val,
				       expected_buf[i], __func__);
		assert_float_equal(*read_val, expected_buf[i], EPSILON);
	}

	/* free memory */
	free_test_sink(sink);
}

static void test_pcm_convert_f_to_s32(void **state)
{
	typedef float Tin;
	typedef int32_t Tout;
	static Tin source_buf[] = {
		PCM_TEST_FLOAT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
	};
	static const Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	assert_int_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio32, source_buf, N);

	/* run test */
	sink = _test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE,
				 N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		print_message("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, (int32_t *)&source_buf[i],
				       (float)expected_buf[i], __func__);
		assert_int_equal(*read_val, expected_buf[i]);
	}

	/* free memory */
	free_test_sink(sink);
}

static void test_pcm_convert_f_to_s32_big_neg(void **state)
{
	typedef float Tin;
	typedef int32_t Tout;
	static Tin source_buf[] = {
		INT24_MIN - 127, INT24_MIN - 128, /* 24B mantissa trimming */
		INT32_MIN + 1, INT32_MIN, /* 24B mantissa trimming */
		INT32_MIN * 2.f, INT32_MIN * 10.f,
		-INFINITY
	};
	static const Tout expected_buf[] = {
		INT24_MIN - 127, INT24_MIN - 128,
		INT32_MIN + 1, INT32_MIN,
		INT32_MIN, INT32_MIN,
		INT32_MIN
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	assert_int_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio32, source_buf, N);

	/* run test */
	sink = _test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE,
				 N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		print_message("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, (int32_t *)&source_buf[i],
				       (float)expected_buf[i], __func__);
		/* assert as float because of possible rounding effects */
		assert_float_equal(*read_val, expected_buf[i], 1);
	}

	/* free memory */
	free_test_sink(sink);
}

static void test_pcm_convert_f_to_s32_big_pos(void **state)
{
	typedef float Tin;
	typedef int32_t Tout;
	static Tin source_buf[] = {
		INT24_MAX + 127, INT24_MAX + 128,
		INT32_MAX - 255, INT32_MAX - 127,
		INT32_MAX - 1, INT32_MAX,
		INT32_MAX * 2.f, INT32_MAX * 10.f,
		INFINITY,
	};
	/* remember about 24B mantissa trimming */
	static const Tout expected_buf[] = {
		INT24_MAX + 127, INT24_MAX + 128,
		INT32_MAX - 254, INT32_MAX - 126,
		INT32_MAX - 1, INT32_MAX,
		INT32_MAX, INT32_MAX,
		INT32_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	assert_int_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio32, source_buf, N);

	/* run test */
	sink = _test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE,
				 N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		print_message("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, (int32_t *)&source_buf[i],
				       (float)expected_buf[i], __func__);
		/* assert as float because of possible rounding effects */
		assert_float_equal(*read_val, expected_buf[i], 1);
	}

	/* free memory */
	free_test_sink(sink);
}
#endif /* CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S32LE */

int main(void)
{
	const struct CMUnitTest tests[] = {
#if CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S16LE
		cmocka_unit_test(test_pcm_convert_s16_to_f),
		cmocka_unit_test(test_pcm_convert_f_to_s16),
#endif /* CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S24LE
		cmocka_unit_test(test_pcm_convert_s24_to_f),
		cmocka_unit_test(test_pcm_convert_s24_in_s32_to_f),
		cmocka_unit_test(test_pcm_convert_f_to_s24),
#endif /* CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S32LE
		cmocka_unit_test(test_pcm_convert_s32_to_f),
		cmocka_unit_test(test_pcm_convert_f_to_s32),
		cmocka_unit_test(test_pcm_convert_f_to_s32_big_neg),
		cmocka_unit_test(test_pcm_convert_f_to_s32_big_pos),
#endif /* CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S32LE */
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	/* log number of converting functions for current configuration */
	print_message("%s start tests, count(pcm_func_map)=%zu\n",
		      __FILE__, pcm_func_count);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
