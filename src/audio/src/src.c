// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Tobiasz Dryjanski <tobiaszx.dryjanski@intel.com>

/*
 * This file has been extracted from src_common.c (formerly src.c)
 * to create separation between defines in different src types.
 */

#include <rtos/init.h>

#include "src_common.h"
#include "src_config.h"

#if SRC_SHORT || CONFIG_COMP_SRC_TINY
#include "coef/src_tiny_int16_define.h"
#include "coef/src_tiny_int16_table.h"
#elif CONFIG_COMP_SRC_SMALL
#include "coef/src_small_int32_define.h"
#include "coef/src_small_int32_table.h"
#elif CONFIG_COMP_SRC_STD
#include "coef/src_std_int32_define.h"
#include "coef/src_std_int32_table.h"
#elif CONFIG_COMP_SRC_IPC4_FULL_MATRIX
#include "coef/src_ipc4_int32_define.h"
#include "coef/src_ipc4_int32_table.h"
#else
#error "No valid configuration selected for SRC"
#endif

LOG_MODULE_DECLARE(src, CONFIG_SOF_LOG_LEVEL);

static int src_prepare(struct processing_module *mod,
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

	src_get_source_sink_params(mod->dev, sources[0], sinks[0]);

	ret = src_param_set(mod->dev, cd);
	if (ret < 0)
		return ret;

	a->stage1 = src_table1[a->idx_out][a->idx_in];
	a->stage2 = src_table2[a->idx_out][a->idx_in];

	ret = src_params_general(mod, sources[0], sinks[0]);
	if (ret < 0)
		return ret;

	return src_prepare_general(mod, sources[0], sinks[0]);
}

static const struct module_interface src_interface = {
	.init = src_init,
	.prepare = src_prepare,
	.process = src_process,
	.is_ready_to_process = src_is_ready_to_process,
	.set_configuration = src_set_config,
	.get_configuration = src_get_config,
	.reset = src_reset,
	.free = src_free,
};

DECLARE_MODULE_ADAPTER(src_interface, SRC_UUID, src_tr);
SOF_MODULE_INIT(src, sys_comp_module_src_interface_init);
