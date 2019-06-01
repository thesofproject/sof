// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>

#include <sof/list.h>

#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

struct test_list_container {
	void *field1;
	struct list_item list;
	void *field2;
};

static void test_list_list_item_when_valid_offset_then_ptr_equal(void **state)
{
	(void) state; /* unused */

	struct test_list_container container;

	list_init(&(container.list));

	struct test_list_container *result_container = list_item(
			&(container.list), struct test_list_container, list);

	assert_ptr_equal(result_container, &container);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_list_list_item_when_valid_offset_then_ptr_equal),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
