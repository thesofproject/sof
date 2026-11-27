// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation.
//
// Converted from CMock to Ztest
// Original tests from test/cmocka/src/audio/pcm_converter/pcm_float.c:
// Author: Karol Trzcinski <karolx.trzcinski@linux.intel.com>

#include <zephyr/ztest.h>

#include <sof/audio/pcm_converter.h>
#include <sof/audio/format.h>
#include <sof/audio/buffer.h>
#include <sof/common.h>
#include <ipc/stream.h>
#include <rtos/string.h>

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

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

/* conversion ratio */
static const float ratio16 = 1.f / (1u << 15);
static const float ratio24 = 1.f / (1u << 23);
static const float ratio32 = 1.f / (1u << 31);

/**
 * @brief Compare two floats like the CMocka assert_float_equal()
 *
 * The values are equal if they differ by at most epsilon, or by at most
 * FLT_EPSILON relative to the larger magnitude of the two.
 */
static bool float_equal(float left, float right, float epsilon)
{
	float diff = fabsf(left - right);
	float largest;

	if (diff <= epsilon)
		return true;

	largest = fabsf(right) > fabsf(left) ? fabsf(right) : fabsf(left);

	return diff <= largest * FLT_EPSILON;
}

/**
 * @brief Print an int sample and the float it converts from or to
 * @param[in] it Int value.
 * @param[in] fl Float value, printed also as its bits, mantissa and exponent.
 * @param[in] ep Expected value.
 * @param[in] func_name Name of the calling test.
 */
static void pcm_float_print_values(int32_t it, const float *fl, double ep,
				   const char *func_name)
{
	const uint32_t mantissa_mask = (1 << 23) - 1;
	uint32_t bits;
	int32_t ex;
	uint32_t mt;
	float mantissa_value;

	memcpy(&bits, fl, sizeof(bits));
	ex = (int32_t)((bits >> 23) & 0xFF) - 127;
	mt = bits & mantissa_mask;
	mantissa_value = mt * (1.0f / mantissa_mask) + 1.f;

	printk("%s: 0x%08Xf => %.3f * 2**%3d = %10.3e f <=> %7d d \t(expected: %10.3e)\n",
	       func_name, bits, (double)mantissa_value, ex, (double)*fl, it, ep);
}

/**
 * @brief Allocate a component buffer for the given frame format
 * @param[in] frame_fmt Frame format of the stream.
 * @param[in] size Buffer size in bytes.
 * @return The new buffer, never NULL.
 */
static struct comp_buffer *test_buffer_new(enum sof_ipc_frame frame_fmt, uint32_t size)
{
	struct sof_ipc_buffer desc = {.size = size};
	struct comp_buffer *buf = buffer_new(NULL, &desc, BUFFER_USAGE_NOT_SHARED);

	zassert_not_null(buf, "Failed to allocate a %u byte buffer.", size);

	memset(buf->stream.addr, 0, size);
	audio_stream_set_frm_fmt(&buf->stream, frame_fmt);
	audio_stream_set_channels(&buf->stream, 1);

	return buf;
}

/**
 * @brief Convert samples between two formats with the PCM converter
 *
 * The sink is one sample larger than needed, its last byte is checked to be
 * left untouched by the conversion.
 *
 * @param[in] frm_in Source frame format.
 * @param[in] frm_out Sink frame format.
 * @param[in] samples Number of samples to convert.
 * @param[in] data Source samples.
 * @return The sink buffer with the converted samples, to be freed by the caller.
 */
static struct comp_buffer *test_pcm_convert(enum sof_ipc_frame frm_in, enum sof_ipc_frame frm_out,
					    int samples, const void *data)
{
	struct comp_buffer *source;
	struct comp_buffer *sink;
	pcm_converter_func fun;
	const uint8_t fillval = 0xAB;
	const int inbytes = samples * get_sample_bytes(frm_in);
	const int outbytes = (samples + 1) * get_sample_bytes(frm_out);

	/* create buffers - output buffer may be too big for test */
	source = test_buffer_new(frm_in, inbytes);
	sink = test_buffer_new(frm_out, outbytes);

	/* fill source */
	memcpy_s(source->stream.w_ptr, source->stream.size, data, inbytes);
	audio_stream_produce(&source->stream, inbytes);

	/* fill sink memory - to validate last value */
	memset(sink->stream.w_ptr, fillval, outbytes);

	/* run conversion */
	fun = pcm_get_conversion_function(frm_in, frm_out);
	zassert_not_null(fun, "No conversion function from format %d to %d", frm_in, frm_out);

	struct cir_buf_source cir_src = {
		.buf_start = source->stream.addr,
		.buf_end = source->stream.end_addr,
		.ptr = source->stream.r_ptr,
	};
	struct cir_buf_sink cir_snk = {
		.buf_start = sink->stream.addr,
		.buf_end = sink->stream.end_addr,
		.ptr = sink->stream.w_ptr,
	};

	fun(&cir_src, audio_stream_get_channels(&source->stream),
	    &cir_snk, audio_stream_get_channels(&sink->stream), samples, DUMMY_CHMAP);

	/* assert last value in sink is untouched */
	zassert_equal(((uint8_t *)sink->stream.w_ptr)[outbytes - 1], fillval,
		      "Conversion wrote past the last sample");

	/* free source and return sink */
	buffer_free(source);
	return sink;
}

static void mask_array(int32_t mask, int32_t *parray, int cnt)
{
	zassert_not_null(parray);
	while (cnt--)
		parray[cnt] &= mask;
}

static void scale_array(float ratio, float *parray, int cnt)
{
	zassert_not_null(parray);
	while (cnt--)
		parray[cnt] *= ratio;
}

#if CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S16LE
/**
 * @brief Test S16_LE to float conversion
 */
ZTEST(audio_pcm_float_suite, test_pcm_convert_s16_to_f)
{
	typedef int16_t Tin;
	typedef float Tout;
	static const Tin source_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
	};
	Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	zassert_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio16, expected_buf, N);

	/* run test */
	sink = test_pcm_convert(SOF_IPC_FRAME_S16_LE, SOF_IPC_FRAME_FLOAT,
				N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		printk("%2d/%02d ", i + 1, N);
		pcm_float_print_values(source_buf[i], read_val, (double)expected_buf[i],
				       __func__);
		zassert_true(float_equal(*read_val, expected_buf[i], EPSILON),
			     "sample %d: %e != %e", i, (double)*read_val,
			     (double)expected_buf[i]);
	}

	/* free memory */
	buffer_free(sink);
}

/**
 * @brief Test float to S16_LE conversion
 */
ZTEST(audio_pcm_float_suite, test_pcm_convert_f_to_s16)
{
	typedef float Tin;
	typedef int16_t Tout;
	Tin source_buf[] = {
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

	zassert_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio16, source_buf, N);

	/* run test */
	sink = test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S16_LE,
				N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		printk("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, &source_buf[i], (double)expected_buf[i],
				       __func__);
		zassert_equal(*read_val, expected_buf[i], "sample %d: %d != %d", i,
			      *read_val, expected_buf[i]);
	}

	/* free memory */
	buffer_free(sink);
}
#endif /* CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S24LE
/**
 * @brief Test S24_4LE to float conversion of sign extended samples
 */
ZTEST(audio_pcm_float_suite, test_pcm_convert_s24_in_s32_to_f)
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
	Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	zassert_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio24, expected_buf, N);

	/* run test */
	sink = test_pcm_convert(SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_FLOAT,
				N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		printk("%2d/%02d ", i + 1, N);
		pcm_float_print_values(source_buf[i], read_val, (double)expected_buf[i],
				       __func__);
		zassert_true(float_equal(*read_val, expected_buf[i], EPSILON),
			     "sample %d: %e != %e", i, (double)*read_val,
			     (double)expected_buf[i]);
	}

	/* free memory */
	buffer_free(sink);
}

/**
 * @brief Test S24_4LE to float conversion of samples with a zero top byte
 */
ZTEST(audio_pcm_float_suite, test_pcm_convert_s24_to_f)
{
	typedef int32_t Tin;
	typedef float Tout;
	Tin source_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
	};
	Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	zassert_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	mask_array(MASK(24, 0), source_buf, N);
	scale_array(ratio24, expected_buf, N);

	/* run test */
	sink = test_pcm_convert(SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_FLOAT,
				N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		printk("%2d/%02d ", i + 1, N);
		pcm_float_print_values(sign_extend_s24(source_buf[i]), read_val,
				       (double)expected_buf[i], __func__);
		zassert_true(float_equal(*read_val, expected_buf[i], EPSILON),
			     "sample %d: %e != %e", i, (double)*read_val,
			     (double)expected_buf[i]);
	}

	/* free memory */
	buffer_free(sink);
}

/**
 * @brief Test float to S24_4LE conversion, including saturation
 */
ZTEST(audio_pcm_float_suite, test_pcm_convert_f_to_s24)
{
	typedef float Tin;
	typedef int32_t Tout;
	Tin source_buf[] = {
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

	zassert_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio24, source_buf, N);

	/* run test */
	sink = test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S24_4LE,
				N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		printk("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, &source_buf[i], (double)expected_buf[i],
				       __func__);
		zassert_equal(*read_val, expected_buf[i], "sample %d: %d != %d", i,
			      *read_val, expected_buf[i]);
	}

	/* free memory */
	buffer_free(sink);
}
#endif /* CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S32LE
#define S24TOS32MULT (1 << (32 - 24))
/**
 * @brief Test S32_LE to float conversion
 */
ZTEST(audio_pcm_float_suite, test_pcm_convert_s32_to_f)
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
	Tout expected_buf[] = {
		PCM_TEST_INT_NUMBERS,
		INT16_MIN + 1, INT16_MIN,
		INT16_MAX - 1, INT16_MAX,
		INT24_MIN + 1, INT24_MIN,
		INT24_MAX - 1, INT24_MAX,
		INT24_MIN * S24TOS32MULT, INT24_MIN * S24TOS32MULT,
		INT24_MAX * S24TOS32MULT, INT24_MAX * S24TOS32MULT,
	};

	struct comp_buffer *sink;
	int i, N = ARRAY_SIZE(source_buf);
	Tout *read_val;

	zassert_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio32, expected_buf, N);

	/* run test */
	sink = test_pcm_convert(SOF_IPC_FRAME_S32_LE, SOF_IPC_FRAME_FLOAT,
				N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		printk("%2d/%02d ", i + 1, N);
		pcm_float_print_values(source_buf[i], read_val, (double)expected_buf[i],
				       __func__);
		zassert_true(float_equal(*read_val, expected_buf[i], EPSILON),
			     "sample %d: %e != %e", i, (double)*read_val,
			     (double)expected_buf[i]);
	}

	/* free memory */
	buffer_free(sink);
}

/**
 * @brief Test float to S32_LE conversion
 */
ZTEST(audio_pcm_float_suite, test_pcm_convert_f_to_s32)
{
	typedef float Tin;
	typedef int32_t Tout;
	Tin source_buf[] = {
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

	zassert_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio32, source_buf, N);

	/* run test */
	sink = test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE,
				N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		printk("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, &source_buf[i], (double)expected_buf[i],
				       __func__);
		zassert_equal(*read_val, expected_buf[i], "sample %d: %d != %d", i,
			      *read_val, expected_buf[i]);
	}

	/* free memory */
	buffer_free(sink);
}

/**
 * @brief Test float to S32_LE conversion of large negative values
 */
ZTEST(audio_pcm_float_suite, test_pcm_convert_f_to_s32_big_neg)
{
	typedef float Tin;
	typedef int32_t Tout;
	Tin source_buf[] = {
		INT24_MIN - 127, INT24_MIN - 128, /* 24B mantissa trimming */
		/* 24B mantissa trimming, explicit casts as clang rejects implicit lossy ones */
		(float)(INT32_MIN + 1), INT32_MIN,
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

	zassert_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio32, source_buf, N);

	/* run test */
	sink = test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE,
				N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		printk("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, &source_buf[i], (double)expected_buf[i],
				       __func__);
		/* assert as float because of possible rounding effects */
		zassert_true(float_equal((float)*read_val, (float)expected_buf[i], 1),
			     "sample %d: %d != %d", i, *read_val, expected_buf[i]);
	}

	/* free memory */
	buffer_free(sink);
}

/**
 * @brief Test float to S32_LE conversion of large positive values
 */
ZTEST(audio_pcm_float_suite, test_pcm_convert_f_to_s32_big_pos)
{
	typedef float Tin;
	typedef int32_t Tout;
	Tin source_buf[] = {
		INT24_MAX + 127, INT24_MAX + 128,
		INT32_MAX - 255, INT32_MAX - 127,
		/* 24B mantissa trimming, explicit casts as clang rejects implicit lossy ones */
		(float)(INT32_MAX - 1), (float)INT32_MAX,
		(float)INT32_MAX * 2.f, (float)INT32_MAX * 10.f,
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

	zassert_equal(ARRAY_SIZE(source_buf), ARRAY_SIZE(expected_buf));
	scale_array(ratio32, source_buf, N);

	/* run test */
	sink = test_pcm_convert(SOF_IPC_FRAME_FLOAT, SOF_IPC_FRAME_S32_LE,
				N, source_buf);

	/* check results */
	for (i = 0; i < N; ++i) {
		read_val = audio_stream_read_frag(&sink->stream, i, sizeof(Tout));
		printk("%2d/%02d ", i + 1, N);
		pcm_float_print_values(*read_val, &source_buf[i], (double)expected_buf[i],
				       __func__);
		/* assert as float because of possible rounding effects */
		zassert_true(float_equal((float)*read_val, (float)expected_buf[i], 1),
			     "sample %d: %d != %d", i, *read_val, expected_buf[i]);
	}

	/* free memory */
	buffer_free(sink);
}
#endif /* CONFIG_FORMAT_FLOAT && CONFIG_FORMAT_S32LE */

static void *audio_pcm_float_setup(void)
{
	/* log number of converting functions for current configuration */
	printk("%s start tests, count(pcm_func_map)=%zu\n", __FILE__, pcm_func_count);

	return NULL;
}

ZTEST_SUITE(audio_pcm_float_suite, NULL, audio_pcm_float_setup, NULL, NULL, NULL);
