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
LOG_MODULE_REGISTER(dai, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(dai_lib);

DECLARE_TR_CTX(dai_tr, SOF_UUID(dai_lib_uuid), LOG_LEVEL_INFO);

struct dai_group_list {
	struct list_item list;
} __aligned(PLATFORM_DCACHE_ALIGN);

static struct dai_group_list *groups[CONFIG_CORE_COUNT];

static struct dai_group_list *dai_group_list_get(int core_id)
{
	struct dai_group_list *group_list = groups[core_id];

	if (!group_list) {
		group_list = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
				     sizeof(*group_list));

		groups[core_id] = group_list;
		list_init(&group_list->list);
	}

	return group_list;
}

static struct dai_group *dai_group_find(uint32_t group_id)
{
	struct list_item *dai_groups;
	struct list_item *group_item;
	struct dai_group *group = NULL;

	dai_groups = &dai_group_list_get(cpu_get_id())->list;

	list_for_item(group_item, dai_groups) {
		group = container_of(group_item, struct dai_group, list);

		if (group->group_id == group_id)
			break;

		group = NULL;
	}

	return group;
}

static struct dai_group *dai_group_alloc()
{
	struct list_item *dai_groups = &dai_group_list_get(cpu_get_id())->list;
	struct dai_group *group;

	group = rzalloc(SOF_MEM_ZONE_SYS, 0, SOF_MEM_CAPS_RAM,
			sizeof(*group));

	list_item_prepend(&group->list, dai_groups);

	return group;
}

struct dai_group *dai_group_get(uint32_t group_id, uint32_t flags)
{
	struct dai_group *group;

	if (!group_id) {
		tr_err(&dai_tr, "dai_group_get(): invalid group_id %u",
		       group_id);
		return NULL;
	}

	/* Check if this group already exists */
	group = dai_group_find(group_id);

	/* Check if there's a released and now unused group */
	if (!group)
		group = dai_group_find(0);

	/* Group doesn't exist, let's allocate and initialize it */
	if (!group && flags & DAI_CREAT)
		group = dai_group_alloc();

	if (group) {
		/* Group might've been previously unused */
		if (!group->group_id)
			group->group_id = group_id;

		group->num_dais++;
	} else {
		tr_err(&dai_tr, "dai_group_get(): failed to get group_id %u",
		       group_id);
	}

	return group;
}

void dai_group_put(struct dai_group *group)
{
	group->num_dais--;

	/* Mark as unused if there are no more DAIs in this group */
	if (!group->num_dais)
		group->group_id = 0;
}

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
};

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
	case SOF_DAI_IMX_SAI:
		return DAI_IMX_SAI;
	case SOF_DAI_IMX_ESAI:
		return DAI_IMX_ESAI;
	case SOF_DAI_AMD_BT:
		return DAI_AMD_BT;
	case SOF_DAI_AMD_SP:
		return DAI_AMD_SP;
	case SOF_DAI_AMD_DMIC:
		return DAI_AMD_DMIC;
	case SOF_DAI_MEDIATEK_AFE:
		return DAI_MEDIATEK_AFE;
	case SOF_DAI_IMX_MICFIL:
		return DAI_IMX_MICFIL;
	case SOF_DAI_AMD_HS:
	case SOF_DAI_AMD_SP_VIRTUAL:
	case SOF_DAI_AMD_HS_VIRTUAL:
	case SOF_DAI_AMD_SW_AUDIO:
		return -ENOTSUP;
	default:
		return -EINVAL;
	}
}

const struct device *dai_get_device(uint32_t type, uint32_t index)
{
	struct dai_config cfg;
	int z_type;
	int dir;
	int i;

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
		d->dma_dev = SOF_DMA_DEV_HDA;
		d->dma_caps = SOF_DMA_CAP_HDA;
		break;
	default:
		break;
	}
}

/* called from ipc/ipc3/handler.c and some platform.c files */
struct dai *dai_get(uint32_t type, uint32_t index, uint32_t flags)
{
	const struct device *dev;
	struct dai *d;

	dev = dai_get_device(type, index);
	if (!dev) {
		tr_err(&dai_tr, "dai_get: failed to get dai with index %d type %d",
		       index, type);
		return NULL;
	}

	d = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM, sizeof(struct dai));
	if (!d)
		return NULL;

	d->index = index;
	d->type = type;
	d->dev = dev;

	dai_set_device_params(d);

	if (dai_probe(d->dev)) {
		tr_err(&dai_tr, "dai_get: failed to probe dai with index %d type %d",
		       index, type);
		rfree(d);
		return NULL;
	}

	return d;
}

/* called from src/ipc/ipc3/handler.c */
void dai_put(struct dai *dai)
{
	int ret;

	ret = dai_remove(dai->dev);
	if (ret < 0) {
		tr_err(&dai_tr, "dai_put_zephyr: index %d failed ret = %d",
		       dai->index, ret);
	}

	rfree(dai);
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

struct dai *dai_get(uint32_t type, uint32_t index, uint32_t flags)
{
	int ret = 0;
	const struct dai_type_info *dti;
	struct dai *d;
	k_spinlock_key_t key;

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

void dai_put(struct dai *dai)
{
	int ret;
	k_spinlock_key_t key;

	key = k_spin_lock(&dai->lock);
	if (--dai->sref == 0) {
		ret = dai_remove(dai);
		if (ret < 0) {
			tr_err(&dai_tr, "dai_put: type %d index %d dai_remove() failed ret = %d",
			       dai->drv->type, dai->index, ret);
		}
	}
	tr_info(&dai_tr, "dai_put type %d index %d new sref %d",
		dai->drv->type, dai->index, dai->sref);
	k_spin_unlock(&dai->lock, key);
}
#endif
