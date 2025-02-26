// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2025 Intel Corporation. All rights reserved.
//
// Author: Adrian Bonislawski <adrian.bonislawski@intel.com>

#include "tester_simple_dram_test.h"
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/lib/memory.h>

/*
 * This is a simple test case for DRAM execution
 * The test case will copy every 2nd frame of data
 * by setting do_copy flag to true/false
 */

struct tester_module_simple_dram_test_data {
	bool do_copy_data;
};

static int validate_l3_memory(void *ptr)
{
	if (!((POINTER_TO_UINT(ptr) >= L3_MEM_BASE_ADDR) &&
	      (POINTER_TO_UINT(ptr) < L3_MEM_BASE_ADDR + L3_MEM_SIZE)))
		return -EINVAL;

	return 0;
}

__cold static int simple_dram_test_case_init(struct processing_module *mod, void **ctx)
{
#if !CONFIG_L3_HEAP
	return -EINVAL;
#endif
	struct tester_module_simple_dram_test_data *data =
		rzalloc(0, 0, SOF_MEM_CAPS_L3, sizeof(*data));

	if (!data)
		return -ENOMEM;

	if (validate_l3_memory(data) != 0) {
		rfree(data);
		return -EINVAL;
	}

	if (validate_l3_memory(tester_interface_simple_dram_test.init) != 0 ||
	    validate_l3_memory(tester_interface_simple_dram_test.process) != 0 ||
	    validate_l3_memory(tester_interface_simple_dram_test.free) != 0) {
		rfree(data);
		return -EINVAL;
	}

	data->do_copy_data = false;
	*ctx = data;
	return 0;
}

__cold static int simple_dram_test_case_process(void *ctx, struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks,
			    bool *do_copy)
{
	struct tester_module_simple_dram_test_data *data = ctx;

	/* copy every second cycle */
	*do_copy = data->do_copy_data;
	data->do_copy_data = !data->do_copy_data;

	return 0;
}

__cold static int simple_dram_test_free(void *ctx, struct processing_module *mod)
{
	rfree(ctx);
	return 0;
}

const struct tester_test_case_interface tester_interface_simple_dram_test = {
	.init = simple_dram_test_case_init,
	.process = simple_dram_test_case_process,
	.free = simple_dram_test_free
};
