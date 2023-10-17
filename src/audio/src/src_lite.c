// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Intel Corporation. All rights reserved.
//
// Author: Fabiola Jasinska <fabiola.jasinska@intel.com>

#include <rtos/init.h>
#include "src.h"
#include "src_config.h"

#define SRC_LITE 1

LOG_MODULE_REGISTER(src_lite, CONFIG_SOF_LOG_LEVEL);

static const struct module_interface src_lite_interface = {
	.init = src_init,
	.prepare = src_prepare,
	.process = src_process,
	.is_ready_to_process = src_is_ready_to_process,
	.set_configuration = src_set_config,
	.get_configuration = src_get_config,
	.reset = src_reset,
	.free = src_free,
};

DECLARE_SOF_RT_UUID("src_lite", src_lite_uuid, 0x33441051, 0x44CD, 0x466A,
		    0x83, 0xA3, 0x17, 0x84, 0x78, 0x70, 0x8A, 0xEA);

DECLARE_TR_CTX(src_lite_tr, SOF_UUID(src_lite_uuid), LOG_LEVEL_INFO);

DECLARE_MODULE_ADAPTER(src_lite_interface, src_lite_uuid, src_lite_tr);
SOF_MODULE_INIT(src_lite, sys_comp_module_src_lite_interface_init);
