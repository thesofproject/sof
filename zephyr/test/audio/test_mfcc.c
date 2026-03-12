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
#include <user/mfcc.h>

#include <rtos/alloc.h>

extern void sys_comp_module_mfcc_interface_init(void);

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

	sys_comp_module_mfcc_interface_init();
	return NULL;
}

SOF_DEFINE_UUID("mfcc_test", mfcc_test_uuid,
		0xdb10a773, 0x1aa4, 0x4cea,
		0xa2, 0x1f, 0x2d, 0x57, 0xa5, 0xc9, 0x82, 0xeb);

/* Test MFCC initialization */
ZTEST(audio_mfcc, test_mfcc_init)
{
	struct comp_dev *comp;
	struct comp_ipc_config ipc_config;
	struct ipc_config_process spec;
	struct sof_mfcc_config mfcc_init_data;
	struct ipc4_base_module_cfg base_cfg;

	/* Prepare core parameter configuration struct layout */
	memset(&mfcc_init_data, 0, sizeof(mfcc_init_data));
	memset(&base_cfg, 0, sizeof(base_cfg));

	base_cfg.audio_fmt.channels_count = 2;
	base_cfg.audio_fmt.sampling_frequency = 16000;
	base_cfg.audio_fmt.depth = 16;
	base_cfg.audio_fmt.valid_bit_depth = 16;
	base_cfg.audio_fmt.interleaving_style = IPC4_CHANNELS_INTERLEAVED;
	base_cfg.ibs = 1024;
	base_cfg.obs = 200;

	/* 
	 * Map ipc4_base_module_cfg natively to the 32 byte prefix of 
	 * sof_mfcc_config (size+reserved values format mappings)
	 */
	memcpy(&mfcc_init_data, &base_cfg, sizeof(base_cfg));

	/* Prepare algorithm variables safely natively */
	mfcc_init_data.size = sizeof(struct sof_mfcc_config);
	mfcc_init_data.sample_frequency = 16000;
	mfcc_init_data.channel = 0;
	mfcc_init_data.frame_length = 400;
	mfcc_init_data.frame_shift = 160;
	mfcc_init_data.num_mel_bins = 23;
	mfcc_init_data.num_ceps = 13;
	mfcc_init_data.dct = MFCC_DCT_II;
	mfcc_init_data.window = MFCC_BLACKMAN_WINDOW;
	mfcc_init_data.blackman_coef = MFCC_BLACKMAN_A0;
	mfcc_init_data.cepstral_lifter = 22 << 9;
	mfcc_init_data.round_to_power_of_two = true;
	mfcc_init_data.snip_edges = true;

	/* Setup basic IPC configuration */
	memset(&ipc_config, 0, sizeof(ipc_config));
	ipc_config.id = 1;
	ipc_config.pipeline_id = 1;
	ipc_config.core = 0;

	memset(&spec, 0, sizeof(spec));
	spec.size = sizeof(mfcc_init_data);
	spec.data = (unsigned char *)&mfcc_init_data;

	struct list_item *clist;
	const struct comp_driver *drv = NULL;

	/* Find driver by UUID */
	list_for_item(clist, &comp_drivers_get()->list) {
		struct comp_driver_info *info = container_of(clist, struct comp_driver_info, list);
		if (!info->drv->uid) continue;
		if (!memcmp(info->drv->uid, &mfcc_test_uuid, sizeof(struct sof_uuid))) {
			drv = info->drv;
			break;
		}
	}
	zassert_not_null(drv, "Driver for MFCC not found");

	/* Initialize component via ops */
	comp = drv->ops.create(drv, &ipc_config, &spec);
	zassert_not_null(comp, "MFCC allocation failed");

	/* Verify component state */
	zassert_equal(comp->state, COMP_STATE_READY, "Component is not in READY state");
	zassert_equal(comp->ipc_config.id, 1, "IPC ID mismatch");

	/* Free the component */
	drv->ops.free(comp);
}

ZTEST_SUITE(audio_mfcc, NULL, setup, NULL, NULL, NULL);
