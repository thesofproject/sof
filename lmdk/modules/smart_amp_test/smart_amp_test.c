// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>
// Author: Andrula Song <andrula.song@intel.com>
// Author: Chao Song <chao.song@linux.intel.com>

#define MODULE_BUILD

#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include <module/base.h>
#include <module/api_ver.h>
#include <module/interface.h>
#include <iadk/adsp_error_code.h>
#include <rimage/sof/user/manifest.h>
#include <audio/source_api.h>
#include <audio/sink_api.h>
#include "smart_amp_test.h"
#include <ipc4/module.h>

static struct smart_amp_data smart_amp_priv;

static int smart_amp_init(struct processing_module *mod)
{
	const struct ipc4_base_module_extended_cfg *base_cfg;
	struct module_data *mod_data = &mod->priv;
	struct smart_amp_data *sad;
	uint8_t *pin_data;
	int bs;
	int ret;

	base_cfg = mod_data->cfg.init_data;
	sad = &smart_amp_priv;
	mod_data->private = sad;

	if (base_cfg->base_cfg_ext.nb_input_pins != SMART_AMP_NUM_IN_PINS ||
	    base_cfg->base_cfg_ext.nb_output_pins != SMART_AMP_NUM_OUT_PINS) {
		ret = -EINVAL;
		goto sad_fail;
	}

	pin_data = (uint8_t *)base_cfg->base_cfg_ext.pin_formats;
	sad->ipc4_cfg.input_pins[0] = *(struct ipc4_input_pin_format *)pin_data;
	sad->ipc4_cfg.input_pins[1] = *((struct ipc4_input_pin_format *)pin_data + 1);
	sad->ipc4_cfg.output_pin =
		*(struct ipc4_output_pin_format *)(pin_data + sizeof(sad->ipc4_cfg.input_pins));

	return 0;

sad_fail:
	return ret;
}

static int smart_amp_set_config(struct processing_module *mod, uint32_t config_id,
				enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				size_t response_size)
{
	struct smart_amp_data *sad = module_get_private_data(mod);

	switch (config_id) {
	case SMART_AMP_SET_MODEL:
		return 0;
	case SMART_AMP_SET_CONFIG:
		if (fragment_size != sizeof(sad->config))
			return -EINVAL;

		sad->config = *(struct sof_smart_amp_config *)fragment;
		return 0;
	default:
		return -EINVAL;
	}
}

static inline int smart_amp_get_config(struct processing_module *mod,
				       uint32_t config_id, uint32_t *data_offset_size,
				       uint8_t *fragment, size_t fragment_size)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	int ret;

	switch (config_id) {
	case SMART_AMP_GET_CONFIG:
		*(struct sof_smart_amp_config *)fragment = sad->config;
		*data_offset_size = sizeof(sad->config);
		return 0;
	default:
		return -EINVAL;
	}
}

static void process_s16(uint8_t *src_ptr,
			uint8_t const *src_begin,
			uint8_t const *src_end,
			uint8_t *dst_ptr,
			uint8_t const *dst_begin,
			uint8_t const *dst_end,
			uint32_t size,
			bool first)
{
	uint16_t *src_addr = (uint16_t *)src_ptr;
	uint16_t *dst_addr = (uint16_t *)dst_ptr;
	int i;

	for (i = 0; i < size; i++) {
		if (src_addr >= (uint16_t *)src_end)
			src_addr = (uint16_t *)src_begin;

		if (dst_addr >= (uint16_t *)dst_end)
			dst_addr = (uint16_t *)dst_begin;

		if (first)
			*dst_addr = *src_addr >> 2;
		else
			*dst_addr += *src_addr >> 2;

		src_addr++;
		dst_addr++;
	}
}

static void process_s32(uint8_t *src_ptr,
			uint8_t const *src_begin,
			uint8_t const *src_end,
			uint8_t *dst_ptr,
			uint8_t const *dst_begin,
			uint8_t const *dst_end,
			uint32_t size,
			bool first)
{
	uint32_t *src_addr = (uint32_t *)src_ptr;
	uint32_t *dst_addr = (uint32_t *)dst_ptr;
	int i;

	for (i = 0; i < size; i++) {
		if (src_addr >= (uint32_t *)src_end)
			src_addr = (uint32_t *)src_begin;

		if (dst_addr >= (uint32_t *)dst_end)
			dst_addr = (uint32_t *)dst_begin;

		if (first)
			*dst_addr = *src_addr >> 2;
		else
			*dst_addr += *src_addr >> 2;

		src_addr++;
		dst_addr++;
	}
}

static int smart_amp_process_data(struct processing_module *mod,
				  struct sof_source *source,
				  struct sof_source *feedback,
				  struct sof_sink *sink,
				  size_t size)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	uint8_t *src_ptr;
	uint8_t const *src_begin;
	uint8_t const *src_end;
	uint8_t *dst_ptr;
	uint8_t const *dst_begin;
	uint8_t const *dst_end;
	size_t src_size;
	size_t dst_size;
	int ret;

	ret = source_get_data(source, size,
			      (void const **)&src_ptr,
			      (void const **)&src_begin,
			      &src_size);
	if (ret)
		return ret;

	ret = sink_get_buffer(sink, size,
			      (void **)&dst_ptr,
			      (void **)&dst_begin,
			      &dst_size);
	if (ret) {
		source_release_data(source, 0);
		return ret;
	}

	src_end = src_begin + src_size;
	dst_end = dst_begin + dst_size;
	sad->process(src_ptr, src_begin, src_end, dst_ptr, dst_begin, dst_end, size, true);
	source_release_data(source, size);

	if (!feedback || !source_get_data_available(feedback))
		goto end;

	ret = source_get_data(feedback, size, (void const **)&src_ptr,
			      (void const **)&src_begin,
			      &src_size);
	if (ret) {
		if (ret == -EBUSY)
			return 0;
		else
			return ret;
	}

	src_end = src_begin + src_size;
	dst_end = dst_begin + dst_size;
	sad->process(src_ptr, src_begin, src_end, dst_ptr, dst_begin, dst_end, size, false);
	source_release_data(feedback, size);

end:
	sink_commit_buffer(sink, size);
	return 0;
}

static int smart_amp_process(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct smart_amp_data *sad = module_get_private_data(mod);
	struct sof_source *src_input = sources[0];
	struct sof_source *fb_input = NULL;
	uint32_t avail_passthrough;
	uint32_t avail = 0;
	uint32_t i;
	int ret;

	if (num_of_sources == SMART_AMP_NUM_IN_PINS) {
		for (i = 0; i < num_of_sources; i++) {
			if (IPC4_SINK_QUEUE_ID(source_get_id(sources[i])) ==
			    SOF_SMART_AMP_FEEDBACK_QUEUE_ID) {
				fb_input = sources[i];
			} else {
				src_input = sources[i];
			}
		}
	}

	avail_passthrough = source_get_data_available(src_input);
	if (fb_input && source_get_data_available(fb_input)) {
		avail = MIN(avail_passthrough, source_get_data_available(fb_input));
		avail = MIN(avail, sink_get_free_size(sinks[0]));
	} else {
		avail = MIN(avail_passthrough, sink_get_free_size(sinks[0]));
	}

	ret = smart_amp_process_data(mod, src_input, fb_input, sinks[0], avail);

	return ret;
}

static smart_amp_proc get_smart_amp_process(struct sof_sink *sink)
{
	switch (sink->audio_stream_params->valid_sample_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		return process_s16;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		return process_s32;
	default:
		return NULL;
	}
}

static int smart_amp_prepare(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct smart_amp_data *sad = module_get_private_data(mod);

	sad->process = get_smart_amp_process(sinks[0]);
	if (!sad->process)
		return -EINVAL;

	return 0;
}

static int smart_amp_free(struct processing_module *mod)
{
	return 0;
}

static int smart_amp_reset(struct processing_module *mod)
{
	return 0;
}

struct module_interface smart_amp_test_interface = {
	.init  = smart_amp_init,
	.prepare = smart_amp_prepare,
	.process = smart_amp_process,
	.set_configuration = smart_amp_set_config,
	.get_configuration = smart_amp_get_config,
	.reset = smart_amp_reset,
	.free = smart_amp_free
};

static void *loadable_module_main(void *mod_cfg, void *parent_ppl, void **mod_ptr)
{
	return &smart_amp_test_interface;
}

DECLARE_LOADABLE_MODULE_API_VERSION(smart_amp_test);

__attribute__((section(".module")))
const struct sof_man_module_manifest main_manifest = {
	.module = {
		.name = "SMATEST",
		.uuid = {0x1E, 0x96, 0x7A, 0x16, 0xE4, 0x8A, 0xEA, 0x11,
			 0x89, 0xF1, 0x00, 0x0C, 0x29, 0xCE, 0x16, 0x35},
		.entry_point = (uint32_t)loadable_module_main,
		.type = { .load_type = SOF_MAN_MOD_TYPE_MODULE,
		.domain_ll = 1 },
		.affinity_mask = 1,
	}
};
