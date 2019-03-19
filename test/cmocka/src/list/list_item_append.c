/*
 * Copyright (c) 2018, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Janusz Jankowski <janusz.jankowski@linux.intel.com>
 */

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

	if (data == NULL)
		return -1;

	data->head = malloc(sizeof(struct list_item));
	data->tail_minus_1 = malloc(sizeof(struct list_item));
	data->tail = malloc(sizeof(struct list_item));

	if (data->head == NULL
			|| data->tail_minus_1 == NULL
			|| data->tail == NULL) {
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

static void test_list_list_item_append_head_prev_is_tail(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->head->prev, data->tail);
}

static void test_list_list_item_append_head_next_is_tail_minus_1(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->head->next, data->tail_minus_1);
}

static void test_list_list_item_append_tail_minus_1_prev_is_head(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->tail_minus_1->prev, data->head);
}

static void test_list_list_item_append_tail_minus_1_next_is_tail(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->tail_minus_1->next, data->tail);
}

static void test_list_list_item_append_tail_prev_is_tail_minus_1(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->tail->prev, data->tail_minus_1);
}

static void test_list_list_item_append_tail_next_is_head(void **state)
{
	struct test_data *data = *state;

	assert_ptr_equal(data->tail->next, data->head);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_list_list_item_append_head_prev_is_tail),
		cmocka_unit_test(test_list_list_item_append_head_next_is_tail_minus_1),
		cmocka_unit_test(test_list_list_item_append_tail_minus_1_prev_is_head),
		cmocka_unit_test(test_list_list_item_append_tail_minus_1_next_is_tail),
		cmocka_unit_test(test_list_list_item_append_tail_prev_is_tail_minus_1),
		cmocka_unit_test(test_list_list_item_append_tail_next_is_head),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, setup, teardown);
}

