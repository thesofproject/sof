// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2020 Intel Corporation. All rights reserved.
//
// Author: Bartosz Kokoszko <bartoszx.kokoszko@linux.intel.com>

#ifndef __SOF_MODULE_SERVICE_BUILD__
#include <sof/compiler_attributes.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/ipc-config.h>
#include <sof/trace/trace.h>
#include <sof/ipc/msg.h>
#include <rtos/init.h>
#include <sof/ut.h>

#ifndef __ZEPHYR__
#include <rtos/mutex.h>
#endif
#include <ipc4/module.h>

LOG_MODULE_REGISTER(smart_amp_test, CONFIG_SOF_LOG_LEVEL);
#include <zephyr/logging/log.h>

/* 167a961e-8ae4-11ea-89f1-000c29ce1635 */
DECLARE_SOF_RT_UUID("smart_amp-test", smart_amp_test_comp_uuid, 0x167a961e, 0x8ae4,
		    0x11ea, 0x89, 0xf1, 0x00, 0x0c, 0x29, 0xce, 0x16, 0x35);

DECLARE_TR_CTX(smart_amp_test_comp_tr, SOF_UUID(smart_amp_test_comp_uuid),
	       LOG_LEVEL_INFO);
#else
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include <module/base.h>
#include <module/interface.h>
#include <iadk/adsp_error_code.h>
#include <audio/source_api.h>
#include <audio/sink_api.h>
#include <ipc4/module.h>
#include <sof/math/numbers.h>
#endif
#include <sof/samples/audio/smart_amp_test.h>
#include <module/module/api_ver.h>
#include <rimage/sof/user/manifest.h>

typedef void (*smart_amp_proc)(int8_t const *src_ptr,
			       int8_t const *src_begin,
			       int8_t const *src_end,
			       int8_t *dst_ptr,
			       int8_t const *dst_begin,
			       int8_t const *dst_end,
			       uint32_t size);

struct smart_amp_data {
	struct sof_smart_amp_ipc4_config ipc4_cfg;
	struct sof_smart_amp_config config;
	smart_amp_proc process;
	uint32_t out_channels;
};

#ifdef __SOF_MODULE_SERVICE_BUILD__
static struct smart_amp_data smart_amp_priv;
#endif

/* When building as a loadable module, we need .bss to avoid rimage errors */
static int keep_bss __attribute__((used));

static int smart_amp_init(struct processing_module *mod)
{
	struct smart_amp_data *sad;
	struct module_data *mod_data = &mod->priv;
	int ret;
	const struct ipc4_base_module_extended_cfg *base_cfg = mod_data->cfg.init_data;

	if (!base_cfg) {
		LOG_ERR("smart_amp_init(): no module configuration");
		return -EINVAL;
	}

	LOG_DBG("smart_amp_init()");

#ifndef __SOF_MODULE_SERVICE_BUILD__
	sad = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, sizeof(*sad));
	if (!sad)
		return -ENOMEM;
#else
	sad = &smart_amp_priv;
#endif
	mod_data->private = sad;

	if (base_cfg->base_cfg_ext.nb_input_pins != SMART_AMP_NUM_IN_PINS ||
	    base_cfg->base_cfg_ext.nb_output_pins != SMART_AMP_NUM_OUT_PINS) {
		ret = -EINVAL;
		goto sad_fail;
	}

	/* Copy the pin formats */
	sad->ipc4_cfg.input_pins[0] =
		*((struct ipc4_input_pin_format *)base_cfg->base_cfg_ext.pin_formats);
	sad->ipc4_cfg.input_pins[1] =
		*((struct ipc4_input_pin_format *)base_cfg->base_cfg_ext.pin_formats + 1);
	sad->ipc4_cfg.output_pin =
		*(struct ipc4_output_pin_format *)(base_cfg->base_cfg_ext.pin_formats
			+ sizeof(sad->ipc4_cfg.input_pins));

	return 0;

sad_fail:
#ifndef __SOF_MODULE_SERVICE_BUILD__
	rfree(sad);
#endif
	return ret;
}

static int smart_amp_set_config(struct processing_module *mod, uint32_t config_id,
				enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				size_t response_size)
{
	struct smart_amp_data *sad = module_get_private_data(mod);

	LOG_DBG("smart_amp_set_config()");

	switch (config_id) {
	case SMART_AMP_SET_MODEL:
		return 0;
	case SMART_AMP_SET_CONFIG:
		if (fragment_size != sizeof(sad->config)) {
			LOG_ERR("smart_amp_set_config(): invalid config size %u, expect %u",
				fragment_size, sizeof(struct sof_smart_amp_config));
			return -EINVAL;
		}
		LOG_DBG("smart_amp_set_config(): config size = %u", fragment_size);
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

	LOG_DBG("smart_amp_get_config()");

	switch (config_id) {
	case SMART_AMP_GET_CONFIG:
		 *(struct sof_smart_amp_config *)fragment = sad->config;
		*data_offset_size = sizeof(sad->config);
		return 0;
	default:
		return -EINVAL;
	}
}

static int smart_amp_free(struct processing_module *mod)
{
	LOG_DBG("smart_amp_free()");

#ifndef __SOF_MODULE_SERVICE_BUILD__
	struct smart_amp_data *sad = module_get_private_data(mod);

	rfree(sad);
#endif

	return 0;
}

static void process_s16(int8_t const *src_ptr,
			int8_t const *src_begin,
			int8_t const *src_end,
			int8_t *dst_ptr,
			int8_t const *dst_begin,
			int8_t const *dst_end,
			uint32_t size)
{
	int16_t const *src_addr = (int16_t const *)src_ptr;
	int16_t *dst_addr = (int16_t *)dst_ptr;
	int count = size >> 1;
	int i;

	for (i = 0; i < count; i++) {
		if (src_addr >= (int16_t const *)src_end)
			src_addr = (int16_t const *)src_begin;

		if (dst_addr >= (int16_t *)dst_end)
			dst_addr = (int16_t *)dst_begin;

		*dst_addr = *src_addr;
		src_addr++;
		dst_addr++;
	}
}

static void process_s32(int8_t const *src_ptr,
			int8_t const *src_begin,
			int8_t const *src_end,
			int8_t *dst_ptr,
			int8_t const *dst_begin,
			int8_t const *dst_end,
			uint32_t size)
{
	int32_t const *src_addr = (int32_t const *)src_ptr;
	int32_t *dst_addr = (int32_t *)dst_ptr;
	int count = size >> 2;
	int i;

	for (i = 0; i < count; i++) {
		if (src_addr >= (int32_t const *)src_end)
			src_addr = (int32_t const *)src_begin;

		if (dst_addr >= (int32_t *)dst_end)
			dst_addr = (int32_t *)dst_begin;

		*dst_addr = *src_addr;
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
	int8_t const *src_ptr;
	int8_t const *src_begin;
	int8_t const *src_end;
	int8_t *dst_ptr;
	int8_t const *dst_begin;
	int8_t const *dst_end;
	size_t src_size;
	size_t dst_size;
	int ret;

	ret = sink_get_buffer(sink, size,
			      (void **)&dst_ptr,
			      (void **)&dst_begin,
			      &dst_size);
	if (ret)
		return ret;

	if (!feedback || !source_get_data_available(feedback))
		goto source;

	ret = source_get_data(feedback, size,
			      (void const **)&src_ptr,
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
	sad->process(src_ptr, src_begin, src_end, dst_ptr, dst_begin, dst_end, size);
	source_release_data(feedback, size);

source:
	ret = source_get_data(source, size,
			      (void const **)&src_ptr,
			      (void const **)&src_begin,
			      &src_size);
	if (ret)
		return ret;

	src_end = src_begin + src_size;
	dst_end = dst_begin + dst_size;
	sad->process(src_ptr, src_begin, src_end, dst_ptr, dst_begin, dst_end, size);
	source_release_data(source, size);
	sink_commit_buffer(sink, size);
	return 0;
}

static int smart_amp_process(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct sof_source *fb_input = NULL;
	/* if there is only one input stream, it should be the source input */
	struct sof_source *src_input = sources[0];
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
		/* feedback */
		avail = MIN(avail_passthrough, source_get_data_available(fb_input));
		avail = MIN(avail, sink_get_free_size(sinks[0]));
	} else {
		avail = MIN(avail_passthrough, sink_get_free_size(sinks[0]));
	}

	/* process data */
	ret = smart_amp_process_data(mod, src_input, fb_input, sinks[0], avail);

	return ret;
}

static int smart_amp_reset(struct processing_module *mod)
{
	LOG_DBG("smart_amp_reset()");

	return 0;
}

static smart_amp_proc get_smart_amp_process(struct sof_sink *sink)
{
	switch (sink_get_frm_fmt(sink)) {
	case SOF_IPC_FRAME_S16_LE:
		return process_s16;
	case SOF_IPC_FRAME_S24_4LE:
	case SOF_IPC_FRAME_S32_LE:
		return process_s32;
	default:
		LOG_ERR("smart_amp_process() error: not supported frame format");
		return NULL;
	}
}

static int smart_amp_prepare(struct processing_module *mod,
			     struct sof_source **sources, int num_of_sources,
			     struct sof_sink **sinks, int num_of_sinks)
{
	struct smart_amp_data *sad = module_get_private_data(mod);

	LOG_DBG("smart_amp_prepare()");

	sad->process = get_smart_amp_process(sinks[0]);
	if (!sad->process) {
		LOG_ERR("smart_amp_prepare(): get_smart_amp_process failed");
		return -EINVAL;
	}

	return 0;
}

static const struct module_interface smart_amp_test_interface = {
	.init = smart_amp_init,
	.prepare = smart_amp_prepare,
	.process = smart_amp_process,
	.set_configuration = smart_amp_set_config,
	.get_configuration = smart_amp_get_config,
	.reset = smart_amp_reset,
	.free = smart_amp_free
};

/*
 * We have to distinguish between 3 kinds of builds:
 * 1. built-in: MAJOR_IADSP_API_VERSION isn't defined
 * 2. system-service API: MAJOR_IADSP_API_VERSION and
 *    __SOF_MODULE_SERVICE_BUILD__ are defined
 * 3. dynamic linking: only MAJOR_IADSP_API_VERSION is defined
 */

#ifndef __SOF_MODULE_SERVICE_BUILD__
/* All builds except system-service API */

DECLARE_MODULE_ADAPTER(smart_amp_test_interface, smart_amp_test_comp_uuid, smart_amp_test_comp_tr);
/* DECLARE_MODULE_ADAPTER() creates
 * "sys_comp_module_<smart_amp_test_interface>_init()" (and a lot more)
 */
SOF_MODULE_INIT(smart_amp_test, sys_comp_module_smart_amp_test_interface_init);

#endif

#ifdef MAJOR_IADSP_API_VERSION
/* modular: system-services or dynamic link */

static const struct module_interface *loadable_module_main(void *mod_cfg,
							   void *parent_ppl,
							   void **mod_ptr)
{
	return &smart_amp_test_interface;
}

static const struct sof_man_module_manifest main_manifest __section(".module") __attribute__((used)) = {
	.module = {
		.name = "SMATEST",
		.uuid = {0x1E, 0x96, 0x7A, 0x16, 0xE4, 0x8A, 0xEA, 0x11,
			 0x89, 0xF1, 0x00, 0x0C, 0x29, 0xCE, 0x16, 0x35},
		.entry_point = (uint32_t)loadable_module_main,
		.type = {
#ifdef __SOF_MODULE_SERVICE_BUILD__
			.load_type = SOF_MAN_MOD_TYPE_MODULE,
#else
			.load_type = SOF_MAN_MOD_TYPE_LLEXT,
#endif
			.domain_ll = 1,
		},
		.affinity_mask = 1,
	}
};

#ifdef __SOF_MODULE_SERVICE_BUILD__
/* system services */

DECLARE_LOADABLE_MODULE_API_VERSION(smart_amp_test);

#else
/* dynamic link */

static const struct sof_module_api_build_info buildinfo __section(".mod_buildinfo") __attribute__((used)) = {
	.format = SOF_MODULE_API_BUILD_INFO_FORMAT,
	.api_version_number.full = SOF_MODULE_API_CURRENT_VERSION,
};

#endif

#endif
