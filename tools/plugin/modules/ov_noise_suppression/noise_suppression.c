// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2024 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/pipeline.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <rtos/init.h>
#include <sof/lib/memory.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "noise_suppression_interface.h"

LOG_MODULE_REGISTER(ns, CONFIG_SOF_LOG_LEVEL);

/* 7ae671a7-4617-4a09-bf6d-9d29c998dbc1 */
SOF_DEFINE_UUID("ns", ns_uuid, 0x7ae671a7, 0x4617,
		    0x4a09, 0xbf, 0x6d, 0x9d, 0x29, 0xc9, 0x98, 0xdb, 0xc1);

DECLARE_TR_CTX(ns_comp_tr, SOF_UUID(ns_comp_uuid), LOG_LEVEL_INFO);

static int ns_free(struct processing_module *mod)
{
	ns_handle handle = module_get_private_data(mod);

	ov_ns_free(handle);
	return 0;
}

static int ns_init(struct processing_module *mod)
{
	struct module_data *mod_data = &mod->priv;

	return ov_ns_init(&mod_data->private);
}

static int
ns_process(struct processing_module *mod,
	   struct input_stream_buffer *input_buffers, int num_input_buffers,
	   struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	ns_handle handle = module_get_private_data(mod);
	int ret;

	ret = ov_ns_process(handle, input_buffers, num_input_buffers,
			    output_buffers, num_output_buffers);
	if (ret < 0)
		return ret;

	module_update_buffer_position(&input_buffers[0], &output_buffers[0], ret);

	return 0;
}

static const struct module_interface ns_interface = {
	.init = ns_init,
	.process_audio_stream = ns_process,
	.free = ns_free
};

DECLARE_MODULE_ADAPTER(ns_interface, ns_comp_uuid, ns_comp_tr);
SOF_MODULE_INIT(ns, sys_comp_module_ns_interface_init);
