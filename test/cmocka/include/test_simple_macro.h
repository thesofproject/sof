/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>
 */

#define c_u_t(x) cmocka_unit_test(x)

#define TEST_HERE_DECLARE(test_func, ...)\
	TEST_FUNC(TEST_PREFIX, test_func, __VA_ARGS__)

#define TEST_HERE_USE(test_func, postfix)\
	c_u_t(META_CONCAT_SEQ_DELIM_(TEST_PREFIX, test_func, postfix))
