// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>
// Author: Paul Olaru <paul.olaru@nxp.com>

/* Dummy DMA driver (software-based DMA controller)
 *
 * This driver is usable on all platforms where the DSP can directly access
 * all of the host physical memory (or at least the host buffers).
 *
 * The way this driver works is that it simply performs the copies
 * synchronously within the dma_start() and dma_copy() calls.
 *
 * One of the drawbacks of this driver is that it doesn't actually have a true
 * IRQ context, as the copy is done synchronously and the IRQ callbacks are
 * called in process context.
 *
 * An actual hardware DMA driver may be preferable because of the above
 * drawback which comes from a software implementation. But if there isn't any
 * hardware DMA controller dedicated for the host this driver can be used.
 *
 * This driver requires physical addresses in the elems. This assumption only
 * holds if you have CONFIG_HOST_PTABLE enabled, at least currently.
 */

#include <rtos/atomic.h>
#include <sof/audio/component.h>
#include <rtos/timer.h>
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/dma.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/platform.h>
#include <rtos/spinlock.h>
#include <rtos/string.h>
#include <sof/trace/trace.h>
#include <sys/types.h>
#include <ipc/topology.h>
#include <user/trace.h>

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_REGISTER(dummy_dma, CONFIG_SOF_LOG_LEVEL);

/* f6d15ad3-b122-458c-ae9b-0ab0b5867aa0 */
DECLARE_SOF_UUID("dummy-dma", dummy_dma_uuid, 0xf6d15ad3, 0xb122, 0x458c,
		 0xae, 0x9b, 0x0a, 0xb0, 0xb5, 0x86, 0x7a, 0xa0);

DECLARE_TR_CTX(ddma_tr, SOF_UUID(dummy_dma_uuid), LOG_LEVEL_INFO);

struct dma_chan_pdata {
	struct dma_sg_elem_array *elems;
	int sg_elem_curr_idx;
	uintptr_t r_pos;
	uintptr_t w_pos;
	uintptr_t elem_progress;
	bool cyclic;
};

#define DUMMY_DMA_BUFFER_PERIOD_COUNT	2

/**
 * \brief Copy the currently-in-progress DMA SG elem
 * \param[in,out] pdata: Private data structure for this DMA channel
 * \param[in] bytes: The amount of data requested for copying
 * \return How many bytes have been copied, or -ENODATA if nothing can be
 *	   copied. Will return 0 quickly if 0 bytes are requested.
 *
 * Perform the individual copy of the in-progress DMA SG elem. To copy more
 * data, one should call this function repeatedly.
 */
static ssize_t dummy_dma_copy_crt_elem(struct dma_chan_pdata *pdata,
				       int bytes)
{
	int ret;
	uintptr_t rptr, wptr;
	size_t orig_size, remaining_size, copy_size;

	if (bytes == 0)
		return 0;

	/* Quick check, do we have a valid elem? */
	if (pdata->sg_elem_curr_idx >= pdata->elems->count)
		return -ENODATA;

	/* We should copy whatever is left of the element, unless we have
	 * too little remaining for that to happen
	 */

	/* Compute copy size and pointers */
	rptr = pdata->elems->elems[pdata->sg_elem_curr_idx].src;
	wptr = pdata->elems->elems[pdata->sg_elem_curr_idx].dest;
	orig_size = pdata->elems->elems[pdata->sg_elem_curr_idx].size;
	remaining_size = orig_size - pdata->elem_progress;
	copy_size = MIN(remaining_size, bytes);

	/* On playback, invalidate host buffer (it may lie in a cached area).
	 * Otherwise we could be playing stale data.
	 * On capture this should be safe as host.c does a writeback before
	 * triggering the DMA.
	 */
	dcache_invalidate_region((void *)rptr, copy_size);

	/* Perform the copy, being careful if we overflow the elem */
	ret = memcpy_s((void *)wptr, remaining_size, (void *)rptr, copy_size);
	assert(!ret);

	/* On capture, writeback the host buffer (it may lie in a cached area).
	 * On playback, also writeback because host.c does an invalidate to
	 * be able to use the data transferred by the DMA.
	 */
	dcache_writeback_region((void *)wptr, copy_size);

	pdata->elem_progress += copy_size;

	if (remaining_size == copy_size) {
		/* Advance to next elem, if we can */
		pdata->sg_elem_curr_idx++;
		pdata->elem_progress = 0;
		/* Support cyclic copying */
		if (pdata->cyclic &&
		    pdata->sg_elem_curr_idx == pdata->elems->count)
			pdata->sg_elem_curr_idx = 0;
	}

	return copy_size;
}

static size_t dummy_dma_comp_avail_data_cyclic(struct dma_chan_pdata *pdata)
{
	/* Simple: just sum up all of the elements */
	size_t size = 0;
	int i;

	for (i = 0; i < pdata->elems->count; i++)
		size += pdata->elems->elems[i].size;

	return size;
}

static size_t dummy_dma_comp_avail_data_noncyclic(struct dma_chan_pdata *pdata)
{
	/* Slightly harder, take remainder of the current element plus
	 * all of the data in future elements
	 */
	size_t size = 0;
	int i;

	for (i = pdata->sg_elem_curr_idx; i < pdata->elems->count; i++)
		size += pdata->elems->elems[i].size;

	/* Account for partially copied current elem */
	size -= pdata->elem_progress;

	return size;
}

/**
 * \brief Compute how much data is available for copying at this point
 * \param[in] pdata: Private data structure for this DMA channel
 * \return Number of available/free bytes for copying, possibly 0
 *
 * Returns how many bytes can be copied with one dma_copy() call.
 */
static size_t dummy_dma_compute_avail_data(struct dma_chan_pdata *pdata)
{
	if (pdata->cyclic)
		return dummy_dma_comp_avail_data_cyclic(pdata);
	else
		return dummy_dma_comp_avail_data_noncyclic(pdata);
}

/**
 * \brief Copy as many elems as required to copy @bytes bytes
 * \param[in,out] pdata: Private data structure for this DMA channel
 * \param[in] bytes: The amount of data requested for copying
 * \return How many bytes have been copied, or -ENODATA if nothing can be
 *	   copied.
 *
 * Perform as many elem copies as required to fulfill the request for copying
 * @bytes bytes of data. Will copy exactly this much data if possible, however
 * it will stop short if you try to copy more data than available.
 */
static ssize_t dummy_dma_do_copies(struct dma_chan_pdata *pdata, int bytes)
{
	size_t avail = dummy_dma_compute_avail_data(pdata);
	ssize_t copied = 0;
	ssize_t crt_copied;

	if (!avail)
		return -ENODATA;

	while (bytes) {
		crt_copied = dummy_dma_copy_crt_elem(pdata, bytes);
		if (crt_copied <= 0) {
			if (copied > 0)
				return copied;
			else
				return crt_copied;
		}
		bytes -= crt_copied;
		copied += crt_copied;
	}

	return copied;
}

/**
 * \brief Allocate next free DMA channel
 * \param[in] dma: DMA controller
 * \param[in] req_chan: Ignored, would have been a preference for a particular
 *			channel
 * \return A structure to be used with the other callbacks in this driver,
 * or NULL in case no channel could be allocated.
 *
 * This function allocates a DMA channel for actual usage by any SOF client
 * code.
 */
static struct dma_chan_data *dummy_dma_channel_get(struct dma *dma,
						   unsigned int req_chan)
{
	k_spinlock_key_t key;
	int i;

	key = k_spin_lock(&dma->lock);
	for (i = 0; i < dma->plat_data.channels; i++) {
		/* use channel if it's free */
		if (dma->chan[i].status == COMP_STATE_INIT) {
			dma->chan[i].status = COMP_STATE_READY;

			atomic_add(&dma->num_channels_busy, 1);

			/* return channel */
			k_spin_unlock(&dma->lock, key);
			return &dma->chan[i];
		}
	}
	k_spin_unlock(&dma->lock, key);
	tr_err(&ddma_tr, "dummy-dmac: %d no free channel",
	       dma->plat_data.id);
	return NULL;
}

static void dummy_dma_channel_put_unlocked(struct dma_chan_data *channel)
{
	struct dma_chan_pdata *ch = dma_chan_get_data(channel);

	/* Reset channel state */
	notifier_unregister_all(NULL, channel);

	ch->elems = NULL;
	channel->desc_count = 0;
	ch->sg_elem_curr_idx = 0;

	ch->r_pos = 0;
	ch->w_pos = 0;

	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
}

/**
 * \brief Free a DMA channel
 * \param[in] channel: DMA channel
 *
 * Once a DMA channel is no longer needed it should be freed by calling this
 * function.
 */
static void dummy_dma_channel_put(struct dma_chan_data *channel)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&channel->dma->lock);
	dummy_dma_channel_put_unlocked(channel);
	k_spin_unlock(&channel->dma->lock, key);
}

/* Since copies are synchronous, the triggers do nothing */
static int dummy_dma_start(struct dma_chan_data *channel)
{
	return 0;
}

/* Since copies are synchronous, the triggers do nothing */
static int dummy_dma_release(struct dma_chan_data *channel)
{
	return 0;
}

/* Since copies are synchronous, the triggers do nothing */
static int dummy_dma_pause(struct dma_chan_data *channel)
{
	return 0;
}

/* Since copies are synchronous, the triggers do nothing */
static int dummy_dma_stop(struct dma_chan_data *channel)
{
	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int dummy_dma_status(struct dma_chan_data *channel,
			    struct dma_chan_status *status,
			    uint8_t direction)
{
	struct dma_chan_pdata *ch = dma_chan_get_data(channel);

	status->state = channel->status;
	status->flags = 0; /* TODO What flags should be put here? */
	status->r_pos = ch->r_pos;
	status->w_pos = ch->w_pos;

	status->timestamp = sof_cycle_get_64();
	return 0;
}

/**
 * \brief Set channel configuration
 * \param[in] channel: The channel to configure
 * \param[in] config: Configuration data
 * \return 0 on success, -EINVAL if the config is invalid or unsupported.
 *
 * Sets the channel configuration. For this particular driver the config means
 * the direction and the actual SG elems for copying.
 */
static int dummy_dma_set_config(struct dma_chan_data *channel,
				struct dma_sg_config *config)
{
	struct dma_chan_pdata *ch = dma_chan_get_data(channel);
	k_spinlock_key_t key;
	int ret = 0;

	key = k_spin_lock(&channel->dma->lock);

	if (!config->elem_array.count) {
		tr_err(&ddma_tr, "dummy-dmac: %d channel %d no DMA descriptors",
		       channel->dma->plat_data.id,
		       channel->index);

		ret = -EINVAL;
		goto out;
	}

	channel->direction = config->direction;

	if (config->direction != DMA_DIR_HMEM_TO_LMEM &&
	    config->direction != DMA_DIR_LMEM_TO_HMEM) {
		/* Shouldn't even happen though */
		tr_err(&ddma_tr, "dummy-dmac: %d channel %d invalid direction %d",
		       channel->dma->plat_data.id, channel->index,
		       config->direction);
		ret = -EINVAL;
		goto out;
	}
	channel->desc_count = config->elem_array.count;
	ch->elems = &config->elem_array;
	ch->sg_elem_curr_idx = 0;
	ch->cyclic = config->cyclic;

	channel->status = COMP_STATE_PREPARE;
out:
	k_spin_unlock(&channel->dma->lock, key);
	return ret;
}

/**
 * \brief Perform the DMA copy itself
 * \param[in] channel The channel to do the copying
 * \param[in] bytes How many bytes are requested to be copied
 * \param[in] flags Flags which may alter the copying (this driver ignores them)
 * \return 0 on success (this driver always succeeds)
 *
 * The copying must be done synchronously within this function, then SOF (the
 * host component) is notified via the callback that this number of bytes is
 * available.
 */
static int dummy_dma_copy(struct dma_chan_data *channel, int bytes,
			  uint32_t flags)
{
	ssize_t copied;
	struct dma_cb_data next = {
		.channel = channel,
	};
	struct dma_chan_pdata *pdata = dma_chan_get_data(channel);

	copied = dummy_dma_do_copies(pdata, bytes);
	if (copied < 0)
		return copied;

	next.elem.size = copied;

	/* Let the user of the driver know how much we copied */
	notifier_event(channel, NOTIFIER_ID_DMA_COPY,
		       NOTIFIER_TARGET_CORE_LOCAL, &next, sizeof(next));

	return 0;
}

/**
 * \brief Initialize the driver
 * \param[in] dma The preallocated DMA controller structure
 * \return 0 on success, a negative value on error
 *
 * This function must be called before any other will work. Calling functions
 * such as dma_channel_get() without a successful dma_probe() is undefined
 * behavior.
 */
static int dummy_dma_probe(struct dma *dma)
{
	struct dma_chan_pdata *chanp;
	int i;

	if (dma->chan) {
		tr_err(&ddma_tr, "dummy-dmac %d already created!",
		       dma->plat_data.id);
		return -EEXIST; /* already created */
	}

	dma->chan = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			    dma->plat_data.channels * sizeof(dma->chan[0]));
	if (!dma->chan) {
		tr_err(&ddma_tr, "dummy-dmac %d: Out of memory!",
		       dma->plat_data.id);
		return -ENOMEM;
	}

	chanp = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			dma->plat_data.channels * sizeof(chanp[0]));
	if (!chanp) {
		rfree(dma->chan);
		tr_err(&ddma_tr, "dummy-dmac %d: Out of memory!",
		       dma->plat_data.id);
		dma->chan = NULL;
		return -ENOMEM;
	}

	for (i = 0; i < dma->plat_data.channels; i++) {
		dma->chan[i].dma = dma;
		dma->chan[i].index = i;
		dma->chan[i].status = COMP_STATE_INIT;
		dma_chan_set_data(&dma->chan[i], &chanp[i]);
	}

	atomic_init(&dma->num_channels_busy, 0);

	return 0;
}

/**
 * \brief Free up all memory and resources used by this driver
 * \param[in] dma The DMA controller structure belonging to this driver
 *
 * This function undoes everything that probe() did. All channels that were
 * returned via dma_channel_get() become invalid and further usage of them is
 * undefined behavior. dma_channel_put() is automatically called on all
 * channels.
 *
 * This function is idempotent, and safe to call multiple times in a row.
 */
static int dummy_dma_remove(struct dma *dma)
{
	tr_dbg(&ddma_tr, "dummy_dma %d -> remove", dma->plat_data.id);
	if (!dma->chan)
		return 0;

	rfree(dma_chan_get_data(&dma->chan[0]));
	rfree(dma->chan);
	dma->chan = NULL;
	return 0;
}

/**
 * \brief Get DMA copy data sizes
 * \param[in] channel DMA channel on which we're interested of the sizes
 * \param[out] avail How much data the channel can deliver if copy() is called
 *		     now
 * \param[out] free How much data can be copied to the host via this channel
 *		    without going over the buffer size
 * \return 0 on success, -EINVAL if a configuration error is detected
 */
static int dummy_dma_get_data_size(struct dma_chan_data *channel,
				   uint32_t *avail, uint32_t *free)
{
	struct dma_chan_pdata *pdata = dma_chan_get_data(channel);
	uint32_t size = dummy_dma_compute_avail_data(pdata);

	switch (channel->direction) {
	case DMA_DIR_HMEM_TO_LMEM:
		*avail = size;
		break;
	case DMA_DIR_LMEM_TO_HMEM:
		*free = size;
		break;
	default:
		tr_err(&ddma_tr, "get_data_size direction: %d",
		       channel->direction);
		return -EINVAL;
	}
	return 0;
}

static int dummy_dma_interrupt(struct dma_chan_data *channel,
			       enum dma_irq_cmd cmd)
{
	/* Software DMA doesn't need any interrupts */
	return 0;
}

static int dummy_dma_get_attribute(struct dma *dma, uint32_t type,
				   uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = sizeof(void *);
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = PLATFORM_DCACHE_ALIGN;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = DUMMY_DMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		return -ENOENT; /* Attribute not found */
	}
	return 0;
}

const struct dma_ops dummy_dma_ops = {
	.channel_get	= dummy_dma_channel_get,
	.channel_put	= dummy_dma_channel_put,
	.start		= dummy_dma_start,
	.stop		= dummy_dma_stop,
	.pause		= dummy_dma_pause,
	.release	= dummy_dma_release,
	.copy		= dummy_dma_copy,
	.status		= dummy_dma_status,
	.set_config	= dummy_dma_set_config,
	.probe		= dummy_dma_probe,
	.remove		= dummy_dma_remove,
	.get_data_size	= dummy_dma_get_data_size,
	.interrupt	= dummy_dma_interrupt,
	.get_attribute	= dummy_dma_get_attribute,
};
