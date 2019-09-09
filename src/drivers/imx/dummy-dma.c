// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/atomic.h>
#include <sof/lib/dma.h>
#include <errno.h>
#include <stdint.h>
#include <sof/string.h>
#include <config.h>
#include <sof/audio/component.h>

#define trace_dummydma(__e, ...) \
	trace_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define tracev_dummydma(__e, ...) \
	tracev_event(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)
#define trace_dummydma_error(__e, ...) \
	trace_error(TRACE_CLASS_DMA, __e, ##__VA_ARGS__)

#define DMA_MAX_CHANS 16

struct dma_chan_pdata {
	struct dma_sg_elem_array *elems;
	int sg_elem_curr_idx;
	int sg_elem_progress_bytes; /* TODO this may remain unused */
};

struct dma_pdata {
	struct dma *dma;
	int32_t num_channels;
	struct dma_chan_data chan[DMA_MAX_CHANS];
	struct dma_chan_pdata chanp[DMA_MAX_CHANS];
};

/* allocate next free DMA channel */
static struct dma_chan_data *dummy_dma_channel_get(struct dma *dma,
						   unsigned int req_chan)
{
	struct dma_pdata *p = dma_get_drvdata(dma);
	uint32_t flags;
	int channel;

	trace_dummydma("dummy-dmac: get");
	spin_lock_irq(dma->lock, flags);
	for (channel = 0; channel < DMA_MAX_CHANS; channel++) {
		/* use channel if it's free */
		if (p->chan[channel].status == COMP_STATE_INIT) {
			p->chan[channel].status = COMP_STATE_READY;

			atomic_add(&dma->num_channels_busy, 1);

			/* return channel */
			spin_unlock_irq(dma->lock, flags);
			dma_chan_set_data(&p->chan[channel], &p->chanp[channel]);
			return &p->chan[channel];
		}
	}
	spin_unlock_irq(dma->lock, flags);
	trace_dummydma_error("dummy-dmac: %d no free channel",
			     dma->plat_data.id);
	return NULL;
}

static void dummy_dma_channel_put_unlocked(struct dma_chan_data *channel)
{
	struct dma_chan_pdata *ch = dma_chan_get_data(channel);

	/* Reset channel state */
	channel->cb = NULL;
	channel->cb_type = 0;
	channel->cb_data = NULL;

	ch->elems = NULL;
	channel->desc_count = 0;
	ch->sg_elem_curr_idx = 0;

	ch->sg_elem_progress_bytes = 0;

	channel->status = COMP_STATE_INIT;
	atomic_sub(&channel->dma->num_channels_busy, 1);
}

/* channel must not be running when this is called */
static void dummy_dma_channel_put(struct dma_chan_data *channel)
{
	uint32_t flags;

	trace_dummydma("dummy-dmac: put");
	spin_lock_irq(channel->dma->lock, flags);
	dummy_dma_channel_put_unlocked(channel);
	spin_unlock_irq(channel->dma->lock, flags);
}

static int docopies(struct dma_chan_data *ch, uint32_t cbtype, int bytes);

static int dummy_dma_start(struct dma_chan_data *channel)
{
	trace_dummydma("dummy-dmac: start");
	return docopies(channel, DMA_CB_TYPE_IRQ, 0);
}


static int dummy_dma_release(struct dma_chan_data *channel)
{
	trace_dummydma("dummy-dmac: release (no-op!)");
	return 0;
}

static int dummy_dma_pause(struct dma_chan_data *channel)
{
	trace_dummydma("dummy-dmac: pause (no-op!)");
	return 0;
}

static int dummy_dma_stop(struct dma_chan_data *channel)
{
	trace_dummydma("dummy-dmac: stop (no-op!)");
	return 0;
}

/* fill in "status" with current DMA channel state and position */
static int dummy_dma_status(struct dma_chan_data *channel,
			    struct dma_chan_status *status,
			    uint8_t direction)
{
	struct dma_chan_pdata *ch = dma_chan_get_data(channel);

	(void) direction;

	trace_dummydma("dummy-dmac: status");

	status->state = channel->status;
	status->flags = 0;
	/* TODO the below should actually be *pointers* on the read and
	 * write ends */
	status->w_pos = ch->sg_elem_progress_bytes; /* TODO wrong */
	status->r_pos = ch->sg_elem_progress_bytes; /* TODO wrong */

	status->timestamp = 0; /* TODO */
	return 0;
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int dummy_dma_set_config(struct dma_chan_data *channel,
				struct dma_sg_config *config)
{
	struct dma_chan_pdata *ch = dma_chan_get_data(channel);
	//struct dma_sg_elem *sg_elem; /* TODO what is this: check buffer range? */
	//uint32_t buffer_addr = 0; /* TODO what is this */
	//uint32_t period_bytes = 0; /* TODO what is this */
	//uint32_t buffer_bytes = 0; /* TODO what is this */
	uint32_t flags;
	int ret = 0;

	trace_dummydma("dummy-dmac: set config");
	spin_lock_irq(channel->dma->lock, flags);

	trace_dummydma("dummy-dmac: %d channel %d -> config", channel->dma->plat_data.id,
		       channel->index);

	if (!config->elem_array.count) {
		trace_dummydma_error("dummy-dmac: %d channel %d no DMA descriptors",
				     channel->dma->plat_data.id, channel->index);

		ret = -EINVAL;
		goto out;
	}

	channel->direction = config->direction;
	channel->desc_count = config->elem_array.count;
	ch->elems = &config->elem_array;
	ch->sg_elem_curr_idx = 0;

	tracev_dummydma("dummy-dmac: %d channel %d direction %d",
			channel->dma->plat_data.id, channel->index, config->direction);

	/* TODO remove next line */
	ch->sg_elem_progress_bytes = 0;
	/* TODO is this even correct? Elements may be unused.... */
	//p->chan[channel].period_bytes = period_bytes;
	//p->chan[channel].buffer_bytes = buffer_bytes;

	/* Once the configuration is done, transition into PREPARE (TODO
	 * remove comment) */
	channel->status = COMP_STATE_PREPARE;
out:
	spin_unlock_irq(channel->dma->lock, flags);
	return ret;
}

/* restore DMA context after leaving D3 */
static int dummy_dma_pm_context_restore(struct dma *dma)
{
	return 0;
}

/* store DMA context after leaving D3 */
static int dummy_dma_pm_context_store(struct dma *dma)
{
	return 0;
}

static int dummy_dma_set_cb(struct dma_chan_data *channel, int type,
		void (*cb)(void *data, uint32_t type, struct dma_cb_data *next),
		void *data)
{
	channel->cb = cb;
	channel->cb_data = data;
	channel->cb_type = type;
	return 0;
}

static void __attribute__((used)) checkdebug(void *dest, int bytes) {
	int orig_bytes = bytes;
	trace_dummydma("dummy-dmac -> checkdebug");
	if (bytes <= 1)
		return;
	char *buf = (char *)dest;
	if (bytes > 256)
		bytes = 256;
	for (int i = 1; i < bytes; i++)
		if (buf[i] != buf[0])
			return;
	trace_dummydma("dummy-dmac -> copy -- got %d/%d bytes of value %d", bytes, orig_bytes, buf[0]);
}

static bool get_next_elem(struct dma_chan_data *channel, struct dma_cb_data *next) {
	struct dma_chan_pdata *ch = dma_chan_get_data(channel);
	int curr_index = ch->sg_elem_curr_idx++;

	if (curr_index >= channel->desc_count)
		return false;

	next->elem = ch->elems->elems[curr_index];
	return true;
}

static int docopy(struct dma_chan_data *ch, struct dma_cb_data *next, uint32_t cbtype)
{
	/* Perform a single elem copy */
	void *dest = (void *)next->elem.dest;
	void *src = (void *)next->elem.src;
	size_t size = next->elem.size;

	trace_dummydma("dummy_dma -> docopy");
	trace_dummydma("dummy_dma copy: %p -> %p size %zu", (uintptr_t)dest, (uintptr_t)src, size);

	/* Do the copy */
	memcpy(dest, src, size);

	//checkdebug(dest, size);
	
	if (!(ch->cb_type & cbtype)) {
		trace_dummydma("NOCB");
		return 0; /* Callback not applicable, end for now */
	}

	/* Call the callback */
	next->status = DMA_CB_STATUS_RELOAD; /* Do nothing if they don't ask for anything */
	ch->cb(ch->cb_data, cbtype, next);
	switch (next->status) {
	case DMA_CB_STATUS_RELOAD:
		/* We need to pull new elem from our elem array */
		trace_dummydma("RELOAD");
		return get_next_elem(ch, next) ? size : -size;
	case DMA_CB_STATUS_SPLIT:
		/* We got new elem in next; we may just return */
		trace_dummydma("SPLIT");
		return size;
	case DMA_CB_STATUS_END:
		trace_dummydma("END");
		/* This copy should end; we may also free the channel? */
		return -size;
	case DMA_CB_STATUS_IGNORE:
		trace_dummydma("IGNORE");
		return -size;
	default:
		trace_dummydma("DEFAULT(what the fuck)");
		trace_dummydma("status is %d", next->status);
		/* Do nothing */
		return -size;
	}
}

static int docopies(struct dma_chan_data *ch, uint32_t cbtype, int bytes) {
	/* Allocate a static "next" variable */
	struct dma_cb_data next;
	int orig_bytes = bytes;

	trace_dummydma("dummy_dma: docopies");
	if (!get_next_elem(ch, &next))
		return 0;

	for (;;) {
		int size = docopy(ch, &next, cbtype);

		if (size <= 0) {
			// Break unconditionally, we done
			bytes += size;
			break;
		}
		
		if (bytes < 0) {
			// Negative bytes, exhaust descriptors
		} else {
			bytes -= size;
			if (bytes < 0)
				break;
		}
	}
	
	return orig_bytes - bytes;
}


static int dummy_dma_copy(struct dma_chan_data *channel, int bytes,
			  uint32_t flags) {
	trace_dummydma("dummy-dmac -> copy");
	int ret;
	struct dma_cb_data next;

	next.elem.size = 0;
	ret = docopies(channel, DMA_CB_TYPE_IRQ, bytes);

	next.elem.size = ret;
	if (channel->cb_type & DMA_CB_TYPE_COPY)
		channel->cb(channel->cb_data, DMA_CB_TYPE_COPY, &next);

	return ret;
}

#if 0
static int dummy_dma_copy(struct dma *dma, unsigned int channel, int bytes,
			  uint32_t flags)
{
	/* Perform the actual copying
	 * TODO */
	trace_dummydma("dummy-dmac -> copy");
	struct dma_pdata *p = dma_get_drvdata(dma);
	struct dma_chan_data *ch = p->chan + channel;
	struct dma_cb_data next;
	uint32_t i;

	for (i = 0; i < ch->desc_count; i++) {
		struct dma_sg_elem el = ch->elems->elems[i];

		ch->sg_elem_curr_idx = i;
		ch->sg_elem_progress_bytes = 0;
		trace_dummydma("copy: dest=0x%08x, src=0x%08x, size=0x%08x", el.dest, el.src, el.size);
		memcpy((void *)el.dest, (void *)el.src, el.size);
		ch->sg_elem_progress_bytes += el.size;
		checkdebug((void *)el.dest, el.size);
	}
	/* Call the callback */
	if (ch->cb) {
		for (uint32_t type = 1; type; type <<= 1) {
resetloop:
			if (ch->cb_type & type) {
				next.status = DMA_CB_STATUS_END; /* Ensure that if unset we get no trouble */
				ch->cb(ch->cb_data, type, &next);
				/* Not sure what to do with next */
				if (next.status == DMA_CB_STATUS_SPLIT) {
					trace_dummydma("copy from cb: dest=0x%08x, src=0x%08x, size=0x%08x",
						       next.elem.dest, next.elem.src, next.elem.size);
					memcpy((void *)next.elem.dest, (void *)next.elem.src, next.elem.size);
					checkdebug((void *)next.elem.dest, next.elem.size);
					/* Reset the callback type... */
					type = 1;
					goto resetloop;
					/* Note that this is a hack;
					 * multiple callbacks aren't
					 * supported and this may not
					 * work correctly */
				}
			}
		}
	}
	/* Not sure how to use next now */
	return 0;
}
#endif

static int dummy_dma_probe(struct dma *dma)
{
	struct dma_pdata *dma_pdata;
	int i;

	trace_dummydma("dummy-dmac %d -> probe", dma->plat_data.id);

	if (dma_get_drvdata(dma)) {
		trace_dummydma_error("dummy-dmac %d already created!", dma->plat_data.id);
		return -EEXIST; /* already created */
	}

	/* allocate private data */
	dma_pdata = rzalloc(RZONE_SYS_RUNTIME | RZONE_FLAG_UNCACHED,
			    SOF_MEM_CAPS_RAM, sizeof(*dma_pdata));
	if (!dma_pdata) {
		trace_dummydma_error("dummy-dmac %d: Out of memory!", dma->plat_data.id);
		return -ENOMEM;
	}

	dma_set_drvdata(dma, dma_pdata);

	for (i = 0; i < DMA_MAX_CHANS; i++) {
		dma_pdata->chan[i].dma = dma;
		dma_pdata->chan[i].index = i;
		dma_pdata->chan[i].status = COMP_STATE_INIT;
	}

	atomic_init(&dma->num_channels_busy, 0);

	return 0;
}

static int dummy_dma_remove(struct dma *dma)
{
	tracev_dummydma("dummy_dma %d -> remove", dma->plat_data.id);
	rfree(dma_get_drvdata(dma));
	dma_set_drvdata(dma, NULL);
	return 0;
}

static int dummy_dma_get_data_size(struct dma_chan_data *channel,
				   uint32_t *avail, uint32_t *free)
{
	// TODO
	return 0;
}

static int dummy_dma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
	case DMA_ATTR_COPY_ALIGNMENT:
		*value = 4; /* Align to uint32_t; not sure, maybe 1 works? */
		return 0;
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
	.set_cb		= dummy_dma_set_cb,
	.pm_context_restore		= dummy_dma_pm_context_restore,
	.pm_context_store		= dummy_dma_pm_context_store,
	.probe		= dummy_dma_probe,
	.remove		= dummy_dma_remove,
	.get_data_size	= dummy_dma_get_data_size,
	.get_attribute	= dummy_dma_get_attribute,
};
