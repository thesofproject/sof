// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/list.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

static void test_list_list_init_prev_equal_to_root(void **state)
{
	(void) state; /* unused */

	struct list_item list = {.prev = NULL, .next = NULL};

	list_init(&list);

	assert_ptr_equal(&list, list.prev);
}

static void test_list_list_init_next_equal_to_root(void **state)
{
	(void) state; /* unused */

	struct list_item list = {.prev = NULL, .next = NULL};

	list_init(&list);

	assert_ptr_equal(&list, list.next);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_list_list_init_prev_equal_to_root),
		cmocka_unit_test(test_list_list_init_next_equal_to_root),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
