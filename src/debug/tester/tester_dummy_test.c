// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Marcin Szkudlinski <marcin.szkudlinski@linux.intel.com>

#include "tester_dummy_test.h"
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>

/*
 * This is a dummy test case
 * The test case will copy every 2nd frame of data
 * by setting do_copy flag to true/false
 */

struct tester_module_dummy_test_data {
	bool do_copy_data;
};

static int dummy_test_case_init(struct processing_module *mod, void **ctx)
{
	struct tester_module_dummy_test_data *data =
		rzalloc(SOF_MEM_ZONE_SYS_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(*data));

	if (!data)
		return -ENOMEM;

	data->do_copy_data = false;
	*ctx = data;
	return 0;
}

int dummy_test_case_process(void *ctx, struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks,
			    bool *do_copy)
{
	struct tester_module_dummy_test_data *data = ctx;

	/* copy every second cycle */
	*do_copy = data->do_copy_data;
	data->do_copy_data = !data->do_copy_data;

	return 0;
}

int dummy_test_free(void *ctx, struct processing_module *mod)
{
	rfree(ctx);
	return 0;
}

const struct tester_test_case_interface tester_interface_dummy_test = {
	.init = dummy_test_case_init,
	.process = dummy_test_case_process,
	.free = dummy_test_free
};
