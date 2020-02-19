// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>

#include <sof/lib/alloc.h>

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdint.h>
#include <malloc.h>
#include <cmocka.h>

#include <sof/trace/preproc.h>
#include <sof/sof.h>
#include <sof/trace/trace.h>
#include <user/trace.h>

#define CAPTURE(aggr, ...)\
	META_RECURSE(META_MAP(1, META_QUOTE, aggr, __VA_ARGS__))

static void test_debugability_macros_declare_log_entry(void **state)
{
	const char *macro_result = CAPTURE(_DECLARE_LOG_ENTRY(
		LOG_LEVEL_CRITICAL,
		"Message",
		TRACE_CLASS_DMA,
		1
	));
	const char *should_be_eq =
		"__attribute__((section(\".static_log.\""
		" \"LOG_LEVEL_CRITICAL\"))) "
		"static const struct "
		"{ "
			"uint32_t level; "
			"uint32_t component_class; "
			"uint32_t params_num; "
			"uint32_t line_idx; "
			"uint32_t file_name_len; "
			"uint32_t text_len; "
			"const char file_name[sizeof(\"" RELATIVE_FILE "\")]; "
			"const char text[sizeof(\"Message\")]; "
		"} log_entry = { "
			"1"
			"(6 << 24)"
			"1"
			"31"
			"sizeof(\"" RELATIVE_FILE "\")"
			"sizeof(\"Message\")"
			"\"" RELATIVE_FILE "\""
			"\"Message\" "
		"}";
	(void)state;
	assert_string_equal(macro_result, should_be_eq);
}

static char *get_param_list(const int param_count)
{
	char *result = malloc(sizeof(char) * 128);
	int current;

	strcpy(result, "");
	for (current = 0; current < param_count; ++current) {
		char newparam[11];

		sprintf(newparam, ", param%d", current);
		strcat(result, newparam);
	}
	return result;
}

static char *get_should_be(const int param_count)
{
	char *result = malloc(sizeof(char) * 1024);
	char *paramlist = get_param_list(param_count);

	/* which format:  0 1 2 3 4 5 6 7 8*/
	sprintf(result, "%s%d%s%d%s%d%s%s%s",
	/*0*/"{ __attribute__((unused)) typedef char assertion_failed_"
	     META_QUOTE(BASE_LOG_ASSERT_FAIL_MSG)
	     "[(",
	/*1*/_TRACE_EVENT_MAX_ARGUMENT_COUNT,
	/*2*/" >= ",
	/*3*/param_count,
	/*4*/") ? 1 : -1]; _trace_event",
	/*5*/param_count,
	/*6*/" ((uint32_t)&log_entry, 0, 1, 1",
	/*7*/paramlist,
	/*8*/"); }"
	);
	if (paramlist)
		free(paramlist);
	// TODO: maybe remove all whitespace chars; they're not important here
	return result;
}

#define test_debugability_macros_base_base(...)			\
do {								\
	_DECLARE_LOG_ENTRY(					\
		LOG_LEVEL_CRITICAL,				\
		"Message",					\
		TRACE_CLASS_DMA,				\
		META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__)	\
	);							\
	const char *macro_result = CAPTURE(BASE_LOG(		\
		_trace_event,					\
		0,						\
		1,						\
		1,						\
		&log_entry,					\
		##__VA_ARGS__					\
	));							\
	char *should_be_eq = get_should_be(			\
		META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__));	\
								\
	/* to avoid "log_entry not used" warning */		\
	assert_true(log_entry.level == log_entry.level);	\
								\
	(void)state;						\
								\
	assert_string_equal(macro_result, should_be_eq);	\
	if (should_be_eq)					\
		free(should_be_eq);				\
} while (0)

#define TEST_FUNC_(param_count, ...)			\
static void META_CONCAT_SEQ_DELIM_(			\
	test_debugability_macros_base_log,		\
	param_count,					\
	params)						\
(void **state)						\
{							\
	test_debugability_macros_base_base(__VA_ARGS__);\
}

TEST_FUNC_(0,)
TEST_FUNC_(1, param0)
TEST_FUNC_(2, param0, param1)
TEST_FUNC_(3, param0, param1, param2)
TEST_FUNC_(4, param0, param1, param2, param3)
TEST_FUNC_(5, param0, param1, param2, param3, param4)
#undef TEST_FUNC_
#undef CAPTURE

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_debugability_macros_declare_log_entry),
		cmocka_unit_test(test_debugability_macros_base_log_0_params),
		cmocka_unit_test(test_debugability_macros_base_log_1_params),
		cmocka_unit_test(test_debugability_macros_base_log_2_params),
		cmocka_unit_test(test_debugability_macros_base_log_3_params),
		cmocka_unit_test(test_debugability_macros_base_log_4_params),
		cmocka_unit_test(test_debugability_macros_base_log_5_params),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
