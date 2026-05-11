// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Fabiola Jasinska <fabiola.jasinska@intel.com>

#include <rtos/init.h>

#include "src_common.h"
#include "src_config.h"

#include "coef/src_lite_int32_define.h"
#include "coef/src_lite_int32_table.h"

LOG_MODULE_REGISTER(src_lite, CONFIG_SOF_LOG_LEVEL);

/* Set rate table pointers, compute rate indices, and copy filter stages.
 * Must be in src_lite.c because src_table1/2, src_in_fs, etc. come from
 * the coefficient headers included by this file.
 */
static int src_lite_setup_stages(struct processing_module *mod)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct src_param *a = &cd->param;
	int ret;

	a->in_fs = src_in_fs;
	a->out_fs = src_out_fs;
	a->num_in_fs = NUM_IN_FS;
	a->num_out_fs = NUM_OUT_FS;
	a->max_fir_delay_size_xnch = (PLATFORM_MAX_CHANNELS * MAX_FIR_DELAY_SIZE);
	a->max_out_delay_size_xnch = (PLATFORM_MAX_CHANNELS * MAX_OUT_DELAY_SIZE);

	ret = src_param_set(mod->dev, cd);
	if (ret < 0)
		return ret;

	return src_allocate_copy_stages(mod, a,
					src_table1[a->idx_out][a->idx_in],
					src_table2[a->idx_out][a->idx_in]);
}

static int src_lite_do_init(struct processing_module *mod)
{
	struct comp_data *cd;
	int ret;

	ret = src_init(mod);
	if (ret < 0)
		return ret;

	cd = module_get_private_data(mod);
	cd->setup_stages = src_lite_setup_stages;

	return src_init_stages(mod);
}

static int src_lite_prepare(struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks)
{
	comp_info(mod->dev, "entry");

	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	src_get_source_sink_params(mod->dev, sources[0], sinks[0]);

	return src_prepare_do(mod, sources[0], sinks[0]);
}

const struct module_interface src_lite_interface = {
	.init = src_lite_do_init,
	.prepare = src_lite_prepare,
	.process = src_process,
	.is_ready_to_process = src_is_ready_to_process,
	.reset = src_reset,
	.free = src_free,
};

SOF_DEFINE_REG_UUID(src_lite);

DECLARE_TR_CTX(src_lite_tr, SOF_UUID(src_lite_uuid), LOG_LEVEL_INFO);

#if !CONFIG_COMP_SRC_MODULE
DECLARE_MODULE_ADAPTER(src_lite_interface, src_lite_uuid, src_lite_tr);
SOF_MODULE_INIT(src_lite, sys_comp_module_src_lite_interface_init);
#endif
