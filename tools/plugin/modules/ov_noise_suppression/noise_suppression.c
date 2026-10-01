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

SOF_DEFINE_REG_UUID(ns);

DECLARE_TR_CTX(ns_comp_tr, SOF_UUID(ns_uuid), LOG_LEVEL_INFO);

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
	   struct sof_source **sources, int num_of_sources,
	   struct sof_sink **sinks, int num_of_sinks)
{
	ns_handle handle = module_get_private_data(mod);
	struct sof_source *source = sources[0];
	struct sof_sink *sink = sinks[0];
	const size_t frame_bytes = source_get_frame_bytes(source);
	struct cir_buf_source src_desc;
	struct cir_buf_sink snk_desc;
	size_t buf_size;
	size_t copy_bytes;
	size_t frames;
	int ret;

	frames = source_get_data_frames_available(source);
	frames = MIN(frames, sink_get_free_frames(sink));

	/* Noise suppression keeps the stream format and channel count, so the
	 * source and sink consume and produce the same number of bytes.
	 */
	copy_bytes = frames * frame_bytes;
	if (copy_bytes == 0)
		return 0;

	ret = source_get_data(source, copy_bytes, &src_desc.ptr, &src_desc.buf_start, &buf_size);
	if (ret)
		return ret;

	src_desc.buf_end = (const char *)src_desc.buf_start + buf_size;

	ret = sink_get_buffer(sink, copy_bytes, &snk_desc.ptr, &snk_desc.buf_start, &buf_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	snk_desc.buf_end = (char *)snk_desc.buf_start + buf_size;

	ret = ov_ns_process(handle, &src_desc, &snk_desc, frames);
	if (ret < 0) {
		source_release_data(source, 0);
		sink_commit_buffer(sink, 0);
		return ret;
	}

	source_release_data(source, ret * frame_bytes);
	sink_commit_buffer(sink, ret * frame_bytes);

	return 0;
}

static const struct module_interface ns_interface = {
	.init = ns_init,
	.process = ns_process,
	.free = ns_free
};

DECLARE_MODULE_ADAPTER(ns_interface, ns_uuid, ns_comp_tr);
SOF_MODULE_INIT(ns, sys_comp_module_ns_interface_init);
