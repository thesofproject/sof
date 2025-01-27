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

/*
 * This function is 100% identical to src_prepare(), but it's
 * assigning different coefficient arrays because it's including
 * different headers.
 */
static int src_lite_prepare(struct processing_module *mod,
			    struct sof_source **sources, int num_of_sources,
			    struct sof_sink **sinks, int num_of_sinks)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct src_param *a = &cd->param;
	int ret;

	comp_info(mod->dev, "src_prepare()");

	if (num_of_sources != 1 || num_of_sinks != 1)
		return -EINVAL;

	a->in_fs = src_in_fs;
	a->out_fs = src_out_fs;
	a->num_in_fs = NUM_IN_FS;
	a->num_out_fs = NUM_OUT_FS;
	a->max_fir_delay_size_xnch = (PLATFORM_MAX_CHANNELS * MAX_FIR_DELAY_SIZE);
	a->max_out_delay_size_xnch = (PLATFORM_MAX_CHANNELS * MAX_OUT_DELAY_SIZE);

	ret = src_param_set(mod->dev, cd);
	if (ret < 0)
		return ret;

	ret = src_allocate_copy_stages(mod->dev, a,
				       src_table1[a->idx_out][a->idx_in],
				       src_table2[a->idx_out][a->idx_in]);
	if (ret < 0)
		return ret;

	ret = src_params_general(mod, sources[0], sinks[0]);
	if (ret < 0)
		return ret;

	return src_prepare_general(mod, sources[0], sinks[0]);
}

const struct module_interface src_lite_interface = {
	.init = src_init,
	.prepare = src_lite_prepare,
	.process = src_process,
	.is_ready_to_process = src_is_ready_to_process,
	.set_configuration = src_set_config,
	.get_configuration = src_get_config,
	.reset = src_reset,
	.free = src_free,
};

SOF_DEFINE_REG_UUID(src_lite);

DECLARE_TR_CTX(src_lite_tr, SOF_UUID(src_lite_uuid), LOG_LEVEL_INFO);

DECLARE_MODULE_ADAPTER(src_lite_interface, src_lite_uuid, src_lite_tr);
SOF_MODULE_INIT(src_lite, sys_comp_module_src_lite_interface_init);
