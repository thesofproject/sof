// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2026 Intel Corporation. All rights reserved.
//
// Author: Tomasz Leman <tomasz.m.leman@intel.com>

#include <zephyr/ztest.h>
#include <sof/math/numbers.h>
#include <stdint.h>

/**
 * @brief Test crc32 with an empty buffer
 *
 * With zero bytes the outer loop does not execute and the function returns
 * ~(~base) == 0 for base == 0.
 */
ZTEST(math_arithmetic_suite, test_math_numbers_crc32_empty_buffer)
{
	uint8_t data[1] = {0};
	uint32_t result = crc32(0, data, 0);

	zassert_equal(result, 0x00000000U,
		      "crc32 of empty buffer with base=0 should be 0x00000000");
}

/**
 * @brief Test crc32 with a single zero byte
 *
 * Exercises one iteration of the outer loop and all 8 iterations of the
 * inner bit-processing loop.
 */
ZTEST(math_arithmetic_suite, test_math_numbers_crc32_single_zero_byte)
{
	uint8_t data[] = {0x00};
	uint32_t result = crc32(0, data, sizeof(data));

	zassert_equal(result, 0xD202EF8DU,
		      "crc32({0x00}, base=0) should be 0xD202EF8D");
}

/**
 * @brief Test crc32 with a single 0xFF byte
 *
 * Exercises the branch where (cur & 1) is false in the inner loop.
 */
ZTEST(math_arithmetic_suite, test_math_numbers_crc32_single_ff_byte)
{
	uint8_t data[] = {0xFF};
	uint32_t result = crc32(0, data, sizeof(data));

	zassert_equal(result, 0xFF000000U,
		      "crc32({0xFF}, base=0) should be 0xFF000000");
}

/**
 * @brief Test crc32 against the well-known CRC-32 check value
 *
 * The string "123456789" must produce 0xCBF43926 — the standard reference
 * value for CRC-32 (ISO 3309 / ITU-T V.42).
 */
ZTEST(math_arithmetic_suite, test_math_numbers_crc32_known_vector_123456789)
{
	const uint8_t data[] = "123456789";
	uint32_t result = crc32(0, data, sizeof(data) - 1); /* exclude NUL */

	zassert_equal(result, 0xCBF43926U,
		      "crc32(\"123456789\", base=0) should be 0xCBF43926");
}

/**
 * @brief Test crc32 with a non-zero base
 *
 * A non-zero base changes the initial CRC register value (~base), allowing
 * incremental / chained CRC computation. Verifies the result differs from the
 * base=0 case and matches the expected precomputed value.
 */
ZTEST(math_arithmetic_suite, test_math_numbers_crc32_nonzero_base)
{
	uint8_t data[] = {0xAB, 0xCD, 0xEF};
	uint32_t result_base0   = crc32(0x00000000U, data, sizeof(data));
	uint32_t result_nonzero = crc32(0xDEADBEEFU, data, sizeof(data));

	zassert_equal(result_base0, 0x648D3D79U,
		      "crc32({0xAB,0xCD,0xEF}, base=0) should be 0x648D3D79");
	zassert_equal(result_nonzero, 0x1412F659U,
		      "crc32({0xAB,0xCD,0xEF}, base=0xDEADBEEF) should be 0x1412F659");
	zassert_not_equal(result_base0, result_nonzero,
			  "Different bases must yield different CRC values");
}

/**
 * @brief Test crc32 over a four-byte all-zeros buffer
 *
 * Exercises multiple iterations of the outer byte loop, producing a result
 * that differs from the single-byte zero-byte case.
 */
ZTEST(math_arithmetic_suite, test_math_numbers_crc32_four_zero_bytes)
{
	uint8_t data[] = {0x00, 0x00, 0x00, 0x00};
	uint32_t result = crc32(0, data, sizeof(data));

	zassert_equal(result, 0x2144DF1CU,
		      "crc32({0,0,0,0}, base=0) should be 0x2144DF1C");
}
