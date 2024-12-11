// SPDX-License-Identifier: BSD-3-Clause
// Copyright(c) 2022 Google LLC.  All rights reserved.
// Author: Andy Ross <andyross@google.com>
#include <zephyr/drivers/dma.h>
#include <sof/lib/dma.h>

/* Zephyr "DMA" stub device */

#define NUM_CHANS 2

/* Note that the spinlock in this struct isn't really needed for
 * native_posix, which can't preempt app code.  But it's here for
 * correctness in case anyone wants to port this to a different test
 * environment.
 */
struct pzdma_data {
	struct dma_context ctx;  /* MUST BE FIRST!  See API docs */
	struct k_spinlock lock;
	atomic_t chan_atom; /* weird API... */
	struct {
		struct dma_config cfg;
		uint32_t src, dst, sz;
		bool started;
		bool suspended;
	} chans[NUM_CHANS];
};

struct pzdma_cfg {
	int id;
};

static int pzdma_config(const struct device *dev, uint32_t channel,
			struct dma_config *config)
{
	struct pzdma_data *dev_data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&dev_data->lock);

	__ASSERT_NO_MSG(!dev_data->chans[channel].started);

	dev_data->chans[channel].cfg = *config;
	k_spin_unlock(&dev_data->lock, key);
	return 0;
}

static int pzdma_reload(const struct device *dev, uint32_t channel,
			uint32_t src, uint32_t dst, size_t size)
{
	struct pzdma_data *dev_data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&dev_data->lock);

	__ASSERT_NO_MSG(!dev_data->chans[channel].started);

	dev_data->chans[channel].src = src;
	dev_data->chans[channel].dst = dst;
	dev_data->chans[channel].sz = size;

	k_spin_unlock(&dev_data->lock, key);
	return 0;
}

static int pzdma_suspend(const struct device *dev, uint32_t channel)
{
	struct pzdma_data *dev_data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&dev_data->lock);

	__ASSERT_NO_MSG(dev_data->chans[channel].started);
	__ASSERT_NO_MSG(!dev_data->chans[channel].suspended);
	dev_data->chans[channel].suspended = true;

	// FIXME: cancel callback

	k_spin_unlock(&dev_data->lock, key);
	return 0;
}

static int pzdma_resume(const struct device *dev, uint32_t channel)
{
	struct pzdma_data *dev_data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&dev_data->lock);

	__ASSERT_NO_MSG(dev_data->chans[channel].started);
	__ASSERT_NO_MSG(dev_data->chans[channel].suspended);
	dev_data->chans[channel].suspended = false;

	// FIXME: resume callback

	k_spin_unlock(&dev_data->lock, key);
	return 0;
}

static int pzdma_start(const struct device *dev, uint32_t channel)
{
	struct pzdma_data *dev_data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&dev_data->lock);

	__ASSERT_NO_MSG(!dev_data->chans[channel].started);
	dev_data->chans[channel].started = true;
	dev_data->chans[channel].suspended = true;
	pzdma_resume(dev, channel);
	k_spin_unlock(&dev_data->lock, key);
	return 0;
}

static int pzdma_stop(const struct device *dev, uint32_t channel)
{
	struct pzdma_data *dev_data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&dev_data->lock);

	__ASSERT_NO_MSG(dev_data->chans[channel].started);

	if (!dev_data->chans[channel].suspended)
		pzdma_suspend(dev, channel);

	dev_data->chans[channel].started = false;
	k_spin_unlock(&dev_data->lock, key);
	return 0;
}

static int pzdma_get_status(const struct device *dev, uint32_t channel,
			    struct dma_status *status)
{
	struct pzdma_data *dev_data = dev->data;
	k_spinlock_key_t key = k_spin_lock(&dev_data->lock);

	// FIXME: synthesize offsets

	k_spin_unlock(&dev_data->lock, key);
	return 0;
}

static bool pzdma_chan_filter(const struct device *dev,
			      int channel, void *filter_param)
{
	return true;
}

static int pzdma_init(const struct device *dev)
{
	struct pzdma_data *dev_data = dev->data;

	dev_data->ctx.magic = DMA_MAGIC;
	dev_data->ctx.dma_channels = NUM_CHANS;
	dev_data->ctx.atomic = &dev_data->chan_atom; /* ... why?! */
	return 0;
}

/* Zephyr device definition */

static DEVICE_API(dma, pzdma_api) = {
	.config = pzdma_config,
	.reload = pzdma_reload,
	.start = pzdma_start,
	.stop = pzdma_stop,
	.suspend = pzdma_suspend,
	.resume = pzdma_resume,
	.get_status = pzdma_get_status,
	.chan_filter = pzdma_chan_filter,
};

struct pzdma_data pzdma0_data;
struct pzdma_data pzdma1_data;
struct pzdma_data pzdma2_data;
struct pzdma_data pzdma3_data;

const struct pzdma_cfg pzdma0_cfg = { .id = 0 };
const struct pzdma_cfg pzdma1_cfg = { .id = 1 };
const struct pzdma_cfg pzdma2_cfg = { .id = 2 };
const struct pzdma_cfg pzdma3_cfg = { .id = 3 };

DEVICE_DEFINE(pzdma0, "pzdma0", pzdma_init, NULL, &pzdma0_data, &pzdma0_cfg,
	      POST_KERNEL, 0, &pzdma_api);
DEVICE_DEFINE(pzdma1, "pzdma1", pzdma_init, NULL, &pzdma1_data, &pzdma1_cfg,
	      POST_KERNEL, 0, &pzdma_api);
DEVICE_DEFINE(pzdma2, "pzdma2", pzdma_init, NULL, &pzdma2_data, &pzdma2_cfg,
	      POST_KERNEL, 0, &pzdma_api);
DEVICE_DEFINE(pzdma3, "pzdma3", pzdma_init, NULL, &pzdma3_data, &pzdma3_cfg,
	      POST_KERNEL, 0, &pzdma_api);

struct dma posix_sof_dma[4];

const struct dma_info posix_sof_dma_info = {
	.dma_array = posix_sof_dma,
	.num_dmas = ARRAY_SIZE(posix_sof_dma),
};

void posix_dma_init(struct sof *sof)
{
	/* DEVICE_DEFINE() used to produce proper C identifiers, and
	 * is still documented that way, but now they have a
	 * "__device_" prefix...
	 */
	const struct device *devs[] = {
		&__device_pzdma0, &__device_pzdma1, &__device_pzdma2, &__device_pzdma3
	};

	for (int i = 0; i < ARRAY_SIZE(posix_sof_dma); i++) {
		posix_sof_dma[i] = (struct dma) {};
		posix_sof_dma[i].plat_data.dir = 0xffffffff;
		posix_sof_dma[i].plat_data.caps = 0xffffffff;
		posix_sof_dma[i].plat_data.devs = 0xffffffff;
		posix_sof_dma[i].plat_data.channels = NUM_CHANS;
		posix_sof_dma[i].z_dev = devs[i];
	};

	sof->dma_info = &posix_sof_dma_info;
}
