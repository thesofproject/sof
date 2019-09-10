// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright 2019 NXP
//
// Author: Daniel Baluta <daniel.baluta@nxp.com>

#include <sof/audio/component.h>
#include <sof/drivers/edma.h>
#include <sof/lib/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/io.h>
#include <stdint.h>

static inline void
edma_chan_write(struct dma_chan_data *channel, uint32_t reg, uint32_t value)
{
	io_reg_write(dma_chan_base(channel->dma, channel->index) + reg, value);
}

static inline void
edma_chan_write16(struct dma_chan_data *channel, uint32_t reg, uint16_t value)
{
	io_reg_write16(dma_chan_base(channel->dma, channel->index) + reg, value);
}

static inline uint32_t edma_chan_read(struct dma_chan_data *channel, uint32_t reg)
{
	return io_reg_read(dma_chan_base(channel->dma, channel->index) + reg);
}

static inline uint16_t edma_chan_read16(struct dma_chan_data *channel, uint32_t reg)
{
	return io_reg_read16(dma_chan_base(channel->dma, channel->index) + reg);
}

static inline void edma_chan_update_bits(struct dma_chan_data *channel,
					 uint32_t reg, uint32_t mask,
					 uint32_t value)
{
	io_reg_update_bits(dma_chan_base(channel->dma, channel->index) + reg, mask, value);
}

struct edma_tcd {
	uint32_t SADDR;
	uint16_t SOFF;
	uint16_t ATTR;
	uint32_t NBYTES;
	uint32_t SLAST;
	uint32_t DADDR;
	uint16_t DOFF;
	uint16_t CITER;
	uint32_t DLAST_SGA;
	uint16_t CSR;
	uint16_t BITER;
} __attribute__((aligned(32)));

struct edma_ch_data {
	/* Channel-specific configuration data that isn't stored in the
	 * HW registers */
//	struct dma_sg_config config;
	struct edma_tcd tcd_cache;
	struct edma_tcd *sg_tcds;
	void *sg_tcds_alloc;
	int sg_tcds_count;
	/* TCD START bit should be directly in HW */
//	bool INTHALF; /* Enable interrupts when half of the major loop is complete */
//	bool INTMAJ; /* Enable interrupts at end of major loop */
	/* ACTIVE should only be checked in the actual HW TCD */
	/* DONE should not be stored locally */
//	bool EEI; /* Enable interrupt on error */
//	bool EARQ; /* Enable async wake-up */
//	bool ERQ; /* Enable hardware service requests; prereq for EARQ */
	/* We will not support ELINK and MAJORELINK */
	/* Callback support */
};

struct edma_data {
	/* General configuration data that isn't stored in the HW
	 * registers */
	struct dma_chan_data *channels;
	/* ACTIVE check from HW area whether it's currently doing
	 * something */
	/* ACTIVE_ID check what channel is currently running */
	/* CX: Cancel transfer should be an action; generic */
	/* ECX: Cancel transfer with error should be an action; generic */
	/* GMRC is unused; set to 0 */
	/* GCLC: Allow channel linking. Currently unused, we'll set to 0 */
	/* HALT: Suspend DMAC; active channel will still complete */
	/* HAE: On error, set HALT. Must decide on either hardcoding or
	 * offering a config option */
	/* ERCA: Enable round-robin. Might default to 1 or allow config */
	/* EDBG: Enable DMA debug. I don't think the driver should
	 * expose such functionality. We'll hardcode it based on config. */
	/* HW exposes last error conditions. Shouldn't store in this
	 * struct but rather should give relevant issues to callbacks
	 * (err handlers) */
	uint32_t int_status_cache; /* Shouldn't use this field but rather request proper int status on need */
	uint8_t chan_prio[32]; /* Should we even store channel priority groups here, or even use them? */
};

static inline void dump_tcd_offline(struct edma_tcd *tcd)
{
#define DUMP_REG32(REG) \
	tracev_edma("EDMA_" #REG ": %08x", tcd->REG)
#define DUMP_REG16(REG) \
	tracev_edma("EDMA_" #REG ": %04x", tcd->REG)
	DUMP_REG32(SADDR);
	DUMP_REG16(SOFF);
	DUMP_REG16(ATTR);
	DUMP_REG32(NBYTES);
	DUMP_REG32(SLAST);
	DUMP_REG32(DADDR);
	DUMP_REG16(DOFF);
	DUMP_REG16(CITER);
	DUMP_REG32(DLAST_SGA);
	DUMP_REG16(CSR);
	DUMP_REG16(BITER);
#undef DUMP_REG32
#undef DUMP_REG16
}

static inline void dump_tcd(struct dma_chan_data *channel)
{
	// Disable the functionality
	return;
#define DUMP_REG32(REG) \
	tracev_edma("EDMA_" #REG ": %08x", edma_chan_read(channel, EDMA_##REG))
#define DUMP_REG16(REG) \
	tracev_edma("EDMA_" #REG ": %04x", edma_chan_read16(channel, EDMA_##REG))
	DUMP_REG32(CH_CSR);
	DUMP_REG32(CH_ES);
	DUMP_REG32(CH_INT);
	DUMP_REG32(CH_SBR);
	DUMP_REG32(CH_PRI);
	DUMP_REG32(TCD_SADDR);
	DUMP_REG16(TCD_SOFF);
	DUMP_REG16(TCD_ATTR);
	DUMP_REG32(TCD_NBYTES);
	DUMP_REG32(TCD_SLAST);
	DUMP_REG32(TCD_DADDR);
	DUMP_REG16(TCD_DOFF);
	DUMP_REG16(TCD_CITER);
	DUMP_REG32(TCD_DLAST_SGA);
	DUMP_REG16(TCD_CSR);
	DUMP_REG16(TCD_BITER);
#undef DUMP_REG32
#undef DUMP_REG16
	if (edma_chan_read16(channel, EDMA_TCD_CSR) & EDMA_TCD_CSR_ESG) {
		tracev_edma("EDMA: Dumping ESG next value");
		dump_tcd_offline((struct edma_tcd *)edma_chan_read(channel, EDMA_TCD_DLAST_SGA));
	}
}

static struct edma_ch_data *get_ch_data(struct dma_chan_data *channel) {
	tracev_edma("EDMA: get_ch_data(%p)", (uintptr_t)channel);

	struct edma_ch_data *ch = dma_chan_get_data(channel);

	tracev_edma("EDMA: Channel private data already there: %p", (uintptr_t)ch);

	if (!ch) {
		tracev_edma("EDMA: About to allocate channel data");
		ch = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, sizeof(struct edma_ch_data));
		if (!ch)
			trace_edma_error("Unable to allocate private channel data for ch %d",
					 channel->index);
		tracev_edma("EDMA: Setting channel data");
		dma_chan_set_data(channel, ch);
	}

	tracev_edma("EDMA: About to return channel data: %p", (uintptr_t)ch);
	return ch;
}

/* TODO: Refactor to remove this function and the one above */
static inline struct edma_ch_data *get_ch_data_maybe(struct dma_chan_data *channel) {
	return dma_chan_get_data(channel);
}

static void free_ch_data(struct dma_chan_data *channel) {
	struct edma_ch_data *ch = dma_chan_get_data(channel);

	if (ch) {
		dma_chan_set_data(channel, NULL);
		rfree(ch);
	}
}

static void __attribute__((unused)) edma_init_test(struct dma *dma)
{
	/* Set TCD_CSR to 0x12 -- ESG and INTMAJOR enabled, the rest
	 * disabled
	 */

	/* TCD_CITER and TCD_BITER will be set to 1 */

	/* TCD_DLAST_SGA will point to next TCD for SG; linked list of
	 * TCDs
	 */

	/* TCD_SLAST_SDA will be set to -SIZE so as to reset the
	 * source pointer after the TCD is done; what about capture?
	 * Just have the TCDs in memory point properly.
	 */

	/* TCD_SADDR and TCD_DADDR will be set to source and destination
	 * accordingly; TCD_SOFF and TCD_DOFF also
	 */

	/* TCD_NBYTES will be the size of each buffer */

	/* CH_CSR shall be set to 0x7 for EARQ, ERQ, EEI */

	/* Global CSR */
	/* Or with 0x20 to cancel remaining data transfer. One of the
	 * few things that can stop the minor loop without it finishing.
	 */
	/* Initially set to 0x04, enable round robin and nothing else */
	struct dma_chan_data ch = {
		.index = 7,
		.dma = dma,
	};
	// Not supported actually... Let's hope the default CSR works
	// for us
	// OR WE COULD JUST DISABLE SG SUPPORT ENTIRELY
	//edma_write(dma, EDMA_CSR, EDMA_DEFAULT_GLOBAL_CSR);
	edma_chan_write(&ch, EDMA_CH_CSR, 7);
	edma_chan_write(&ch, EDMA_TCD_CSR, 0x12);
}

/* acquire the specific DMA channel */
static struct dma_chan_data *edma_channel_get(struct dma *dma,
					      unsigned int req_chan)
{
	struct edma_data *pdata = dma_get_drvdata(dma);
	struct dma_chan_data *channel;
	struct edma_ch_data *ch;

	tracev_edma("EDMA: channel_get(%d)", req_chan);
	if (req_chan > 32) {
		trace_edma_error("EDMA: Channel %d out of range", req_chan);
		return NULL;
	}

	channel = &pdata->channels[req_chan];
	if (channel->status != COMP_STATE_INIT) {
		trace_edma_error("EDMA: Cannot reuse channel %d", req_chan);
		return NULL;
	}

	/* Initialize channel structures */
	channel->dma = dma;
	channel->index = req_chan;

	tracev_edma("EDMA: channel_get(%d), allocating private data", req_chan);
	ch = get_ch_data(channel);
	if (!ch)
		return NULL; // If the channel is valid, failing to get it can only come from out of memory errors
	channel->status = COMP_STATE_READY;
	/* Initialize channel data */
	/* No data available for now, we will initialize when set config
	 * is called */
	tracev_edma("EDMA: channel_get(%d) returning %p", req_chan, (uintptr_t)channel);
	return channel;
}

/* channel must not be running when this is called */
static void edma_channel_put(struct dma_chan_data *channel)
{
	struct edma_ch_data *ch = get_ch_data_maybe(channel);

	if (!ch) {
		tracev_edma("EDMA: channel_put(%d) [DUMMY]", channel->index);
		return;
	}
	// Assuming channel is stopped, we thus don't need hardware to
	// do anything right now
	tracev_edma("EDMA: channel_put(%d)", channel->index);
	// Also release extra memory used for scatter-gather
	channel->status = COMP_STATE_INIT;
	if (ch->sg_tcds_alloc) {
		rfree(ch->sg_tcds_alloc);
		ch->sg_tcds_alloc = 0;
		ch->sg_tcds_count = 0;
		ch->sg_tcds = 0;
	}

	free_ch_data(channel);
}

static int edma_start(struct dma_chan_data *channel)
{
	tracev_edma("EDMA: start(%d)", channel->index);
	// Assuming the channel is already configured, just perform a
	// manual start. Also update states
	switch (channel->status) {
	case COMP_STATE_PREPARE: // Just prepared
	case COMP_STATE_SUSPEND: // Must resume though
		// Allow starting
		break;
	default:
		return -EINVAL; // Cannot start, wrong state
	}
	channel->status = COMP_STATE_ACTIVE;
	// Enable HW requests
	// Do the HW start of the DMA
	tracev_edma("EDMA: about to do start (preload)");
	edma_chan_update_bits(channel, EDMA_TCD_CSR,
			      EDMA_TCD_CSR_START, EDMA_TCD_CSR_START);
	tracev_edma("EDMA: enabling HW requests (and allowing the ESAI to autostart the DMA)");
	edma_chan_update_bits(channel, EDMA_CH_CSR,
			      EDMA_CH_CSR_ERQ_EARQ, EDMA_CH_CSR_ERQ_EARQ);
	tracev_edma("EDMA: did start");
	return 0;
}

static int edma_release(struct dma_chan_data *channel)
{
	tracev_edma("EDMA: release(%d)", channel->index);
	// Validate state
	switch (channel->status) {
	case COMP_STATE_PAUSED:
		break;
	default:
		return -EINVAL;
	}
	channel->status = COMP_STATE_ACTIVE;
	// Reenable HW requests
	edma_chan_update_bits(channel, EDMA_CH_CSR,
			      EDMA_CH_CSR_ERQ_EARQ, EDMA_CH_CSR_ERQ_EARQ);
	return 0;
}

static int edma_pause(struct dma_chan_data *channel)
{
	tracev_edma("EDMA: pause(%d)", channel->index);
	// Validate state
	switch (channel->status) {
	case COMP_STATE_ACTIVE:
		break;
	default:
		return -EINVAL;
	}
	channel->status = COMP_STATE_PAUSED;
	// Disable HW requests
	edma_chan_update_bits(channel, EDMA_CH_CSR,
			      EDMA_CH_CSR_ERQ_EARQ, 0);
	return 0;
}

static int edma_stop(struct dma_chan_data *channel)
{
	tracev_edma("EDMA: stop(%d)", channel->index);
	// Validate state
	switch (channel->status) {
	case COMP_STATE_READY:
	case COMP_STATE_PREPARE:
		return 1; // Already stopped, don't propagate request
	case COMP_STATE_PAUSED:
	case COMP_STATE_ACTIVE:
		break;
	default:
		return -EINVAL;
	}
	// TODO Should we keep config after stop? Or not?
	channel->status = COMP_STATE_READY; // Also lose the config
	// Disable channel
	edma_chan_write(channel, EDMA_CH_CSR, 0);
	edma_chan_write(channel, EDMA_TCD_CSR, 0);
	// The remaining TCD values will still be valid but I don't care
	// about them anyway

	return 0;
}

static int edma_copy(struct dma_chan_data *channel, int bytes, uint32_t flags)
{
	trace_edma_error("EDMA: ________________ edma_copy not implemented!");
	return -EINVAL;
}

static int edma_status(struct dma_chan_data *channel,
		       struct dma_chan_status *status, uint8_t direction)
{
	// TODO
	/* direction: SOF_IPC_STREAM_PLAYBACK, SOF_IPC_STREAM_CAPTURE */
	// Do I care about the direction? Me thinks not...
	switch (direction) {
	case SOF_IPC_STREAM_PLAYBACK:
		tracev_edma("EDMA: status(playback %d)", channel->index);
		break;
	case SOF_IPC_STREAM_CAPTURE:
		tracev_edma("EDMA: status(capture %d)", channel->index);
		break;
	default:
		trace_edma_error("EDMA: status(<UNKNOWN DIRECTION %d> %d)", direction, channel->index);
		break;
	}
	status->state = channel->status;
	status->flags = 0;
	// Note: these might be slightly inaccurate as they are only
	// updated at the end of each minor (block) transfer
	status->r_pos = edma_chan_read(channel, EDMA_TCD_SADDR);
	status->w_pos = edma_chan_read(channel, EDMA_TCD_DADDR);
	status->timestamp = timer_get_system(platform_timer);
	return 0;
}

static inline int signum(int x)
{
	return x < 0 ? -1 : x == 0 ? 0 : 1;
}

static int validate_nonsg_config(struct dma_sg_elem_array *sgelems, uint32_t SOFF, uint32_t DOFF)
{
	if (!sgelems)
		return -EINVAL;
	if (sgelems->count != 2)
		return -EINVAL; /* Only ping-pong configs supported */
	
	uint32_t saddr0 = sgelems->elems[0].src;
	uint32_t saddr1 = sgelems->elems[1].src;
	uint32_t size0 = sgelems->elems[0].size;
	uint32_t size1 = sgelems->elems[1].size;
	uint32_t daddr0 = sgelems->elems[0].dest;
	uint32_t daddr1 = sgelems->elems[1].dest;
	uint32_t ssize = size0 * signum(SOFF);
	uint32_t dsize = size0 * signum(DOFF);

	if (saddr0 + ssize != saddr1) {
		return -EINVAL; /* Not contiguous */
	}
	if (daddr0 + dsize != daddr1)
		return -EINVAL; /* Not contiguous */
	if (size0 != size1)
		return -EINVAL; /* Sizes must match */

	/* More checks for the wildly broken -- huge sizes or weird,
	 * perhaps unaligned, addresses, TBD */

	return 0; /* Ok, we good */
}

/* Some set_config helper functions */
static int setup_tcd(struct dma_chan_data *channel, uint16_t SOFF, uint16_t DOFF,
		     bool cyclic, bool sg, bool irqoff, struct dma_sg_elem_array *sgelems) {
	struct edma_ch_data *ch = dma_chan_get_data(channel);

	// TODO
	// Set up the normal TCD and, for SG, also set up the SG TCDs
	if (!sg) {
		// Not scatter-gather, just create a regular TCD. Don't
		// allocate anything
		// Actually, not supported? Any test cases?

		/* The only supported non-SG configurations are:
		 * -> 2 buffers
		 * -> The buffers must be of equal size
		 * -> The buffers must be contiguous
		 * -> The first buffer should be of the lower address
		 */

		int rc = validate_nonsg_config(sgelems, SOFF, DOFF);

		if (rc < 0)
			return rc;

		/* We should work directly with the TCD cache */
		ch->tcd_cache.SADDR = sgelems->elems[0].src;
		ch->tcd_cache.SOFF = SOFF;
		ch->tcd_cache.ATTR = EDMA_DEFAULT_TCD_ATTR;
		ch->tcd_cache.NBYTES = sgelems->elems[0].size;
		ch->tcd_cache.SLAST = -2 * sgelems->elems[0].size * signum(SOFF);
		ch->tcd_cache.DADDR = sgelems->elems[0].dest;
		ch->tcd_cache.DOFF = DOFF;
		ch->tcd_cache.CITER = 2;
		ch->tcd_cache.DLAST_SGA = -2 * sgelems->elems[0].size * signum(DOFF);
		ch->tcd_cache.CSR =
			irqoff ? 0 : EDMA_TCD_CSR_INTMAJOR | EDMA_TCD_CSR_INTHALF;
		ch->tcd_cache.BITER = 2;

		goto load_cached;
		/* The below two lines are unreachable */
		trace_edma_error("Unable to set up non-SG");
		return -EINVAL;
	}
	// Scatter-gather, we need to allocate additional TCDs
	// ch->edma_tcds{,_count}
	ch->sg_tcds_count = sgelems->count;
	// Since we don't (yet) have aligned allocators, we will do this
	// HACK
	ch->sg_tcds_alloc = rzalloc(RZONE_RUNTIME | RZONE_FLAG_UNCACHED, SOF_MEM_CAPS_RAM,
				    (sgelems->count + 1) * sizeof(struct edma_tcd));

	if (!ch->sg_tcds_alloc) {
		ch->sg_tcds_count = 0;
		trace_edma_error("Unable to allocate SG TCDs");
		return -ENOMEM;
	}

	ch->sg_tcds = (struct edma_tcd *) ALIGN_UP((uintptr_t)ch->sg_tcds_alloc, 32);

	// Populate each tcd
	for (int i = 0; i < ch->sg_tcds_count; i++) {
		struct edma_tcd *tcd = &ch->sg_tcds[i];
		struct dma_sg_elem *elem = &sgelems->elems[i];

		tcd->SADDR = elem->src;
		tcd->SOFF = SOFF;
		tcd->ATTR = EDMA_DEFAULT_TCD_ATTR;
		tcd->NBYTES = elem->size;
		tcd->SLAST = 0; /* Not used */
		tcd->DADDR = elem->dest;
		tcd->DOFF = DOFF;
		tcd->CITER = 1;
		tcd->DLAST_SGA = (uint32_t)&ch->sg_tcds[i+1]; // Will fixup later
		tcd->CSR = EDMA_TCD_CSR_INTMAJOR | EDMA_TCD_CSR_ESG;
		if (irqoff)
			tcd->CSR = EDMA_TCD_CSR_ESG;
		tcd->BITER = 1;
	}

	if (ch->sg_tcds_count) {
		ch->sg_tcds[ch->sg_tcds_count - 1].DLAST_SGA =
			(uint32_t) (cyclic ? &ch->sg_tcds[0] : 0);
		if (!cyclic)
			ch->sg_tcds[ch->sg_tcds_count - 1].CSR &= ~EDMA_TCD_CSR_ESG;
	}

	// We will also copy the first TCD into the cache, for later
	// loading
	
	ch->tcd_cache = ch->sg_tcds[0];

load_cached:
	/* Turn off hardware requests for now */
	edma_chan_write(channel, EDMA_CH_CSR, 0);
	// Load the HW TCD
	// Clear CSR to ensure everything else is stopped
	edma_chan_write(channel, EDMA_TCD_CSR, 0);
	// Write the other fields
#define WRITE_FIELD(FIELD) \
	edma_chan_write(channel, EDMA_TCD_##FIELD, ch->tcd_cache.FIELD)
#define WRITE_FIELD16(FIELD) \
	edma_chan_write16(channel, EDMA_TCD_##FIELD, ch->tcd_cache.FIELD)
	WRITE_FIELD(SADDR);
	WRITE_FIELD16(SOFF);
	WRITE_FIELD16(ATTR);
	WRITE_FIELD(NBYTES);
	WRITE_FIELD(SLAST);
	WRITE_FIELD(DADDR);
	WRITE_FIELD16(DOFF);
	WRITE_FIELD16(CITER);
	WRITE_FIELD(DLAST_SGA);
	WRITE_FIELD16(BITER);
	/* Write the CSR now */
	WRITE_FIELD16(CSR);
#undef WRITE_FIELD
#undef WRITE_FIELD16
	/* Enable hardware requests once the channel is ready */
	/* edit: DON'T, THIS MAY START THE CHANNEL */
	//edma_chan_write(dma, channel, EDMA_CH_CSR, EDMA_CH_CSR_ERQ_EARQ);
	/* Print the status of the CSR after load */
	tracev_edma("EDMA: CSR (for channel %d): 0x%x", channel->index,
		    edma_chan_read16(channel, EDMA_TCD_CSR));
	channel->status = COMP_STATE_PREPARE;
	return 0;
}

static void edma_chan_irq(struct dma_chan_data *channel)
{
	//tracev_edma("edma_chan_irq(%d) (TCD below)", channel->index);

	//dump_tcd(channel);

	if (!channel->cb)
		return;

	if (channel->cb_type & DMA_CB_TYPE_IRQ) {
		struct dma_cb_data next = {
			.status = DMA_CB_STATUS_RELOAD,
		};

		channel->cb(channel->cb_data, DMA_CB_TYPE_IRQ, &next);

		/* We will ignore status as it is currently
		 * never set */
		/* TODO consider changes in next.status */
		/* By default we let the TCDs just continue
		 * reloading */
	}

	//tracev_edma("edma_chan_irq(%d) done", channel->index);
}

extern void dsp_putc(const char c);

static void edma_irq(void *arg)
{
	struct dma_chan_data *channel = (struct dma_chan_data *)arg;

	//dsp_putc('#');
	//tracev_edma("EDMA IRQ");
	/* We have to check the status for all the channels and clear
	 * the interrupts
	 */
	uint32_t err_status = edma_chan_read(channel, EDMA_CH_ES);

	if (err_status & EDMA_CH_ES_ERR) {
		edma_chan_update_bits(channel, EDMA_CH_ES, EDMA_CH_ES_ERR, EDMA_CH_ES_ERR); /* Clear the error */
		/* See the error */
		trace_edma_error("EDMA: Error detected on channel %d. Printing bits:", channel->index);
		if (err_status & EDMA_CH_ES_SAE)
			trace_edma_error("EDMA: SAE");
		if (err_status & EDMA_CH_ES_SOE)
			trace_edma_error("EDMA: SOE");
		if (err_status & EDMA_CH_ES_DAE)
			trace_edma_error("EDMA: DAE");
		if (err_status & EDMA_CH_ES_DOE)
			trace_edma_error("EDMA: DOE");
		if (err_status & EDMA_CH_ES_NCE)
			trace_edma_error("EDMA: NCE");
		if (err_status & EDMA_CH_ES_SGE)
			trace_edma_error("EDMA: SGE");
		if (err_status & EDMA_CH_ES_SBE)
			trace_edma_error("EDMA: SBE");
		if (err_status & EDMA_CH_ES_DBE)
			trace_edma_error("EDMA: DBE");
	}

	/* Valid config, we will now actually handle it */
	/* Check the INT bit status */

	/* Register is EDMA_CH_INT */
	int ch_int = edma_chan_read(channel, EDMA_CH_INT);

	if (!ch_int) {
		trace_edma_error("EDMA spurious interrupt");
		return;
	}

	/* We have an interrupt, we should handle it... */
	edma_chan_irq(channel);

	/* Clear the interrupt as required by the HW specs */
	edma_chan_write(channel, EDMA_CH_INT, 1);
}

/* set the DMA channel configuration, source/target address, buffer sizes */
static int edma_set_config(struct dma_chan_data *channel,
			   struct dma_sg_config *config)
{
	int handshake;

	tracev_edma("EDMA: set config");
	switch (config->direction) {
	case DMA_DIR_MEM_TO_DEV:
		handshake = config->dest_dev;
		break;
	case DMA_DIR_DEV_TO_MEM:
		handshake = config->src_dev;
		break;
	default:
		trace_edma_error("edma_set_config() invalid config direction");
		return -EINVAL;
	}
	tracev_edma("EDMA: source width %d dest width %d burst elems %d", config->src_width, config->dest_width, config->burst_elems);
	switch (config->direction) {
#define CASE(dir) case dir: tracev_edma("EDMA: direction = " #dir ); break;
	CASE(DMA_DIR_MEM_TO_MEM)
	CASE(DMA_DIR_HMEM_TO_LMEM)
	CASE(DMA_DIR_LMEM_TO_HMEM)
	CASE(DMA_DIR_MEM_TO_DEV)
	CASE(DMA_DIR_DEV_TO_MEM)
	CASE(DMA_DIR_DEV_TO_DEV)
#undef CASE
	default:
		trace_edma_error("EDMA: set_config invalid direction %d", config->direction);
		break;
	}
	uint16_t elem_size = EDMA_TRANSFER_OFFSET_MEM;
	uint16_t SOFF = EDMA_TRANSFER_OFFSET_DEV, DOFF = EDMA_TRANSFER_OFFSET_DEV;

	switch (config->direction) {
	case DMA_DIR_MEM_TO_MEM:
	case DMA_DIR_HMEM_TO_LMEM:
	case DMA_DIR_LMEM_TO_HMEM:
		SOFF = DOFF = elem_size;
		break;
	case DMA_DIR_MEM_TO_DEV:
		SOFF = elem_size; DOFF = EDMA_TRANSFER_OFFSET_DEV;
		break;
	case DMA_DIR_DEV_TO_MEM:
		SOFF = EDMA_TRANSFER_OFFSET_DEV; DOFF = elem_size;
		break;
	case DMA_DIR_DEV_TO_DEV:
		SOFF = DOFF = EDMA_TRANSFER_OFFSET_DEV;
		break;
	}

	tracev_edma("EDMA: SOFF = %d DOFF = %d", SOFF, DOFF);
	tracev_edma("EDMA: src dev %d dest dev %d", config->src_dev, config->dest_dev);
	tracev_edma("EDMA: cyclic = %d", config->cyclic);
	if (config->scatter) {
		tracev_edma("EDMA: scatter enabled");
	} else {
		tracev_edma("EDMA: scatter disabled");
	}
	if (config->irq_disabled) {
		tracev_edma("EDMA: IRQ disabled");
	} else {
		tracev_edma("EDMA: Registering IRQ");

		int irq = EDMA_HS_GET_IRQ(handshake);
		int rc = interrupt_register(irq, IRQ_AUTO_UNMASK, &edma_irq, channel);

		if (rc == -EEXIST) {
			/* Ignore error, it's also ours */
			rc = 0;
		}

		if (rc < 0) {
			trace_edma_error("Unable to register IRQ, bailing (rc = %d)", rc);
			return rc;
		}

		interrupt_enable(irq, channel);

		/* TODO: Figure out when to disable and perhaps
		 * unregister the interrupts
		 */
	}
	tracev_edma("EDMA: %d elements", config->elem_array.count);
	for (int i = 0; i < config->elem_array.count; i++)
		tracev_edma("EDMA: elem %d src %d -> dst %d size %d bytes", i, config->elem_array.elems[i].src, config->elem_array.elems[i].dest, config->elem_array.elems[i].size);
	return setup_tcd(channel, SOFF, DOFF, config->cyclic, config->scatter,
			 config->irq_disabled, &config->elem_array);
}

/* restore DMA context after leaving D3 */
static int edma_pm_context_restore(struct dma *dma)
{
	struct edma_data *pdata = dma_get_drvdata(dma);

	tracev_edma("EDMA: resuming... We need to restore from the cache");
	for (int channel = 0; channel < 32; channel++) {
		struct dma_chan_data *dma_chan_data = &pdata->channels[channel];
		struct edma_ch_data *ch = get_ch_data_maybe(dma_chan_data);
		if (!ch)
			continue; // Skip unused channels

#define WRITE_FIELD(FIELD) \
	edma_chan_write(dma_chan_data, EDMA_TCD_##FIELD, ch->tcd_cache.FIELD)
#define WRITE_FIELD16(FIELD) \
	edma_chan_write16(dma_chan_data, EDMA_TCD_##FIELD, ch->tcd_cache.FIELD)
		WRITE_FIELD(SADDR);
		WRITE_FIELD16(SOFF);
		WRITE_FIELD16(ATTR);
		WRITE_FIELD(NBYTES);
		WRITE_FIELD(SLAST);
		WRITE_FIELD(DADDR);
		WRITE_FIELD16(DOFF);
		WRITE_FIELD16(CITER);
		WRITE_FIELD(DLAST_SGA);
		WRITE_FIELD16(BITER);
		WRITE_FIELD16(CSR);
#undef WRITE_FIELD
#undef WRITE_FIELD16
	}
	return 0;
}

/* store DMA context after leaving D3 */
static int edma_pm_context_store(struct dma *dma)
{
	struct edma_data *pdata = dma_get_drvdata(dma);

	/* disable the DMA controller */
	tracev_edma("EDMA: suspending... Let's cache the hardware DMA");

	// TODO Save HW DMA controller state
	for (int channel = 0; channel < 32; channel++) {
		struct dma_chan_data *dma_chan_data = &pdata->channels[channel];
		struct edma_ch_data *ch = get_ch_data_maybe(dma_chan_data);
		if (!ch)
			continue; // Skip unused channels

#define READ_FIELD(FIELD) \
	ch->tcd_cache.FIELD = edma_chan_read(dma_chan_data, EDMA_TCD_##FIELD)
#define READ_FIELD16(FIELD) \
	ch->tcd_cache.FIELD = edma_chan_read16(dma_chan_data, EDMA_TCD_##FIELD)
		READ_FIELD(SADDR);
		READ_FIELD16(SOFF);
		READ_FIELD16(ATTR);
		READ_FIELD(NBYTES);
		READ_FIELD(SLAST);
		READ_FIELD(DADDR);
		READ_FIELD16(DOFF);
		READ_FIELD16(CITER);
		READ_FIELD(DLAST_SGA);
		READ_FIELD16(CSR);
		READ_FIELD16(BITER);
#undef READ_FIELD
#undef READ_FIELD16
	}

	return 0;
}

static int edma_set_cb(struct dma_chan_data *channel, int type,
		void (*cb)(void *data, uint32_t type, struct dma_cb_data *next),
		void *data)
{
	/* We need to protect ourselves from interruppts while updating
	 * the callback
	 * Currently not doing it (TODO)
	 */

	tracev_edma("EDMA: set_cb");
	channel->cb = cb;
	channel->cb_type = type;
	channel->cb_data = data;
	return 0;
}

static int edma_probe(struct dma *dma)
{
	trace_edma("EDMA: probe");
	// We need to allocate the private data for this DMA structure
	struct edma_data *pdata = rzalloc(RZONE_RUNTIME,
					  SOF_MEM_CAPS_RAM, sizeof(struct edma_data));
	if (!pdata) {
		trace_edma("EDMA: can't allocate %d", sizeof(struct edma_data));
		trace_edma_error("EDMA: Probe failure, unable to allocate private data");
		heap_trace_all(0);
		trace_edma_error("EDMA probe failure, unable to allocate %d bytes", sizeof(struct edma_data));
		return -ENOMEM;
	}
	/* TODO: Replace 32 by channel data */
	pdata->channels = rzalloc(RZONE_RUNTIME, SOF_MEM_CAPS_RAM, 32 * sizeof(struct dma_chan_data));
	if (!pdata->channels) {
		rfree(pdata);
		trace_edma("EDMA: can't allocate %d", 32 * sizeof(struct dma_chan_data));
		trace_edma_error("EDMA: Probe failure, unable to allocate channel descriptors");
		heap_trace_all(0);
		trace_edma_error("EDMA probe failure, unable to allocate 32x%d bytes", sizeof(struct dma_chan_data));
	}
	dma_set_drvdata(dma, pdata);
	trace_edma("EDMA probe complete");
	// Hardware initialize: Set up the default HW settings. Enable
	// round-robin and disable everything else.
	// (Can't use the management page?!)
	//edma_write(dma, EDMA_CSR, EDMA_DEFAULT_GLOBAL_CSR);
	// The controller is usable, and multiple probes will not affect
	// it wrongly except lose state and leak memory if probed a
	// second time without a remove inbetween
	return 0;
}

static int edma_remove(struct dma *dma)
{
	struct edma_data *pdata = dma_get_drvdata(dma);

	trace_edma("EDMA: remove (I'd be surprised!)");
	// We should release all private data. Assume all channels are
	// free.
	// TODO Actually release all DMA channels' data
	// Halt the controller
	// Can't!
	//edma_write(dma, EDMA_CSR, EDMA_GLOBAL_CSR_HALT);
	// Clear all channels
	for (int channel = 0; channel < 32; channel++) {
		// Disable HW requests for this channel
		edma_chan_write(&pdata->channels[channel], EDMA_CH_CSR, 0);
		// Remove TCD from channel
		edma_chan_write16(&pdata->channels[channel], EDMA_TCD_CSR, 0);
		// Free up channel private data
		free_ch_data(&pdata->channels[channel]);
	}
	rfree(pdata);
	dma_set_drvdata(dma, NULL);
	return 0;
}

static int edma_get_attribute(struct dma *dma, uint32_t type, uint32_t *value)
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

static int edma_get_data_size(struct dma_chan_data *channel,
			      uint32_t *avail, uint32_t *free) {
	// TODO
	return 0;
}

const struct dma_ops edma_ops = {
	.channel_get	= edma_channel_get,
	.channel_put	= edma_channel_put,
	.start		= edma_start,
	.stop		= edma_stop,
	.pause		= edma_pause,
	.release	= edma_release,
	.copy		= edma_copy,
	.status		= edma_status,
	.set_config	= edma_set_config,
	.set_cb		= edma_set_cb,
	.pm_context_restore	= edma_pm_context_restore,
	.pm_context_store	= edma_pm_context_store,
	.probe		= edma_probe,
	.remove		= edma_remove,
	.get_attribute	= edma_get_attribute,
	.get_data_size	= edma_get_data_size,
};
