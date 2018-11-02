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
 * Author: Michal Jerzy Wierzbicki <michalx.wierzbicki@linux.intel.com>
 */

#include <sof/alloc.h>

#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#include <sof/preproc.h>
#include <sof/sof.h>
#include <sof/trace.h>

#define CAPTURE(aggr, ...)\
	META_RECURSE(META_MAP(1, META_QUOTE, aggr, __VA_ARGS__))

static void test_debugability_macros_declare_log_entry(void **state)
{
	const char *macro_result = CAPTURE(_DECLARE_LOG_ENTRY(
		LOG_LEVEL_CRITICAL, "Message", TRACE_CLASS_DMA, 1));
	const char *should_be_eq =
		"__attribute__((section(\".static_log.\""
		" \"LOG_LEVEL_CRITICAL\"))) "
		"static const struct { uint32_t level; uint32_t component_id; "
		"uint32_t params_num; uint32_t line_idx; uint32_t file_name_len; "
		"const char file_name[sizeof(\"src/debugability/macros.c\")]; "
		"uint32_t text_len; const char text[sizeof(\"Message\")]; } "
		"log_entry = { 1";
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
	char *maybe_space = " ";
	char *maybe_comma = ",";

	if (!param_count)
		maybe_space = "";
	else
		maybe_comma = "";

	/* which format:  0 1 2 3 4 5 6 7 8 9 A*/
	sprintf(result, "%s%d%s%d%s%s%d%s%s%s%s",
	/*0*/"{ __attribute__((unused)) typedef char assertion_failed_"
	     META_QUOTE(BASE_LOG_ASSERT_FAIL_MSG)
	     "[(",
	/*1*/_TRACE_EVENT_MAX_ARGUMENT_COUNT,
	/*2*/" >= ",
	/*3*/param_count,
	/*4*/maybe_space,
	/*5*/") ? 1 : -1]; log_func log_function = (log_func)& _trace_event",
	/*6*/param_count,
	/*7*/"; log_function(&log_entry",
	/*8*/maybe_comma,
	/*9*/paramlist,
	/*A*/");}"
	);
	if (paramlist)
		free(paramlist);
	return result;
}

#define test_debugability_macros_base_base(...)\
do {\
	_DECLARE_LOG_ENTRY(\
		LOG_LEVEL_CRITICAL,\
		"Message",\
		TRACE_CLASS_DMA,\
		META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__)\
	);\
	const char *macro_result = CAPTURE(BASE_LOG(\
		_trace_event,\
		&log_entry,\
		__VA_ARGS__\
	));\
	char *should_be_eq = get_should_be(\
		META_COUNT_VARAGS_BEFORE_COMPILE(__VA_ARGS__));\
\
	/* to avoid "log_entry not used" warning */\
	assert_true(log_entry.level == log_entry.level);\
\
	(void)state;\
\
	assert_string_equal(macro_result, should_be_eq);\
	if (should_be_eq)\
		free(should_be_eq);\
} while (0)\

#define TEST_FUNC_(param_count, ...)\
static void META_CONCAT_SEQ_DELIM_(\
	test_debugability_macros_base_log,\
	param_count,\
	params)\
(void **state)\
{\
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

