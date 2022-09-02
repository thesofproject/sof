// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>

#include <rtos/alloc.h>

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#else
#include <stdlib.h>
#endif

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
		0,
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
			"0"
			"1"
			"36"
			"sizeof(\"" RELATIVE_FILE "\")"
			"sizeof(\"Message\")"
			"\"" RELATIVE_FILE "\""
			"\"Message\" "
		"}";
	(void)state;
	assert_string_equal(macro_result, should_be_eq);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_debugability_macros_declare_log_entry),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
