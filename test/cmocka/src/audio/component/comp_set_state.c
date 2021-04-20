// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#include <sof/audio/component.h>
#include <errno.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <sof/sof.h>

enum test_type {
	SUCCEED = 0,
	FAIL,
	CORRECT_OUTPUT_STATE,

};

struct test_case {
	enum test_type type;
	uint16_t in_state;
	int cmd;
	uint16_t out_state;
	const char *name;
};

#define TEST_CASE(type, in_state, cmd, out_state) \
	{(type), (in_state), (cmd), (out_state), \
	("test_audio_component_comp_set_state__" \
	 #type "__" #in_state "__" #cmd "__" #out_state)}

/*
 * NULL_STATE enum is used in every case, when new state of component is
 * insignificant due to action of testing function
 */
enum {
	NULL_STATE = -1
};

struct test_case test_cases[] = {

	/*succeed set state*/
	TEST_CASE(SUCCEED,		COMP_STATE_PREPARE,
		COMP_TRIGGER_START,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_PAUSED,
		COMP_TRIGGER_RELEASE,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_ACTIVE,
		COMP_TRIGGER_STOP,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_PAUSED,
		COMP_TRIGGER_STOP,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_ACTIVE,
		COMP_TRIGGER_PAUSE,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_INIT,
		COMP_TRIGGER_RESET,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_SUSPEND,
		COMP_TRIGGER_RESET,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_PREPARE,
		COMP_TRIGGER_RESET,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_PAUSED,
		COMP_TRIGGER_RESET,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_ACTIVE,
		COMP_TRIGGER_RESET,	NULL_STATE),
	TEST_CASE(SUCCEED,		COMP_STATE_READY,
		COMP_TRIGGER_PREPARE,	NULL_STATE),

	/*fail set state*/
	TEST_CASE(FAIL,		COMP_STATE_INIT,	COMP_TRIGGER_START,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_READY,	COMP_TRIGGER_START,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_SUSPEND,	COMP_TRIGGER_START,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_PAUSED,	COMP_TRIGGER_START,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_INIT,	COMP_TRIGGER_RELEASE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_READY,	COMP_TRIGGER_RELEASE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_SUSPEND,	COMP_TRIGGER_RELEASE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_PREPARE,	COMP_TRIGGER_RELEASE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_INIT,	COMP_TRIGGER_STOP,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_READY,	COMP_TRIGGER_STOP,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_SUSPEND,	COMP_TRIGGER_STOP,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_INIT,	COMP_TRIGGER_PAUSE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_READY,	COMP_TRIGGER_PAUSE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_SUSPEND,	COMP_TRIGGER_PAUSE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_PREPARE,	COMP_TRIGGER_PAUSE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_INIT,	COMP_TRIGGER_PREPARE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_SUSPEND,	COMP_TRIGGER_PREPARE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_PAUSED,	COMP_TRIGGER_PREPARE,
		NULL_STATE),
	TEST_CASE(FAIL,		COMP_STATE_ACTIVE,	COMP_TRIGGER_PREPARE,
		NULL_STATE),
	
	/*correct output state*/
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_PREPARE,
		COMP_TRIGGER_START,	COMP_STATE_ACTIVE),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_PAUSED,
		COMP_TRIGGER_RELEASE,	COMP_STATE_ACTIVE),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_ACTIVE,
		COMP_TRIGGER_STOP,	COMP_STATE_PREPARE),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_PAUSED,
		COMP_TRIGGER_STOP,	COMP_STATE_PREPARE),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_INIT,
		COMP_TRIGGER_XRUN,	COMP_STATE_INIT),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_SUSPEND,
		COMP_TRIGGER_XRUN,	COMP_STATE_SUSPEND),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_PREPARE,
		COMP_TRIGGER_XRUN,	COMP_STATE_PREPARE),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_PAUSED,
		COMP_TRIGGER_XRUN,	COMP_STATE_PAUSED),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_ACTIVE,
		COMP_TRIGGER_XRUN,	COMP_STATE_ACTIVE),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_ACTIVE,
		COMP_TRIGGER_PAUSE,	COMP_STATE_PAUSED),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_INIT,
		COMP_TRIGGER_RESET,	COMP_STATE_READY),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_READY,
		COMP_TRIGGER_RESET,	COMP_STATE_READY),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_SUSPEND,
		COMP_TRIGGER_RESET,	COMP_STATE_READY),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_PREPARE,
		COMP_TRIGGER_RESET,	COMP_STATE_READY),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_PAUSED,
		COMP_TRIGGER_RESET,	COMP_STATE_READY),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_ACTIVE,
		COMP_TRIGGER_RESET,	COMP_STATE_READY),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_PREPARE,
		COMP_TRIGGER_PREPARE,	COMP_STATE_PREPARE),
	TEST_CASE(CORRECT_OUTPUT_STATE,		COMP_STATE_READY,
		COMP_TRIGGER_PREPARE,	COMP_STATE_PREPARE),
};

static void test_audio_component_comp_set_state_succeed(struct test_case *tc)
{
	struct comp_dev test_dev;

	test_dev.state = tc->in_state;

	assert_int_equal(comp_set_state(&test_dev, tc->cmd), 0);
}

static void test_audio_component_comp_set_state_correct_output_state(struct test_case *tc)
{
	struct comp_dev test_dev;

	test_dev.state = tc->in_state;
	comp_set_state(&test_dev, tc->cmd);

	assert_int_equal(test_dev.state, tc->out_state);
}

static void test_audio_component_comp_set_state_fail(struct test_case *tc)
{
	struct comp_dev test_drv;

	test_drv.state = tc->in_state;
	assert_int_equal(comp_set_state(&test_drv, tc->cmd), -EINVAL);
}

static void test_audio_component_comp_set_state(void **state)
{
	struct test_case *tc = *((struct test_case **) state);

	switch (tc->type) {
	case SUCCEED:
		test_audio_component_comp_set_state_succeed(tc);
		break;
	case FAIL:
		test_audio_component_comp_set_state_fail(tc);
		break;
	case CORRECT_OUTPUT_STATE:
		test_audio_component_comp_set_state_correct_output_state(tc);
		break;
	}
}

int main(void)
{
	struct CMUnitTest tests[ARRAY_SIZE(test_cases)];

	int i;

	for (i = 0; i < ARRAY_SIZE(test_cases); i++) {
		struct CMUnitTest *t = &tests[i];

		t->name = test_cases[i].name;
		t->test_func = test_audio_component_comp_set_state;
		t->initial_state = &test_cases[i];
		t->setup_func = NULL;
		t->teardown_func = NULL;

	}
	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
