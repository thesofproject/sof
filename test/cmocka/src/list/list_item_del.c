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

static void test_list_list_item_del_when_delete_head_then_tail_minus_1_prev_is_tail(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->head);

	assert_ptr_equal(data->tail_minus_1->prev, data->tail);
}

static void test_list_list_item_del_when_delete_head_then_tail_minus_1_next_is_tail(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->head);

	assert_ptr_equal(data->tail_minus_1->next, data->tail);
}

static void test_list_list_item_del_when_delete_head_then_tail_prev_is_tail_minus_1(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->head);

	assert_ptr_equal(data->tail->prev, data->tail_minus_1);
}

static void test_list_list_item_del_when_delete_head_then_tail_next_is_tail_minus_1(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->head);

	assert_ptr_equal(data->tail->next, data->tail_minus_1);
}

static void test_list_list_item_del_when_delete_tail_minus_1_then_head_prev_is_tail(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->tail_minus_1);

	assert_ptr_equal(data->head->prev, data->tail);
}

static void test_list_list_item_del_when_delete_tail_minus_1_then_head_next_is_tail(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->tail_minus_1);

	assert_ptr_equal(data->head->next, data->tail);
}

static void test_list_list_item_del_when_delete_tail_minus_1_then_tail_prev_is_head(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->tail_minus_1);

	assert_ptr_equal(data->tail->prev, data->head);
}

static void test_list_list_item_del_when_delete_tail_minus_1_then_tail_next_is_head(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->tail_minus_1);

	assert_ptr_equal(data->tail->next, data->head);
}

static void test_list_list_item_del_when_delete_tail_then_head_prev_is_tail_minus_1(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->tail);

	assert_ptr_equal(data->head->prev, data->tail_minus_1);
}

static void test_list_list_item_del_when_delete_tail_then_head_next_is_tail_minus_1(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->tail);

	assert_ptr_equal(data->head->next, data->tail_minus_1);
}

static void test_list_list_item_del_when_delete_tail_then_tail_minus_1_prev_is_head(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->tail);

	assert_ptr_equal(data->tail_minus_1->prev, data->head);
}

static void test_list_list_item_del_when_delete_tail_then_tail_minus_1_next_is_head(void **state)
{
	struct test_data *data = *state;

	list_item_del(data->tail);

	assert_ptr_equal(data->tail_minus_1->next, data->head);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_head_then_tail_minus_1_prev_is_tail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_head_then_tail_minus_1_next_is_tail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_head_then_tail_prev_is_tail_minus_1, setup, teardown),
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_head_then_tail_next_is_tail_minus_1, setup, teardown),

		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_tail_minus_1_then_head_prev_is_tail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_tail_minus_1_then_head_next_is_tail, setup, teardown),
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_tail_minus_1_then_tail_prev_is_head, setup, teardown),
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_tail_minus_1_then_tail_next_is_head, setup, teardown),

		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_tail_then_head_prev_is_tail_minus_1, setup, teardown),
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_tail_then_head_next_is_tail_minus_1, setup, teardown),
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_tail_then_tail_minus_1_prev_is_head, setup, teardown),
		cmocka_unit_test_setup_teardown(test_list_list_item_del_when_delete_tail_then_tail_minus_1_next_is_head, setup, teardown),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
