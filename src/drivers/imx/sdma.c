// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2020 NXP
//
// Author: Paul Olaru <paul.olaru@nxp.com>

#include <sof/audio/component.h>
#include <sof/drivers/sdma.h>
#include <sof/drivers/timer.h>
#include <sof/lib/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <sof/lib/notifier.h>
#include <sof/lib/wait.h>
#include <sof/platform.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#define trace_sdma(fmt, ...) trace_event(TRACE_CLASS_DMA, fmt, ##__VA_ARGS__)
#define tracev_sdma(fmt, ...) tracev_event(TRACE_CLASS_DMA, fmt, ##__VA_ARGS__)
#define trace_sdma_error(fmt, ...) \
	trace_error(TRACE_CLASS_DMA, fmt, ##__VA_ARGS__)

#define SDMA_BUFFER_PERIOD_COUNT 2

struct sdma_bd {
	/* SDMA BD (buffer descriptor) configuration */
	uint32_t config;

	/* Buffer addresses, typically source and destination in some
	 * order, dependent on script
	 */
	uint32_t buf_addr;
	uint32_t buf_xaddr;
} __packed;

/* SDMA core context */
struct sdma_context {
	uint32_t pc;
	uint32_t spc;
	uint32_t g_reg[8];
	uint32_t dma_xfer_regs[14];
	uint32_t scratch[8];
} __packed;

/* SDMA channel control block */
struct sdma_ccb {
	uint32_t current_bd_paddr;
	uint32_t base_bd_paddr;
	uint32_t status;
	uint32_t reserved; /* No channel descriptor implemented */
};

/* This structure includes all SDMA related channel data */
struct sdma_chan {
	/* Statically allocate BDs; we can change if we ever need dynamic
	 * allocation
	 */
	struct sdma_bd descriptors[SDMA_MAX_BDS];
	int descriptor_count;
	struct sdma_context *ctx;
	struct sdma_ccb *ccb;
	int hw_event;
	int sdma_chan_type;
	int fifo_paddr;
};

/* Private data for the whole controller */
struct sdma_pdata {
	/* Statically allocate channel private data and contexts array.
	 * CCBs must be allocated as array anyway.
	 */
	struct sdma_chan *chan_pdata;
	struct sdma_context *contexts;
	struct sdma_ccb *ccb_array;
};

static void sdma_set_overrides(struct dma_chan_data *channel,
			       bool event_override, bool host_override)
{
	tracev_sdma("sdma_set_overrides(%d, %d)", event_override,
			 host_override);
	dma_reg_update_bits(channel->dma, SDMA_EVTOVR, BIT(channel->index),
			    event_override ? BIT(channel->index) : 0);
	dma_reg_update_bits(channel->dma, SDMA_HOSTOVR, BIT(channel->index),
			    host_override ? BIT(channel->index) : 0);
}

static int sdma_run_c0(struct dma *dma, uint8_t cmd, uint32_t buf_addr,
		       uint16_t sdma_addr, uint16_t count)
{
	struct dma_chan_data *c0 = dma->chan;
	struct sdma_chan *c0data = dma_chan_get_data(c0);
	int ret;

	tracev_sdma("sdma_run_c0 cmd %d buf_addr 0x%08x sdma_addr 0x%04x count %d",
		    cmd, buf_addr, sdma_addr, count);

	c0data->descriptors[0].config = SDMA_BD_CMD(cmd) | SDMA_BD_COUNT(count)
		| SDMA_BD_WRAP | SDMA_BD_DONE | SDMA_BD_INT | SDMA_BD_CONT;
	c0data->descriptors[0].buf_addr = buf_addr;
	c0data->descriptors[0].buf_xaddr = sdma_addr;
	if (sdma_addr)
		c0data->descriptors[0].config |= SDMA_BD_EXTD;

	c0data->ccb->current_bd_paddr = (uint32_t)&c0data->descriptors[0];
	c0data->ccb->base_bd_paddr = (uint32_t)&c0data->descriptors[0];

	/* Writeback descriptors and CCB */
	dcache_writeback_region(c0data->descriptors,
				sizeof(c0data->descriptors[0]));
	dcache_writeback_region(c0data->ccb, sizeof(*c0data->ccb));

	/* Set event override to true so we can manually start channel */
	sdma_set_overrides(c0, true, false);
	ret = dma_start(&dma->chan[0]);
	if (ret < 0)
		return ret;

	/* 1 is BIT(0) for channel 0, the bit will be cleared as the
	 * channel finishes execution. 1ms is sufficient if everything is fine.
	 */
	ret = poll_for_register_delay(dma_base(dma) + SDMA_STOP_STAT,
				      1, 0, 1000);
	if (ret >= 0)
		ret = 0;

	if (ret < 0)
		trace_sdma_error("SDMA channel 0 timed out");

	/* Switch to dynamic context switch mode if needed. This saves power. */
	if ((dma_reg_read(dma, SDMA_CONFIG) & SDMA_CONFIG_CSM_MSK) ==
	    SDMA_CONFIG_CSM_STATIC)
		dma_reg_update_bits(dma, SDMA_CONFIG, SDMA_CONFIG_CSM_MSK,
				    SDMA_CONFIG_CSM_DYN);

	tracev_sdma("sdma_run_c0 done, ret = %d", ret);

	return ret;
}

static int sdma_register_init(struct dma *dma)
{
	int ret;
	struct sdma_pdata *pdata = dma_get_drvdata(dma);

	tracev_sdma("sdma_register_init");
	dma_reg_write(dma, SDMA_RESET, 1);
	/* Wait for 10us */
	ret = poll_for_register_delay(dma_base(dma) + SDMA_RESET, 1, 0, 1000);
	if (ret < 0) {
		trace_sdma_error("SDMA reset error, base address 0x%08x",
				 (uintptr_t)dma_base(dma));
		return ret;
	}

	/* Ack all interrupts, they're leftover */
	dma_reg_write(dma, SDMA_INTR, MASK(31, 0));

	/* SDMA requires static context switch for first execution of channel 0
	 * in the future. Set it to static here, then have it change to dynamic
	 * after this first execution of channel 0 completes.
	 *
	 * Also set ACR bit according to hardware configuration. Each platform
	 * may have a different configuration.
	 */
#if SDMA_CORE_RATIO
	dma_reg_update_bits(dma, SDMA_CONFIG,
			    SDMA_CONFIG_CSM_MSK | SDMA_CONFIG_ACR,
			    SDMA_CONFIG_ACR);
#else
	dma_reg_update_bits(dma, SDMA_CONFIG,
			    SDMA_CONFIG_CSM_MSK | SDMA_CONFIG_ACR, 0);
#endif
	/* Set 32-word scratch memory size */
	dma_reg_update_bits(dma, SDMA_CHN0ADDR, BIT(14), BIT(14));

	/* Reset channel enable map (it doesn't reset with the controller).
	 * It shall be updated whenever channels need to be activated by
	 * hardware events.
	 */
	for (int i = 0; i < SDMA_HWEVENTS_COUNT; i++)
		dma_reg_write(dma, SDMA_CHNENBL(i), 0);

	/* Write ccb_array pointer to SDMA controller */
	dma_reg_write(dma, SDMA_MC0PTR, (uint32_t)pdata->ccb_array);

	return 0;
}

static void sdma_init_c0(struct dma *dma)
{
	struct dma_chan_data *c0 = &dma->chan[0];
	struct sdma_pdata *sdma_pdata = dma_get_drvdata(dma);
	struct sdma_chan *pdata = &sdma_pdata->chan_pdata[0];

	tracev_sdma("sdma_init_c0");
	c0->status = COMP_STATE_READY;

	/* Reset channel 0 private data */
	memset(pdata, 0, sizeof(*pdata));
	pdata->ctx = sdma_pdata->contexts;
	pdata->ccb = sdma_pdata->ccb_array;
	pdata->hw_event = -1;
	dma_chan_set_data(c0, pdata);
}

static int sdma_boot(struct dma *dma)
{
	int ret;

	tracev_sdma("sdma_boot");
	ret = sdma_register_init(dma);
	if (ret < 0)
		return ret;

	sdma_init_c0(dma);

	tracev_sdma("sdma_boot done");
	return 0;
}

static int sdma_upload_context(struct dma_chan_data *chan)
{
	struct sdma_chan *pdata = dma_chan_get_data(chan);

	/* Ensure context is ready for upload */
	dcache_writeback_region(pdata->ctx, sizeof(*pdata->ctx));

	tracev_sdma("sdma_upload_context for channel %d", chan->index);

	/* Last parameters are unneeded for this command and are ignored;
	 * set to 0.
	 */
	return sdma_run_c0(chan->dma, SDMA_CMD_C0_SET_DM, (uint32_t)pdata->ctx,
			   SDMA_SRAM_CONTEXTS_BASE +
			   chan->index * sizeof(*pdata->ctx) / 4,
			   sizeof(*pdata->ctx) / 4);
}

static int sdma_upload_contexts_all(struct dma *dma)
{
	struct sdma_pdata *pdata = dma_get_drvdata(dma);

	tracev_sdma("sdma_upload_contexts_all");
	dcache_writeback_region(pdata->contexts, sizeof(*pdata->contexts));

	 /* Division by 4 in size calculation is because count is in words and
	  * not in bytes
	  */
	return sdma_run_c0(dma, SDMA_CMD_C0_SET_DM, (uint32_t)pdata->contexts,
			   SDMA_SRAM_CONTEXTS_BASE,
			   dma->plat_data.channels *
			   sizeof(*pdata->contexts) / 4);
}

static int sdma_download_contexts_all(struct dma *dma)
{
	struct sdma_pdata *pdata = dma_get_drvdata(dma);
	int ret;

	tracev_sdma("sdma_download_contexts_all");

	 /* Division by 4 in size calculation is because count is in words and
	  * not in bytes
	  */
	ret = sdma_run_c0(dma, SDMA_CMD_C0_GET_DM, (uint32_t)pdata->contexts,
			  SDMA_SRAM_CONTEXTS_BASE,
			  dma->plat_data.channels *
			  sizeof(*pdata->contexts) / 4);

	dcache_invalidate_region(pdata->contexts, sizeof(*pdata->contexts));

	return ret;
}

/* Below SOF related functions will be placed */

static int sdma_probe(struct dma *dma)
{
	int channel;
	int ret;
	struct sdma_pdata *pdata;

	if (dma->chan) {
		trace_sdma_error("SDMA: Repeated probe");
		return -EEXIST;
	}

	trace_sdma("SDMA: probe");

	dma->chan = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			    dma->plat_data.channels *
			    sizeof(struct dma_chan_data));
	if (!dma->chan) {
		trace_sdma_error("SDMA: Probe failure, unable to allocate channel descriptors");
		return -ENOMEM;
	}

	pdata = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			sizeof(*pdata));
	if (!pdata) {
		rfree(dma->chan);
		dma->chan = NULL;
		trace_sdma_error("SDMA: Probe failure, unable to allocate private data");
		return -ENOMEM;
	}
	dma_set_drvdata(dma, pdata);

	for (channel = 0; channel < dma->plat_data.channels; channel++) {
		dma->chan[channel].index = channel;
		dma->chan[channel].dma = dma;
	}

	pdata->chan_pdata = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				    dma->plat_data.channels *
				    sizeof(struct sdma_chan));
	if (!pdata->chan_pdata) {
		ret = -ENOMEM;
		trace_sdma_error("SDMA: probe: out of memory");
		goto err;
	}

	pdata->contexts = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				  dma->plat_data.channels *
				  sizeof(struct sdma_context));
	if (!pdata->contexts) {
		ret = -ENOMEM;
		trace_sdma_error("SDMA: probe: unable to allocate contexts");
		goto err;
	}

	pdata->ccb_array = rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				   dma->plat_data.channels *
				   sizeof(struct sdma_ccb));
	if (!pdata->ccb_array) {
		ret = -ENOMEM;
		trace_sdma_error("SDMA: probe: unable to allocate CCBs");
		goto err;
	}

	ret = sdma_boot(dma);
	if (ret < 0) {
		trace_sdma_error("SDMA: Unable to boot");
		goto err;
	}

	goto out;
err:
	if (pdata->chan_pdata)
		rfree(pdata->chan_pdata);
	if (pdata->contexts)
		rfree(pdata->contexts);
	if (pdata->ccb_array)
		rfree(pdata->ccb_array);
	/* Failures of allocation were treated already */
	rfree(dma_get_drvdata(dma));
	rfree(dma->chan);
	dma_set_drvdata(dma, NULL);
	dma->chan = NULL;
out:
	return ret;
}

static int sdma_remove(struct dma *dma)
{
	struct sdma_pdata *pdata = dma_get_drvdata(dma);

	if (!dma->chan) {
		trace_sdma_error("SDMA: Remove called without probe, that's a noop");
		return 0;
	}

	tracev_sdma("sdma_remove");

	/* Prevent all channels except channel 0 from running */
	dma_reg_write(dma, SDMA_HOSTOVR, 1);
	dma_reg_write(dma, SDMA_EVTOVR, 0);

	/* Stop all channels except channel 0 */
	dma_reg_write(dma, SDMA_STOP_STAT, ~1);

	/* Reset SDMAC */
	dma_reg_write(dma, SDMA_RESET, 1);

	/* Free all memory related to SDMA */
	rfree(pdata->chan_pdata);
	rfree(pdata->contexts);
	rfree(pdata->ccb_array);
	rfree(dma->chan);
	dma->chan = NULL;

	return 0;
}

static struct dma_chan_data *sdma_channel_get(struct dma *dma,
					      unsigned int chan)
{
	struct sdma_pdata *pdata = dma_get_drvdata(dma);
	struct dma_chan_data *channel;
	struct sdma_chan *cdata;
	int i;
	/* Ignoring channel; let's just allocate a free channel */

	tracev_sdma("sdma_channel_get");
	for (i = 0; i < dma->plat_data.channels; i++) {
		channel = &dma->chan[i];
		if (channel->status != COMP_STATE_INIT)
			continue;

		/* Reset channel private data */
		cdata = &pdata->chan_pdata[i];
		memset(cdata, 0, sizeof(*cdata));
		cdata->ctx = pdata->contexts + i;
		cdata->ccb = pdata->ccb_array + i;
		cdata->hw_event = -1;

		channel->status = COMP_STATE_READY;
		dma_chan_set_data(channel, cdata);

		/* Allow events, allow manual */
		sdma_set_overrides(channel, true, false);
		return channel;
	}
	trace_sdma_error("sdma no channel free");
	return NULL;
}

static void sdma_clear_event(struct dma_chan_data *channel)
{
	struct sdma_chan *pdata = dma_chan_get_data(channel);

	tracev_sdma("sdma_clear_event(%d); old event is %d", channel->index,
		    pdata->hw_event);
	if (pdata->hw_event != -1)
		dma_reg_update_bits(channel->dma, SDMA_CHNENBL(pdata->hw_event),
				    BIT(channel->index), 0);
	pdata->hw_event = -1;
}

static void sdma_set_event(struct dma_chan_data *channel, int eventnum)
{
	struct sdma_chan *pdata = dma_chan_get_data(channel);

	if (eventnum < -1 || eventnum > SDMA_HWEVENTS_COUNT)
		return; /* No change if request is invalid */
	sdma_clear_event(channel);
	if (eventnum == -1) {
		sdma_set_overrides(channel, true, false);
		return;
	}

	tracev_sdma("sdma_set_event(%d, %d)", channel->index, eventnum);

	dma_reg_update_bits(channel->dma, SDMA_CHNENBL(eventnum),
			    BIT(channel->index), BIT(channel->index));
	pdata->hw_event = eventnum;

	/* Set correct overrides for event-driven channels:
	 * EVTOVR=0 HOSTOVR=1
	 */
	sdma_set_overrides(channel, false, true);
}

static void sdma_channel_put(struct dma_chan_data *channel)
{
	if (channel->status == COMP_STATE_INIT)
		return; /* Channel was already free */
	tracev_sdma("sdma_channel_put(%d)", channel->index);
	dma_interrupt(channel, DMA_IRQ_CLEAR);
	sdma_clear_event(channel);
	sdma_set_overrides(channel, false, false);
	channel->status = COMP_STATE_INIT;
}

static int sdma_start(struct dma_chan_data *channel)
{
	struct sdma_chan *pdata = dma_chan_get_data(channel);

	trace_sdma_error("sdma_start(%d)", channel->index);


	if (channel->index) {
		/* rest of the channels are prepared first */
		if (channel->status != COMP_STATE_PREPARE &&
		    channel->status != COMP_STATE_SUSPEND)
			return -EINVAL;
	} else {
		/* channel 0 doesn't get set_config set on */
		if (channel->status != COMP_STATE_READY)
			return -EINVAL;
	}

	channel->status = COMP_STATE_ACTIVE;

	/* If channel is event driven, allow it to run by setting HOSTOVR.
	 * If it's manually controlled, kickstart it by writing to SDMA_HSTART.
	 */
	if (pdata->hw_event != -1)
		dma_reg_update_bits(channel->dma, SDMA_HOSTOVR,
				    BIT(channel->index), BIT(channel->index));
	else
		dma_reg_write(channel->dma, SDMA_HSTART, BIT(channel->index));

	/* Set a runnable channel priority (channel 0 requires maximum priority)
	 * so it remains usable even if others are schedulable.
	 */
	if (channel->index)
		dma_reg_write(channel->dma, SDMA_CHNPRI(channel->index),
			      SDMA_DEFPRI);
	else
		dma_reg_write(channel->dma, SDMA_CHNPRI(0), SDMA_MAXPRI);

	return 0;
}

static int sdma_stop(struct dma_chan_data *channel)
{
	struct sdma_chan *pdata = dma_chan_get_data(channel);

	/* do not try to stop multiple times */
	if (channel->status != COMP_STATE_ACTIVE &&
	    channel->status != COMP_STATE_PAUSED)
		return 0;

	channel->status = COMP_STATE_READY;

	tracev_sdma("sdma_stop(%d)", channel->index);
	if (pdata->hw_event != -1) {
		/* For event driven channels, disable them from running by
		 * setting HOSTOVR to 0. Manually controlled channels need not
		 * be stopped as they will finish their transfer and stop on
		 * their own.
		 */
		dma_reg_update_bits(channel->dma, SDMA_HOSTOVR,
				    BIT(channel->index), 0);

		/* Reset channel, making it ready to start over */
		pdata->ccb->current_bd_paddr = pdata->ccb->base_bd_paddr;
		return sdma_upload_context(channel);
	}

	return 0;
}

static int sdma_pause(struct dma_chan_data *channel)
{
	struct sdma_chan *pdata = dma_chan_get_data(channel);

	if (channel->status != COMP_STATE_ACTIVE)
		return -EINVAL;

	channel->status = COMP_STATE_PAUSED;

	/* Manually controlled channels need not be paused. */
	if (pdata->hw_event != -1)
		dma_reg_update_bits(channel->dma, SDMA_HOSTOVR,
				    BIT(channel->index), 0);

	return 0;
}

static int sdma_release(struct dma_chan_data *channel)
{
	if (channel->status != COMP_STATE_PAUSED)
		return -EINVAL;

	channel->status = COMP_STATE_ACTIVE;

	/* No pointer realignment is necessary for release, context points
	 * correctly to beginning of the following BD.
	 */
	return 0;
}

static int sdma_copy(struct dma_chan_data *channel, int bytes, uint32_t flags)
{
	/* Check the buffer descriptors, set the "DONE" bit on one of
	 * them. Ignoring "bytes" value, if the pipeline is incorrect it
	 * will be audible.
	 */
	struct sdma_chan *pdata = dma_chan_get_data(channel);

	tracev_sdma("sdma_copy");
	for (int i = 0; i < pdata->descriptor_count; i++) {
		/* Work around the fact that we cannot allocate uncached memory
		 * on all platforms supporting SDMA.
		 */
		dcache_invalidate_region(&pdata->descriptors[i].config,
					 sizeof(pdata->descriptors[i].config));
		pdata->descriptors[i].config |= SDMA_BD_DONE;
		dcache_writeback_region(&pdata->descriptors[i].config,
					sizeof(pdata->descriptors[i].config));
	}
	return 0;
}

static int sdma_status(struct dma_chan_data *channel,
		       struct dma_chan_status *status, uint8_t direction)
{
	struct sdma_chan *pdata = dma_chan_get_data(channel);
	struct sdma_bd *bd;

	tracev_sdma("sdma_status");
	if (channel->status == COMP_STATE_INIT)
		return -EINVAL;
	status->state = channel->status;
	status->flags = 0;
	status->w_pos = 0;
	status->r_pos = 0;
	status->timestamp = timer_get_system(timer_get());

	bd = (struct sdma_bd *)pdata->ccb->current_bd_paddr;

	switch (pdata->sdma_chan_type) {
	case SDMA_CHAN_TYPE_AP2AP:
		/* We won't ever enable MMU will we? */
		status->r_pos = bd->buf_addr;
		status->w_pos = bd->buf_xaddr;
		break;
	case SDMA_CHAN_TYPE_AP2MCU:
		status->r_pos = bd->buf_addr;
		status->w_pos = pdata->fifo_paddr;
		/* We cannot see the target address */
		break;
	case SDMA_CHAN_TYPE_MCU2AP:
		status->w_pos = bd->buf_addr;
		status->r_pos = pdata->fifo_paddr;
		break;
	}
	return 0;
}

static int sdma_set_config(struct dma_chan_data *channel,
			   struct dma_sg_config *config)
{
	struct sdma_chan *pdata = dma_chan_get_data(channel);
	int handshake;
	bool src_may_change, dst_may_change;
	uint32_t fifo_paddr = 0;
	int i;
	int hwevent, watermark;
	int width;
	int ret;
	uint32_t sdma_script_addr;

	tracev_sdma("sdma_set_config channel %d", channel->index);
	/* Data to store in the descriptors:
	 * 1) Each descriptor corresponds to each of the
	 *    config->elem_array elems; if we have more than
	 *    MAX_DESCRIPTORS we bail outright. For the future, we could
	 *    allocate the per-channel descriptors dynamically.
	 * 2) For each of them, store the host side (SDRAM side) as
	 *    buf_addr and keep the FIFO address as a separate variable.
	 *    Complain if this address changes between descriptors as we
	 *    do not support this for now.
	 * 3) Enable interrupts, set up transfer width, length of elem,
	 *    wrap bit on the last descriptor, host side address, and
	 *    finally the DONE bit so the SDMA can use the descriptors.
	 * 4) The FIFO address will be stored in the context.
	 * 5) Actually upload context now as we are inside DAI prepare.
	 *    We have no other opportunity in the future.
	 */

	/* Validate requested configuration */
	if (config->elem_array.count > SDMA_MAX_BDS) {
		trace_sdma_error("sdma_set_config: Unable to handle %d descriptors",
				 config->elem_array.count);
		return -EINVAL;
	}
	if (config->elem_array.count <= 0) {
		trace_sdma_error("sdma_set_config: Invalid descriptor count: %d",
				 config->elem_array.count);
		return -EINVAL;
	}

	channel->is_scheduling_source = config->is_scheduling_source;
	channel->direction = config->direction;

	switch (config->direction) {
	case DMA_DIR_MEM_TO_DEV:
		src_may_change = true; dst_may_change = false;
		handshake = config->dest_dev;
		pdata->sdma_chan_type = SDMA_CHAN_TYPE_SHP2MCU;
		break;
	case DMA_DIR_DEV_TO_MEM:
		src_may_change = false; dst_may_change = true;
		handshake = config->src_dev;
		pdata->sdma_chan_type = SDMA_CHAN_TYPE_MCU2SHP;
		break;
	case DMA_DIR_MEM_TO_MEM:
		pdata->sdma_chan_type = SDMA_CHAN_TYPE_AP2AP;
		/* Fallthrough, TODO implement if desired, both are true */
	default:
		trace_sdma_error("sdma_set_config: Unsupported direction %d",
				 config->direction);
		return -EINVAL;
	}

	for (i = 0; i < config->elem_array.count; i++) {
		if (!src_may_change) {
			/* src is fifo */
			if (i == 0) {
				fifo_paddr = config->elem_array.elems[0].src;
			} else if (fifo_paddr != config->elem_array.elems[i].src) {
				trace_sdma_error("sdma_set_config: FIFO changes address!");
				return -EINVAL;
			}
		}
		if (!dst_may_change) {
			/* dst is fifo */
			if (i == 0) {
				fifo_paddr = config->elem_array.elems[0].dest;
			} else if (fifo_paddr != config->elem_array.elems[i].dest) {
				trace_sdma_error("sdma_set_config: FIFO changes address!");
				return -EINVAL;
			}
		}
		if (config->elem_array.elems[i].size > SDMA_BD_MAX_COUNT) {
			/* Future improvement: Create multiple BDs so as to
			 * support this situation
			 */
			trace_sdma_error("sdma_set_config: elem transfers too much: %d bytes",
					 config->elem_array.elems[i].size);
			return -EINVAL;
		}
	}
	/* Checks passed, we need to populate the buffer descriptors,
	 * adjust count, configure context so the FIFO address is also
	 * stored, upload context and have everything be ready.
	 */

	/* The handshake currently only contains the hardware channel
	 * number itself.
	 */
	hwevent = handshake;
	watermark = config->burst_elems;

	for (i = 0; i < config->elem_array.count; i++) {
		/* For MEM2DEV and DEV2MEM, buf_addr holds the RAM address and
		 * the FIFO address is stored in one of the general registers of
		 * the SDMA core.
		 * For MEM2MEM the source is stored in buf_addr and destination
		 * is in buf_xaddr.
		 */
		pdata->descriptors[i].buf_xaddr = 0;
		pdata->descriptors[i].buf_addr =
			config->elem_array.elems[i].src;
		if (!src_may_change)
			pdata->descriptors[i].buf_addr =
				config->elem_array.elems[i].dest;
		if (src_may_change && dst_may_change) {
			/* M2M copy */
			pdata->descriptors[i].buf_xaddr =
				config->elem_array.elems[i].dest;
		}

		/* We are interested in the element width on the hardware side;
		 * on RAM side we can adjust.
		 */
		width = config->src_width;
		if (!dst_may_change)
			width = config->dest_width;

		pdata->descriptors[i].config =
			SDMA_BD_COUNT(config->elem_array.elems[i].size) |
			SDMA_BD_CMD(SDMA_CMD_XFER_SIZE(width)) | SDMA_BD_CONT;
		if (!config->irq_disabled)
			pdata->descriptors[i].config |= SDMA_BD_INT;
		if (dst_may_change) {
			/* Capture or M2M, enable this descriptor to be
			 * used by SDMAC
			 */
			pdata->descriptors[i].config |= SDMA_BD_DONE;
			/* On playback we don't do this and instead wait
			 * for copy() to let us know the data is ready.
			 * copy() is called during preload.
			 */
		}
		if (src_may_change && dst_may_change)
			pdata->descriptors[i].config |= SDMA_BD_EXTD;
	}
	/* Configure last BD to account for cyclic transfers */
	if (config->cyclic)
		pdata->descriptors[config->elem_array.count - 1].config |=
			SDMA_BD_WRAP;
	else
		pdata->descriptors[config->elem_array.count - 1].config &=
			~SDMA_BD_CONT;

	/* CCB must point to buffer descriptors array */
	memset(pdata->ccb, 0, sizeof(*pdata->ccb));
	pdata->ccb->base_bd_paddr = (uint32_t)pdata->descriptors;
	pdata->ccb->current_bd_paddr = (uint32_t)pdata->descriptors;
	pdata->descriptor_count = config->elem_array.count;

	/* Context must be configured, dependent on transfer direction */

	switch (pdata->sdma_chan_type) {
	case SDMA_CHAN_TYPE_AP2AP:
		sdma_script_addr = SDMA_SCRIPT_AP2AP_OFF;
		break;
	case SDMA_CHAN_TYPE_MCU2SHP:
		sdma_script_addr = SDMA_SCRIPT_MCU2SHP_OFF;
		break;
	case SDMA_CHAN_TYPE_SHP2MCU:
		sdma_script_addr = SDMA_SCRIPT_SHP2MCU_OFF;
		break;
	default:
		/* This case doesn't happen; we need to assign the other cases
		 * for AP2MCU and MCU2AP
		 */
		trace_sdma_error("Unexpected SDMA error");
		return -EINVAL;
	}

	memset(pdata->ctx, 0, sizeof(*pdata->ctx));
	pdata->ctx->pc = sdma_script_addr;

	if (pdata->sdma_chan_type == SDMA_CHAN_TYPE_AP2AP) {
		/* Base of RAM, TODO must be moved to a define */
		pdata->ctx->g_reg[7] = 0x40000000;
	} else {
		if (hwevent != -1) {
			if (hwevent >= 32)
				pdata->ctx->g_reg[0] |= BIT(hwevent - 32);
			else
				pdata->ctx->g_reg[1] |= BIT(hwevent);
		}
		pdata->ctx->g_reg[6] = fifo_paddr;
		pdata->ctx->g_reg[7] = watermark;
	}

	/* Upload context */
	ret = sdma_upload_context(channel);
	if (ret < 0) {
		trace_sdma_error("Unable to upload context, bailing");
		return ret;
	}

	tracev_sdma("SDMA context uploaded");
	/* Context uploaded, we can set up events now */
	sdma_set_event(channel, hwevent);

	/* Finally set channel priority */
	dma_reg_write(channel->dma, SDMA_CHNPRI(channel->index), SDMA_DEFPRI);

	channel->status = COMP_STATE_PREPARE;

	return 0;
}

static int sdma_pm_context_store(struct dma *dma)
{
	return sdma_download_contexts_all(dma);
}

static int sdma_pm_context_restore(struct dma *dma)
{
	return sdma_upload_contexts_all(dma);
}

static int sdma_interrupt(struct dma_chan_data *channel, enum dma_irq_cmd cmd)
{
	if (!channel->index) {
		trace_sdma_error("sdma_interrupt called for channel 0; ignoring command");
		return 0;
	}

	tracev_sdma("sdma_interrupt(%d cmd %d)", channel->index, cmd);

	switch (cmd) {
	case DMA_IRQ_STATUS_GET:
		/* Any nonzero value means interrupt is active */
		return dma_reg_read(channel->dma, SDMA_INTR) &
			BIT(channel->index);
	case DMA_IRQ_CLEAR:
		dma_reg_write(channel->dma, SDMA_INTR, BIT(channel->index));
		return 0;
	case DMA_IRQ_MASK:
	case DMA_IRQ_UNMASK:
		/* We cannot control interrupts except by resetting channel to
		 * have it reread the buffer descriptors. That cannot be done
		 * in the context where this function is called. Silently ignore
		 * requests to mask/unmask per-channel interrupts.
		 */
		return 0;
	default:
		trace_sdma_error("sdma_interrupt unknown cmd %d", cmd);
		return -EINVAL;
	}
}

static int sdma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
{
	switch (type) {
	case DMA_ATTR_BUFFER_ALIGNMENT:
	case DMA_ATTR_COPY_ALIGNMENT:
		/* Use a conservative value, because some scripts
		 * require an alignment of 4 while others can read
		 * unaligned data. Account for those which require
		 * aligned data.
		 */
		*value = 4;
		break;
	case DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT:
		*value = PLATFORM_DCACHE_ALIGN;
		break;
	case DMA_ATTR_BUFFER_PERIOD_COUNT:
		*value = SDMA_BUFFER_PERIOD_COUNT;
		break;
	default:
		return -ENOENT; /* Attribute not found */
	}

	return 0;
}

static int sdma_get_data_size(struct dma_chan_data *channel, uint32_t *avail,
			      uint32_t *free)
{
	/* Check buffer descriptors, those with "DONE" = 0 are for the
	 * host, "DONE" = 1 are for SDMA. The host side are either
	 * available or free.
	 */
	struct sdma_chan *pdata = dma_chan_get_data(channel);
	uint32_t result_data = 0;
	int i;

	tracev_sdma("sdma_get_data_size(%d)", channel->index);
	if (channel->index == 0) {
		/* Channel 0 shouldn't have this called anyway */
		trace_sdma_error("Please do not call get_data_size on SDMA channel 0!");
		*avail = *free = 0;
		return -EINVAL;
	}

	for (i = 0; i < pdata->descriptor_count && i < SDMA_MAX_BDS; i++) {
		if (pdata->descriptors[i].config & SDMA_BD_DONE)
			continue; /* These belong to SDMA controller */
		result_data += pdata->descriptors[i].config &
			SDMA_BD_COUNT_MASK;
	}

	*avail = *free = 0;
	switch (channel->direction) {
	case DMA_DIR_MEM_TO_DEV:
		*avail = result_data;
		break;
	case DMA_DIR_DEV_TO_MEM:
		*free = result_data;
		break;
	default:
		trace_sdma_error("sdma_get_data_size channel invalid direction");
		return -EINVAL;
	}
	return 0;
}

const struct dma_ops sdma_ops = {
	.channel_get	= sdma_channel_get,
	.channel_put	= sdma_channel_put,
	.start		= sdma_start,
	.stop		= sdma_stop,
	.pause		= sdma_pause,
	.release	= sdma_release,
	.copy		= sdma_copy,
	.status		= sdma_status,
	.set_config	= sdma_set_config,
	.pm_context_restore	= sdma_pm_context_restore,
	.pm_context_store	= sdma_pm_context_store,
	.probe		= sdma_probe,
	.remove		= sdma_remove,
	.interrupt	= sdma_interrupt,
	.get_attribute	= sdma_get_attribute,
	.get_data_size	= sdma_get_data_size,
};
