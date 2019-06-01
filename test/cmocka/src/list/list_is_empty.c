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

static void test_list_list_is_empty_when_empty_then_true(void **state)
{
	(void) state; /* unused */

	struct list_item list;

	list_init(&list);

	assert_true(list_is_empty(&list));
}

static void test_list_list_is_empty_when_not_empty_then_false(void **state)
{
	(void) state; /* unused */

	struct list_item list, item;

	list_init(&list);
	list_init(&item);

	list_item_append(&item, &list);

	assert_false(list_is_empty(&list));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_list_list_is_empty_when_empty_then_true),
		cmocka_unit_test(test_list_list_is_empty_when_not_empty_then_false),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
