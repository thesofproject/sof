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

	list_item_prepend(data->tail, data->head);
	list_item_prepend(data->tail_minus_1, data->head);

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

static void test_list_list_item_prepend_head_prev_is_tail(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->head->prev, data->tail);
}

static void test_list_list_item_prepend_head_next_is_tail_minus_1(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->head->next, data->tail_minus_1);
}

static void test_list_list_item_prepend_tail_minus_1_prev_is_head(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->tail_minus_1->prev, data->head);
}

static void test_list_list_item_prepend_tail_minus_1_next_is_tail(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->tail_minus_1->next, data->tail);
}

static void test_list_list_item_prepend_tail_prev_is_tail_minus_1(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->tail->prev, data->tail_minus_1);
}

static void test_list_list_item_prepend_tail_next_is_head(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->tail->next, data->head);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_list_list_item_prepend_head_prev_is_tail),
		cmocka_unit_test(test_list_list_item_prepend_head_next_is_tail_minus_1),
		cmocka_unit_test(test_list_list_item_prepend_tail_minus_1_prev_is_head),
		cmocka_unit_test(test_list_list_item_prepend_tail_minus_1_next_is_tail),
		cmocka_unit_test(test_list_list_item_prepend_tail_prev_is_tail_minus_1),
		cmocka_unit_test(test_list_list_item_prepend_tail_next_is_head),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}
