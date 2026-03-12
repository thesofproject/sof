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
#include <sof/ipc/topology.h>
#include <ipc4/base-config.h>

#include <rtos/alloc.h>

extern void sys_comp_module_google_rtc_audio_processing_interface_init(void);
extern void sys_comp_module_google_ctc_audio_processing_interface_init(void);

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

#ifdef CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING
	sys_comp_module_google_rtc_audio_processing_interface_init();
#endif
#ifdef CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING
	sys_comp_module_google_ctc_audio_processing_interface_init();
#endif
	return NULL;
}

SOF_DEFINE_UUID("google_rtc", google_rtc_uuid,
		0xb780a0a6, 0x269f, 0x466f,
		0xb4, 0x77, 0x23, 0xdf, 0xa0, 0x5a, 0xf7, 0x58);

SOF_DEFINE_UUID("google_ctc", google_ctc_uuid,
		0xbf0e1bbc, 0xdc6a, 0x45fe,
		0xbc, 0x90, 0x25, 0x54, 0xcb, 0x13, 0x7a, 0xb4);

struct custom_ipc4_config_google {
	struct ipc4_base_module_cfg base;
} __attribute__((packed, aligned(8)));

ZTEST(audio_google, test_google_rtc_init)
{
#ifndef CONFIG_COMP_GOOGLE_RTC_AUDIO_PROCESSING
	ztest_test_skip();
#endif
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_google init_data;

	memset(&init_data, 0, sizeof(init_data));
	init_data.base.audio_fmt.channels_count = 2;
	init_data.base.audio_fmt.sampling_frequency = 48000;
	init_data.base.audio_fmt.depth = 16;
	init_data.base.audio_fmt.valid_bit_depth = 16;
	init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;
	init_data.base.ibs = 2048;
	init_data.base.obs = 2048;

	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(init_data);
	spec.data = (unsigned char *)&init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &google_rtc_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for google_rtc not found");

	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "google_rtc allocation failed");
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");

	drv->ops.free(comp);
}

ZTEST(audio_google, test_google_ctc_init)
{
#ifndef CONFIG_COMP_GOOGLE_CTC_AUDIO_PROCESSING
	ztest_test_skip();
#endif
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct custom_ipc4_config_google init_data;

	memset(&init_data, 0, sizeof(init_data));
	init_data.base.audio_fmt.channels_count = 2;
	init_data.base.audio_fmt.sampling_frequency = 48000;
	init_data.base.audio_fmt.depth = 16;
	init_data.base.audio_fmt.valid_bit_depth = 16;
	init_data.base.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;
	init_data.base.ibs = 2048;
	init_data.base.obs = 2048;

	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 2;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(init_data);
	spec.data = (unsigned char *)&init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &google_ctc_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for google_ctc not found");

	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "google_ctc allocation failed");
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");

	drv->ops.free(comp);
}

ZTEST_SUITE(audio_google, NULL, setup, NULL, NULL, NULL);
