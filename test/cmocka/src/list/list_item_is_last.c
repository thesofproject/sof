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

struct test_data {
	struct list_item *head;
	struct list_item *tail_minus_1;
	struct list_item *tail;
};

static int setup(void **state)
{
	struct test_data *data = malloc(sizeof(struct test_data));

	if (!data)
		return -1;

	data->head = malloc(sizeof(struct list_item));
	data->tail_minus_1 = malloc(sizeof(struct list_item));
	data->tail = malloc(sizeof(struct list_item));

	if (!data->head	|| !data->tail_minus_1
			|| !data->tail) {
		free(data->head);
		free(data->tail_minus_1);
		free(data->tail);

		free(data);

		return -1;
	}

	list_init(data->head);
	list_init(data->tail_minus_1);
	list_init(data->tail);

	list_item_append(data->tail_minus_1, data->head);
	list_item_append(data->tail, data->head);

	*state = data;
	return 0;
}

static int teardown(void **state)
{
	struct test_data *data = *state;

	free(data->head);
	free(data->tail_minus_1);
	free(data->tail);

	free(data);
	return 0;
}

static void test_list_list_item_is_last_when_head_then_false(void **state)
{
	struct test_data *data = *state;

	assert_false(list_item_is_last(data->head, data->head));
}

static void test_list_list_item_is_last_when_tail_minus_1_then_false(void **state)
{
	struct test_data *data = *state;

	assert_false(list_item_is_last(data->tail_minus_1, data->head));
}

static void test_list_list_item_is_last_when_tail_then_true(void **state)
{
	struct test_data *data = *state;

	assert_true(list_item_is_last(data->tail, data->head));
}

static void test_list_list_item_is_last_when_not_in_list_then_false(void **state)
{
	struct list_item other_list;
	struct test_data *data = *state;

	list_init(&other_list);

	assert_false(list_item_is_last(&other_list, data->head));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_list_list_item_is_last_when_head_then_false),
		cmocka_unit_test(test_list_list_item_is_last_when_tail_minus_1_then_false),
		cmocka_unit_test(test_list_list_item_is_last_when_tail_then_true),
		cmocka_unit_test(test_list_list_item_is_last_when_not_in_list_then_false),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
