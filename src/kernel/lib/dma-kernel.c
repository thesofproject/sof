// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <rtos/atomic.h>
#include <rtos/kernel.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#if CONFIG_INTEL_ADSP_MIC_PRIVACY
#include <sof/audio/mic_privacy_manager.h>
#endif
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <sof/schedule/ll_schedule_domain.h> /* zephyr_ll_user_heap_verify() */
#include <rtos/spinlock.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <module/module/base.h>

LOG_MODULE_REGISTER(dma, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(dma);

/* unused with Zephyr, generates no output */
DECLARE_TR_CTX(dma_tr, SOF_UUID(dma_uuid), LOG_LEVEL_INFO);

/* Kernel part of the DMA library: the DMA controller lookup, reference counting and
 * channel allocation, i.e. the kernel half of the sof_dma_get() and sof_dma_put()
 * system calls, and the getters of the legacy DMA drivers. The DMA buffer helpers
 * are in src/lib/dma.c
 */

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
static int dma_init(struct sof_dma *dma);

#ifdef CONFIG_SOF_USERSPACE_INTERFACE_DMA
/*
 * dma[] is writable by user threads, so the reference count, the lock and
 * the channel array to free are kept in the kernel-only struct
 * sof_dma_kinfo. dma->sref is still updated for diagnostics, but never
 * trusted.
 */
static int *dma_sref(struct sof_dma *dma)
{
	return &sof_dma_kinfo_get(dma)->sref;
}

/*
 * dma->lock is user-writable too: a user thread could release it while the
 * kernel holds it, letting two cores update the reference count at once.
 */
static struct k_spinlock *dma_lock(struct sof_dma *dma)
{
	return &sof_dma_kinfo_get(dma)->lock;
}
#else
static int *dma_sref(struct sof_dma *dma)
{
	return &dma->sref;
}

static struct k_spinlock *dma_lock(struct sof_dma *dma)
{
	return &dma->lock;
}
#endif

__cold struct sof_dma *z_impl_sof_dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags)
{
	const struct dma_info *info = dma_info_get();
	int users, ret = 0;
	int min_users = INT32_MAX;
	struct sof_dma *d = NULL, *dmin = NULL;
	k_spinlock_key_t key;

	assert_can_be_cold();

	if (!info->num_dmas) {
		tr_err(&dma_tr, "dma_get(): No DMACs installed");
		return NULL;
	}

	/* find DMAC with free channels that matches request */
	for (d = info->dma_array; d < info->dma_array + info->num_dmas;
	     d++) {
		/* skip if this DMAC does not support the requested dir */
		if (dir && (d->plat_data.dir & dir) == 0)
			continue;

		/* skip if this DMAC does not support the requested caps */
		if (cap && (d->plat_data.caps & cap) == 0)
			continue;

		/* skip if this DMAC does not support the requested dev */
		if (dev && (d->plat_data.devs & dev) == 0)
			continue;

		/* if exclusive access is requested */
		if (flags & SOF_DMA_ACCESS_EXCLUSIVE) {
			/* ret DMA with no users */
			if (!d->sref) {
				dmin = d;
				break;
			}
		} else {
			/* get number of users for this DMAC*/
			users = d->sref;

			/* pick DMAC with the least num of users */
			if (users < min_users) {
				dmin = d;
				min_users = users;
			}
		}
	}

	if (!dmin) {
		tr_err(&dma_tr, "No DMAC dir %d caps 0x%x dev 0x%x flags 0x%x",
		       dir, cap, dev, flags);

		for (d = info->dma_array;
		     d < info->dma_array + info->num_dmas;
		     d++) {
			tr_err(&dma_tr, " DMAC ID %d users %d busy channels %ld",
			       d->plat_data.id, d->sref,
			       atomic_read(&d->num_channels_busy));
			tr_err(&dma_tr, "  caps 0x%x dev 0x%x",
			       d->plat_data.caps, d->plat_data.devs);
		}

		return NULL;
	}

	/* return DMAC */
	tr_dbg(&dma_tr, "dma_get(), dma-probe id = %d",
	       dmin->plat_data.id);

	/* Shared DMA controllers with multiple channels
	 * may be requested many times, let the probe()
	 * do on-first-use initialization.
	 */
	key = k_spin_lock(dma_lock(dmin));

	if (!*dma_sref(dmin)) {
		ret = dma_init(dmin);
		if (ret < 0) {
			tr_err(&dma_tr, "dma_get(): dma-probe failed id = %d, ret = %d",
			       dmin->plat_data.id, ret);
			goto out;
		}
	}

	++*dma_sref(dmin);
	/* user-visible copy for diagnostics and DMAC selection, never trusted */
	dmin->sref = *dma_sref(dmin);

	tr_info(&dma_tr, "dma_get() ID %d sref = %d busy channels %ld",
		dmin->plat_data.id, dmin->sref,
		atomic_read(&dmin->num_channels_busy));
out:
	k_spin_unlock(dma_lock(dmin), key);
	return !ret ? dmin : NULL;
}

__cold void z_impl_sof_dma_put(struct sof_dma *dma)
{
	k_spinlock_key_t key;

	assert_can_be_cold();

	if (!dma)
		return;

	key = k_spin_lock(dma_lock(dma));
	if (*dma_sref(dma) <= 0) {
		tr_err(&dma_tr, "dma_put(), dma = %p not in use", dma);
		k_spin_unlock(dma_lock(dma), key);
		return;
	}

	--*dma_sref(dma);
	dma->sref = *dma_sref(dma);
	if (!*dma_sref(dma)) {
#ifdef CONFIG_SOF_USERSPACE_INTERFACE_DMA
		struct sof_dma_kinfo *kinfo = sof_dma_kinfo_get(dma);

		sof_heap_free(kinfo->heap, kinfo->chan);
		kinfo->chan = NULL;
#else
		sof_heap_free(dma->heap, dma->chan);
#endif
		dma->chan = NULL;
	}

	tr_info(&dma_tr, "dma_put(), dma = %p, sref = %d",
		dma, dma->sref);
	k_spin_unlock(dma_lock(dma), key);
}

__cold static int dma_init(struct sof_dma *dma)
{
	struct dma_chan_data *chans, *chan;
	struct k_heap *heap = sof_sys_user_heap_get();
#ifdef CONFIG_SOF_USERSPACE_INTERFACE_DMA
	/* dma->plat_data is user-writable, use the kernel snapshot */
	const uint32_t channels = sof_dma_kinfo_get(dma)->channels;
#else
	const uint32_t channels = dma->plat_data.channels;
#endif
	uint32_t i;

	assert_can_be_cold();

#if CONFIG_SOF_USERSPACE_INTERFACE_DMA && CONFIG_SOF_USERSPACE_LL
	/*
	 * Called in supervisor mode from the sof_dma_get() system call. The
	 * heap pointer comes from user-writable memory, so check that it is
	 * the LL user heap before running the allocator on it.
	 */
	if (!zephyr_ll_user_heap_verify(heap)) {
		tr_err(&dma_tr, "dma %d: invalid channel heap %p", dma->plat_data.id, heap);
		return -EINVAL;
	}
#endif

	/*
	 * allocate dma channels, use a local pointer until the array is
	 * initialized: dma->chan is user-writable and must not be read back
	 */
	chans = sof_heap_alloc(heap, SOF_MEM_FLAG_USER | SOF_MEM_FLAG_COHERENT,
			       sizeof(struct dma_chan_data) * channels, 0);
	if (!chans) {
		tr_err(&dma_tr, "dma %d allocation of channels failed",
		       dma->plat_data.id);
		return -ENOMEM;
	}

	memset(chans, 0, sizeof(struct dma_chan_data) * channels);
	/* init work */
	for (i = 0, chan = chans; i < channels; i++, chan++) {
		chan->dma = dma;
		chan->index = i;
	}

#ifdef CONFIG_SOF_USERSPACE_INTERFACE_DMA
	sof_dma_kinfo_get(dma)->heap = heap;
	sof_dma_kinfo_get(dma)->chan = chans;
#endif
	dma->heap = heap;
	dma->chan = chans;

	return 0;
}
#else
struct dma *dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags)
{
	const struct dma_info *info = dma_info_get();
	int users, ret;
	int min_users = INT32_MAX;
	struct dma *d = NULL, *dmin = NULL;
	k_spinlock_key_t key;

	if (!info->num_dmas) {
		tr_err(&dma_tr, "dma_get(): No DMACs installed");
		return NULL;
	}

	/* find DMAC with free channels that matches request */
	for (d = info->dma_array; d < info->dma_array + info->num_dmas;
	     d++) {
		/* skip if this DMAC does not support the requested dir */
		if (dir && (d->plat_data.dir & dir) == 0)
			continue;

		/* skip if this DMAC does not support the requested caps */
		if (cap && (d->plat_data.caps & cap) == 0)
			continue;

		/* skip if this DMAC does not support the requested dev */
		if (dev && (d->plat_data.devs & dev) == 0)
			continue;

		/* skip if this DMAC has 1 user per avail channel */
		/* TODO: this should be fixed in dai.c to allow more users */
		if (d->sref >= d->plat_data.channels)
			continue;

		/* if exclusive access is requested */
		if (flags & DMA_ACCESS_EXCLUSIVE) {
			/* ret DMA with no users */
			if (!d->sref) {
				dmin = d;
				break;
			}
		} else {
			/* get number of users for this DMAC*/
			users = d->sref;

			/* pick DMAC with the least num of users */
			if (users < min_users) {
				dmin = d;
				min_users = users;
			}
		}
	}

	if (!dmin) {
		tr_err(&dma_tr, "No DMAC dir %d caps 0x%x dev 0x%x flags 0x%x",
		       dir, cap, dev, flags);

		for (d = info->dma_array;
		     d < info->dma_array + info->num_dmas;
		     d++) {
			tr_err(&dma_tr, " DMAC ID %d users %d busy channels %ld",
			       d->plat_data.id, d->sref,
			       atomic_read(&d->num_channels_busy));
			tr_err(&dma_tr, "  caps 0x%x dev 0x%x",
			       d->plat_data.caps, d->plat_data.devs);
		}

		return NULL;
	}

	/* return DMAC */
	tr_dbg(&dma_tr, "dma_get(), dma-probe id = %d",
	       dmin->plat_data.id);

	/* Shared DMA controllers with multiple channels
	 * may be requested many times, let the probe()
	 * do on-first-use initialization.
	 */
	key = k_spin_lock(&dmin->lock);

	ret = 0;
	if (!dmin->sref) {
		ret = dma_probe_legacy(dmin);
		if (ret < 0) {
			tr_err(&dma_tr, "dma_get(): dma-probe failed id = %d, ret = %d",
			       dmin->plat_data.id, ret);
		}
	}
	if (!ret)
		dmin->sref++;

	tr_info(&dma_tr, "dma_get() ID %d sref = %d busy channels %ld",
		dmin->plat_data.id, dmin->sref,
		atomic_read(&dmin->num_channels_busy));

	k_spin_unlock(&dmin->lock, key);
	return !ret ? dmin : NULL;
}

void dma_put(struct dma *dma)
{
	k_spinlock_key_t key;
	int ret;

	key = k_spin_lock(&dma->lock);
	if (--dma->sref == 0) {
		ret = dma_remove_legacy(dma);
		if (ret < 0) {
			tr_err(&dma_tr, "dma_remove() failed id  = %d, ret = %d",
			       dma->plat_data.id, ret);
		}
	}
	tr_info(&dma_tr, "dma = %p, sref = %d",
		dma, dma->sref);
	k_spin_unlock(&dma->lock, key);
}
#endif
