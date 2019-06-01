// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Jakub Dabek <jakub.dabek@linux.intel.com>

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/edf_schedule.h>
#include "pipeline_mocks.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static void test_audio_pipeline_pipeline_new_memory_allocation(
	void **state)
{
	(void)state;

	/*Initialize structs for arguments*/
	struct sof_ipc_pipe_new pipe_desc = {.core = 1, .priority = 2};
	struct comp_dev *cd = malloc(sizeof(cd));
	struct pipeline *result;

	/*Memmory allocation values check. Pipeline can have those changed
	 *in future so expect errors here if any change to pipeline memory
	 *capabilities or memmory space was made
	 */
	expect_value(_zalloc, zone, RZONE_RUNTIME);
	expect_value(_zalloc, caps, SOF_MEM_CAPS_RAM);
	expect_value(_zalloc, bytes, sizeof(struct pipeline));

	/*Testing component*/
	result = pipeline_new(&pipe_desc, cd);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(
			test_audio_pipeline_pipeline_new_memory_allocation),
	};

	cmocka_set_message_output(CM_OUTPUT_TAP);

	return cmocka_run_group_tests(tests, NULL, NULL);
}
