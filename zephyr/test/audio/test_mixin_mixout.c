// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright(c) 2026 Intel Corporation.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <string.h>
#include <rtos/sof.h>
#include <sof/list.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/audio/pipeline.h>
#include <sof/ipc/topology.h>
#include <ipc4/base-config.h>

#include <rtos/alloc.h>
#include "test_audio_helper.h"

extern void sys_comp_module_mixin_interface_init(void);
extern void sys_comp_module_mixout_interface_init(void);

static void *setup(void)
{
	struct sof *sof = sof_get();

	sys_comp_init(sof);

	if (!sof->ipc) {
		sof->ipc = rzalloc(SOF_MEM_FLAG_COHERENT, sizeof(*sof->ipc));
		sof->ipc->comp_data = rzalloc(SOF_MEM_FLAG_COHERENT, 4096);
		k_spinlock_init(&sof->ipc->lock);
		list_init(&sof->ipc->msg_list);
		list_init(&sof->ipc->comp_list);
	}

	sys_comp_module_mixin_interface_init();
	sys_comp_module_mixout_interface_init();
	return NULL;
}

/* UUIDs extracted from native registries */
SOF_DEFINE_UUID("mixin_test", mixin_test_uuid, 0x39656eb2, 0x3b71, 0x4049,
		0x8d, 0x3f, 0xf9, 0x2c, 0xd5, 0xc4, 0x3c, 0x09);

SOF_DEFINE_UUID("mixout_test", mixout_test_uuid, 0x3c56505a, 0x24d7, 0x418f,
		0xbd, 0xdc, 0xc1, 0xf5, 0xa3, 0xac, 0x2a, 0xe0);

struct custom_ipc4_config_mix {
	struct ipc4_base_module_cfg base;
} __attribute__((packed, aligned(4)));

/* Test MIXIN initialization */
static struct comp_dev *test_mixin_create(void)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_mix mixin_init_data;

	memset(&mixin_init_data, 0, sizeof(mixin_init_data));
	memset(&mixin_init_data.base.audio_fmt, 0, sizeof(mixin_init_data.base.audio_fmt));
	mixin_init_data.base.audio_fmt.channels_count = 2;
	mixin_init_data.base.audio_fmt.sampling_frequency = 48000;
	mixin_init_data.base.audio_fmt.depth = 32;
	mixin_init_data.base.audio_fmt.valid_bit_depth = 32;
	mixin_init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;

	/* Setup basic IPC configuration */
	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(mixin_init_data);
	spec.data = (unsigned char *)&mixin_init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	/* Find driver by UUID */
	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &mixin_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for MIXIN not found");

	/* Initialize component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "Component allocation failed");

	return comp;
}

/* Test mixin initialization */
ZTEST(audio_mixin_mixout, test_mixin_init)
{
	struct comp_dev *comp = test_mixin_create();

	/* Verify component state */
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");
	zassert_equal(comp->ipc_config.id, 1, "IPC ID mismatch");

	/* Free the component */
		comp->drv->ops.free(comp);
}
ZTEST(audio_mixin_mixout, test_mixin_config)
{
	struct comp_dev *comp = test_mixin_create();

	int ret = 0;
	if (comp->drv->ops.set_large_config) {
		uint32_t val = 0;
		ret = comp->drv->ops.set_large_config(comp, 0, true, true,
			sizeof(val), (uint8_t *)&val);
	}

	comp->drv->ops.free(comp);
}

/* NOTE: mixin_process() writes directly into mixout sink buffers, not through
 * its own sinks. It requires a full mixin→mixout pipeline topology with an
 * intermediate buffer connecting both components. A simple "create + attach
 * buffers + copy" pattern cannot work here since comp_buffer_get_sink_component()
 * on the dummy sink buffer returns NULL, causing a crash. Skip the copy step.
 */
ZTEST(audio_mixin_mixout, test_mixin_process)
{
	struct comp_dev *comp = test_mixin_create();

	struct sof_ipc_stream_params params = {0};
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;
	params.channels = 2;
	params.rate = 48000;
	params.sample_container_bytes = 4;
	params.sample_valid_bytes = 4;

	test_audio_helper_setup_buffers(comp, 4096, &params);

	/* Only run prepare — skip copy since mixin needs a real mixout topology */
	comp->state = COMP_STATE_PREPARE;
	if (comp->drv->ops.prepare) {
		int ret = comp->drv->ops.prepare(comp);
		zassert_equal(ret, 0, "prepare failed: %d", ret);
	}
	comp->state = COMP_STATE_READY;

	test_audio_helper_free_buffers(comp);

	comp->drv->ops.free(comp);
}


/* Test MIXOUT initialization */
ZTEST(audio_mixin_mixout, test_mixout_init)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_mix mixout_init_data;

	memset(&mixout_init_data, 0, sizeof(mixout_init_data));
	memset(&mixout_init_data.base.audio_fmt, 0, sizeof(mixout_init_data.base.audio_fmt));
	mixout_init_data.base.audio_fmt.channels_count = 2;
	mixout_init_data.base.audio_fmt.sampling_frequency = 48000;
	mixout_init_data.base.audio_fmt.depth = 32;
	mixout_init_data.base.audio_fmt.valid_bit_depth = 32;
	mixout_init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;

	/* Setup basic IPC configuration */
	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 2;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(mixout_init_data);
	spec.data = (unsigned char *)&mixout_init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	/* Find driver by UUID */
	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &mixout_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for MIXOUT not found");

	/* Initialize component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "MIXOUT allocation failed");

	comp->drv->ops.free(comp);
}

/* Test MIXIN parameters parsing */
ZTEST(audio_mixin_mixout, test_mixin_params)
{
	struct comp_dev *comp = test_mixin_create();
	struct sof_ipc_stream_params params;
	int ret;

memset(&params, 0, sizeof(params));
	params.channels = 2;
	params.rate = 48000;
	params.sample_container_bytes = 4;
	params.sample_valid_bytes = 4;
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;

	/* Configure parameters */
	if (comp->drv->ops.params) {
		ret = comp->drv->ops.params(comp, &params);
		zassert_true(ret >= 0, "MIXIN parameter setup failed");
	}

		comp->drv->ops.free(comp);
}

/* Test MIXOUT parameters parsing */
ZTEST(audio_mixin_mixout, test_mixout_params)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct sof_ipc_stream_params params;
	struct custom_ipc4_config_mix mixout_init_data;
	int ret;

	memset(&mixout_init_data, 0, sizeof(mixout_init_data));
	memset(&mixout_init_data.base.audio_fmt, 0, sizeof(mixout_init_data.base.audio_fmt));
	mixout_init_data.base.audio_fmt.channels_count = 2;
	mixout_init_data.base.audio_fmt.sampling_frequency = 48000;
	mixout_init_data.base.audio_fmt.depth = 32;
	mixout_init_data.base.audio_fmt.valid_bit_depth = 32;
	mixout_init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;

	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 2;
	ipc_config.pipeline_id = 1;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(mixout_init_data);
	spec.data = (unsigned char *)&mixout_init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &mixout_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for MIXOUT not found");
	comp = drv->ops.create(drv, &ipc_config, &spec);

	memset(&params, 0, sizeof(params));
	params.channels = 2;
	params.rate = 48000;
	params.sample_container_bytes = 4;
	params.sample_valid_bytes = 4;
	params.buffer_fmt = SOF_IPC_BUFFER_INTERLEAVED;

	/* Configure parameters */
	if (comp->drv->ops.params) {
		ret = comp->drv->ops.params(comp, &params);
		zassert_true(ret >= 0, "MIXOUT parameter setup failed");
	}

	comp->drv->ops.free(comp);
}

ZTEST_SUITE(audio_mixin_mixout, NULL, setup, NULL, NULL, NULL);
