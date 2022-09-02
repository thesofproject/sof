/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>
 */

#include <test_simple_macro.h>
#include <sof/trace/preproc.h>
#include <sof/sof.h>
#include <rtos/alloc.h>

/* CMOCKA SETUP */
#define setup_alloc(ptr, type, size, offset) do {\
	if (ptr)\
		free(ptr);\
	ptr = malloc(sizeof(type) * (size) + offset);\
	if (ptr == NULL)\
		return -1;\
} while (0)

#define setup_part(result, setup_func) do {\
	result |= setup_func;\
	if (result)\
		return result;\
} while (0)

/* TEST GROUPS GENERATORS */

#define gen_test_concat(prefix, name) META_CONCAT_SEQ_DELIM_(prefix, name)
#define gen_test_concat_base(prefix)  META_CONCAT_SEQ_DELIM_(prefix, base)
#define gen_test_concat_func(prefix, name, ...) \
META_CONCAT_SEQ_DELIM_(prefix, name, for, __VA_ARGS__)
#define gen_test_concat_flag(prefix, ...) \
META_CONCAT_SEQ_DELIM_(prefix, isetup, for, __VA_ARGS__)

// make function body for "${prefix}_${name}_for_${dlen}_${cbeg}_${cend}"
#define gen_test_with_prefix(prefix_check, prefix_setup, name, ...) \
static void gen_test_concat_func(prefix_setup, name, __VA_ARGS__) \
(void **state) {\
	if (!gen_test_concat_flag(prefix_setup, __VA_ARGS__)) {\
		if (_setup(state, __VA_ARGS__))\
			exit(1);\
		gen_test_concat_base(prefix_setup)(state);\
		++gen_test_concat_flag(prefix_setup, __VA_ARGS__);\
	} \
	gen_test_concat(prefix_check, name)(state);\
}

#define c_u_t_concat(prefix_check, prefix_setup, name, ...)\
	c_u_t(gen_test_concat_func(prefix_setup, name, __VA_ARGS__)),

/* make function bodies for
 * function group "${prefix}_for_${dlen}_${cbeg}_${cend}"
 */
#define gen_test_group(bind_macro, ...)\
	bind_macro(gen_test_with_prefix, __VA_ARGS__)

/* create 1 flag for
 * function group "${prefix}_for_${dlen}_${cbeg}_${cend}"
 */
#define flg_test_group(prefix, ...)\
	static int gen_test_concat_flag(prefix, __VA_ARGS__);

/* paste instances to
 * function grup "${prefix}_for_${dlen}_${cbeg}_${cend}"
 */
#define use_test_group(bind_macro, ...)\
	bind_macro(c_u_t_concat, __VA_ARGS__)

/* for example usage, see /test/cmocka/src/lib/lib/strcheck.c
 * section TEST GROUPS GENERATORS
 */
