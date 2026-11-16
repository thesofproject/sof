// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2018 Intel Corporation. All rights reserved.
//
// Author: Marcin Maka <marcin.maka@linux.intel.com>

#include <sof/lib/dai.h>
#include <rtos/alloc.h>
#include <sof/lib/cpu.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <sof/schedule/ll_schedule_domain.h> /* for zephyr_ll_user_heap() */
#include <ipc/topology.h>
#include <ipc/dai.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
#include <zephyr/device.h>
#include <zephyr/drivers/dai.h>
#endif
LOG_MODULE_DECLARE(dai, CONFIG_SOF_LOG_LEVEL);

extern struct tr_ctx dai_tr;

/* Kernel part of the DAI library: the Zephyr DAI device table, the kernel half of the
 * dai_get_device() system call and the DAI getters, which probe and remove the DAI
 * drivers. The DAI groups are in src/lib/dai.c
 */

#if CONFIG_ZEPHYR_NATIVE_DRIVERS

#define GET_DEVICE_LIST(node) DEVICE_DT_GET(node),

const struct device *zephyr_dev[] = {
#if CONFIG_DAI_INTEL_SSP
	DT_FOREACH_STATUS_OKAY(intel_ssp_dai, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_INTEL_DMIC
	DT_FOREACH_STATUS_OKAY(intel_dai_dmic, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_INTEL_ALH
	DT_FOREACH_STATUS_OKAY(intel_alh_dai, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_INTEL_HDA
	DT_FOREACH_STATUS_OKAY(intel_hda_dai, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_NXP_SAI
	DT_FOREACH_STATUS_OKAY(nxp_dai_sai, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_NXP_ESAI
	DT_FOREACH_STATUS_OKAY(nxp_dai_esai, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_NXP_MICFIL
	DT_FOREACH_STATUS_OKAY(nxp_dai_micfil, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_AMD_DMIC
	DT_FOREACH_STATUS_OKAY(amd_dmic_dai, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_AMD_SDW
	DT_FOREACH_STATUS_OKAY(amd_acp_sdw_dai, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_AMD_TDM
	DT_FOREACH_STATUS_OKAY(amd_tdm_dai, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_INTEL_UAOL
	DT_FOREACH_STATUS_OKAY(intel_uaol_dai, GET_DEVICE_LIST)
#endif
#if DT_HAS_COMPAT_STATUS_OKAY(mediatek_afe)
	DT_FOREACH_STATUS_OKAY(mediatek_afe, GET_DEVICE_LIST)
#endif
#if CONFIG_DAI_POSIX
	DT_FOREACH_STATUS_OKAY(zephyr_posix_dai, GET_DEVICE_LIST)
#endif
};

const struct device **dai_get_device_list(size_t *count)
{
	__ASSERT_NO_MSG(count);
	*count = ARRAY_SIZE(zephyr_dev);

	return zephyr_dev;
}

/* convert sof_ipc_dai_type to Zephyr dai_type */
static int sof_dai_type_to_zephyr(uint32_t type)
{
	switch (type) {
	case SOF_DAI_INTEL_SSP:
		return DAI_INTEL_SSP;
	case SOF_DAI_INTEL_DMIC:
		return DAI_INTEL_DMIC;
	case SOF_DAI_INTEL_HDA:
		return DAI_INTEL_HDA;
	case SOF_DAI_INTEL_ALH:
		return DAI_INTEL_ALH;
	case SOF_DAI_INTEL_UAOL:
		return DAI_INTEL_UAOL;
	case SOF_DAI_IMX_SAI:
		return DAI_IMX_SAI;
	case SOF_DAI_IMX_ESAI:
		return DAI_IMX_ESAI;
	case SOF_DAI_AMD_DMIC:
		return DAI_AMD_DMIC;
	case SOF_DAI_MEDIATEK_AFE:
		return DAI_MEDIATEK_AFE;
	case SOF_DAI_IMX_MICFIL:
		return DAI_IMX_MICFIL;
	case SOF_DAI_AMD_SDW:
		return DAI_AMD_SDW;
	case SOF_DAI_AMD_HS:
	case SOF_DAI_AMD_HS_VIRTUAL:
	case SOF_DAI_AMD_SP:
	case SOF_DAI_AMD_SP_VIRTUAL:
	case SOF_DAI_AMD_BT:
	case SOF_DAI_AMD_TDM:
		return DAI_AMD_TDM;
	default:
		return -EINVAL;
	}
}

__cold const struct device *z_impl_dai_get_device(enum sof_ipc_dai_type type, uint32_t index)
{
	struct dai_config cfg;
	int z_type;
	int dir;
	int i;

	assert_can_be_cold();

	dir = (type == SOF_DAI_INTEL_DMIC) ? DAI_DIR_RX : DAI_DIR_BOTH;

	z_type = sof_dai_type_to_zephyr(type);
	if (z_type < 0) {
		tr_err(&dai_tr, "dai_get_device: no matching zephyr DAI type for %d ret = %d",
		       type, z_type);
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(zephyr_dev); i++) {
		if (dai_config_get(zephyr_dev[i], &cfg, dir))
			continue;
		if (cfg.type == z_type && cfg.dai_index == index)
			return zephyr_dev[i];
	}

	return NULL;
}

static void dai_set_device_params(struct dai *d)
{
	switch (d->type) {
	case SOF_DAI_INTEL_SSP:
		d->dma_dev = SOF_DMA_DEV_SSP;
#ifdef CONFIG_DMA_INTEL_ADSP_GPDMA
		d->dma_caps = SOF_DMA_CAP_GP_LP | SOF_DMA_CAP_GP_HP;
#else
		d->dma_caps = SOF_DMA_CAP_HDA;
#endif
		break;
	case SOF_DAI_INTEL_DMIC:
		d->dma_dev = SOF_DMA_DEV_DMIC;
#ifdef CONFIG_DMA_INTEL_ADSP_GPDMA
		d->dma_caps = SOF_DMA_CAP_GP_LP | SOF_DMA_CAP_GP_HP;
#else
		d->dma_caps = SOF_DMA_CAP_HDA;
#endif
		break;
	case SOF_DAI_INTEL_ALH:
		d->dma_dev = SOF_DMA_DEV_ALH;
#ifdef CONFIG_DMA_INTEL_ADSP_GPDMA
		d->dma_caps = SOF_DMA_CAP_GP_LP | SOF_DMA_CAP_GP_HP;
#else
		d->dma_caps = SOF_DMA_CAP_HDA;
#endif
		break;
	case SOF_DAI_INTEL_HDA:
	case SOF_DAI_INTEL_UAOL:
		d->dma_dev = SOF_DMA_DEV_HDA;
		d->dma_caps = SOF_DMA_CAP_HDA;
		break;
	case SOF_DAI_AMD_SDW:
		d->dma_dev = SOF_DMA_DEV_SW;
		d->dma_caps = SOF_DMA_CAP_SW;
		break;
	/* All TDM-capable AMD DAIs share acp_tdm_dma. */
	case SOF_DAI_AMD_HS:
	case SOF_DAI_AMD_HS_VIRTUAL:
	case SOF_DAI_AMD_SP:
	case SOF_DAI_AMD_SP_VIRTUAL:
	case SOF_DAI_AMD_BT:
		d->dma_dev = SOF_DMA_DEV_HS | SOF_DMA_DEV_SP | SOF_DMA_DEV_BT;
		d->dma_caps = SOF_DMA_CAP_HS | SOF_DMA_CAP_SP | SOF_DMA_CAP_BT;
		break;
	case SOF_DAI_AMD_DMIC:
		d->dma_dev = SOF_DMA_DEV_DMIC;
		d->dma_caps = SOF_DMA_CAP_DMIC;
		break;
	case SOF_DAI_AMD_TDM:
		d->dma_dev = SOF_DMA_DEV_TDM;
		d->dma_caps = SOF_DMA_CAP_TDM;
		break;
	case SOF_DAI_MEDIATEK_AFE:
		d->dma_dev = SOF_DMA_DEV_AFE_MEMIF;
		break;
	default:
		break;
	}
}

/* called from kernel/ipc/ipc3/handler.c and some platform.c files */
__cold struct dai *dai_get(uint32_t type, uint32_t index, uint32_t flags)
{
	const struct device *dev;
	struct dai *d;
	struct k_heap *heap = NULL;

	assert_can_be_cold();

#ifdef CONFIG_SOF_USERSPACE_LL
	heap = zephyr_ll_user_heap();
#endif

	dev = dai_get_device(type, index);
	if (!dev) {
		tr_err(&dai_tr, "dai_get: failed to get dai with index %d type %d",
		       index, type);
		return NULL;
	}

	d = sof_heap_alloc(heap, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT, sizeof(struct dai), 0);
	if (!d)
		return NULL;

	memset(d, 0, sizeof(struct dai));

	d->index = index;
	d->type = type;
	d->dev = dev;

	dai_set_device_params(d);

	if (dai_probe(d->dev)) {
		tr_err(&dai_tr, "dai_get: failed to probe dai with index %d type %d",
		       index, type);
		sof_heap_free(heap, d);
		return NULL;
	}

	return d;
}

/* called from src/kernel/ipc/ipc3/handler.c */
__cold void dai_put(struct dai *dai)
{
	int ret;
	struct k_heap *heap = NULL;

	assert_can_be_cold();

	if (!dai)
		return;

#ifdef CONFIG_SOF_USERSPACE_LL
	heap = zephyr_ll_user_heap();
#endif

	ret = dai_remove(dai->dev);
	if (ret < 0) {
		tr_err(&dai_tr, "index %d failed ret = %d",
		       dai->index, ret);
	}

	sof_heap_free(heap, dai);
}
#else
static inline const struct dai_type_info *dai_find_type(uint32_t type)
{
	const struct dai_info *info = dai_info_get();
	const struct dai_type_info *dti;

	for (dti = info->dai_type_array;
	     dti < info->dai_type_array + info->num_dai_types; dti++) {
		if (dti->type == type)
			return dti;
	}
	return NULL;
}

__cold struct dai *dai_get(uint32_t type, uint32_t index, uint32_t flags)
{
	int ret = 0;
	const struct dai_type_info *dti;
	struct dai *d;
	k_spinlock_key_t key;

	assert_can_be_cold();

	dti = dai_find_type(type);
	if (!dti)
		return NULL; /* type not found */

	for (d = dti->dai_array; d < dti->dai_array + dti->num_dais; d++) {
		if (d->index != index) {
			continue;
		}
		/* device created? */
		key = k_spin_lock(&d->lock);
		if (d->sref == 0) {
			if (flags & DAI_CREAT)
				ret = dai_probe(d);
			else
				ret = -ENODEV;
		}
		if (!ret)
			d->sref++;

		tr_info(&dai_tr, "dai_get type %d index %d new sref %d",
			type, index, d->sref);

		k_spin_unlock(&d->lock, key);

		return !ret ? d : NULL;
	}
	tr_err(&dai_tr, "dai_get: type %d index %d not found", type, index);
	return NULL;
}

__cold void dai_put(struct dai *dai)
{
	int ret;
	k_spinlock_key_t key;

	assert_can_be_cold();

	if (!dai)
		return;

	key = k_spin_lock(&dai->lock);
	if (--dai->sref == 0) {
		ret = dai_remove(dai);
		if (ret < 0) {
			tr_err(&dai_tr, "type %d index %d dai_remove() failed ret = %d",
			       dai->drv->type, dai->index, ret);
		}
	}
	tr_info(&dai_tr, "type %d index %d new sref %d",
		dai->drv->type, dai->index, dai->sref);
	k_spin_unlock(&dai->lock, key);
}
#endif
