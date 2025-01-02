/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOF_LIB_DMA_LEGACY_H__
#define __SOF_LIB_DMA_LEGACY_H__

/* DMA attributes */
#define DMA_ATTR_BUFFER_ALIGNMENT		0
#define DMA_ATTR_COPY_ALIGNMENT			1
#define DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT	2
#define DMA_ATTR_BUFFER_PERIOD_COUNT		3

/* Compatibility for definitions without SOF_ namespace */
#define DMA_DIR_MEM_TO_MEM	SOF_DMA_DIR_MEM_TO_MEM
#define DMA_DIR_HMEM_TO_LMEM	SOF_DMA_DIR_HMEM_TO_LMEM
#define DMA_DIR_LMEM_TO_HMEM	SOF_DMA_DIR_LMEM_TO_HMEM
#define DMA_DIR_MEM_TO_DEV	SOF_DMA_DIR_MEM_TO_DEV
#define DMA_DIR_DEV_TO_MEM	SOF_DMA_DIR_DEV_TO_MEM
#define DMA_DIR_DEV_TO_DEV	SOF_DMA_DIR_DEV_TO_DEV
#define DMA_COPY_BLOCKING	SOF_DMA_COPY_BLOCKING
#define DMA_COPY_ONE_SHOT	SOF_DMA_COPY_ONE_SHOT
#define DMA_DEV_HOST		SOF_DMA_DEV_HOST
#define DMA_ACCESS_EXCLUSIVE	SOF_DMA_ACCESS_EXCLUSIVE
#define DMA_ACCESS_SHARED	SOF_DMA_ACCESS_SHARED
#define DMA_CHAN_INVALID	SOF_DMA_CHAN_INVALID
#define DMA_CORE_INVALID	SOF_DMA_CORE_INVALID

/* Compatibility for drivers using legacy DMA dma_get/put() */
struct dma {
	struct dma_plat_data plat_data;
	struct k_spinlock lock;	/**< locking mechanism */
	int sref;		/**< simple ref counter, guarded by lock */
	const struct dma_ops *ops;
	atomic_t num_channels_busy; /* number of busy channels */
	struct dma_chan_data *chan; /* channels array */
	const struct device *z_dev; /* Zephyr driver */
	void *priv_data;
};

struct dma *dma_get(uint32_t dir, uint32_t caps, uint32_t dev, uint32_t flags);
void dma_put(struct dma *dma);

enum dma_cb_status {
	DMA_CB_STATUS_RELOAD = 0,
	DMA_CB_STATUS_END,
};

/* DMA interrupt commands */
enum dma_irq_cmd {
	DMA_IRQ_STATUS_GET = 0,
	DMA_IRQ_CLEAR,
	DMA_IRQ_MASK,
	DMA_IRQ_UNMASK
};

/* DMA operations */
struct dma_ops {

	struct dma_chan_data *(*channel_get)(struct dma *dma,
					     unsigned int req_channel);
	void (*channel_put)(struct dma_chan_data *channel);

	int (*start)(struct dma_chan_data *channel);
	int (*stop)(struct dma_chan_data *channel);
	int (*stop_delayed)(struct dma_chan_data *channel);
	int (*copy)(struct dma_chan_data *channel, int bytes, uint32_t flags);
	int (*pause)(struct dma_chan_data *channel);
	int (*release)(struct dma_chan_data *channel);
	int (*status)(struct dma_chan_data *channel,
		      struct dma_chan_status *status, uint8_t direction);

	int (*set_config)(struct dma_chan_data *channel,
			  struct dma_sg_config *config);

	int (*probe)(struct dma *dma);
	int (*remove)(struct dma *dma);

	int (*get_data_size)(struct dma_chan_data *channel, uint32_t *avail,
			     uint32_t *free);

	int (*get_attribute)(struct dma *dma, uint32_t type, uint32_t *value);

	int (*interrupt)(struct dma_chan_data *channel, enum dma_irq_cmd cmd);
};

/* DMA API
 * Programming flow is :-
 *
 * 1) dma_channel_get()
 * 2) notifier_register()
 * 3) dma_set_config()
 * 4) dma_start()
 *   ... DMA now running ...
 * 5) dma_stop()
 * 6) dma_stop_delayed()
 * 7) dma_channel_put()
 */

static inline struct dma_chan_data *dma_channel_get_legacy(struct dma *dma,
							   int req_channel)
{
	if (!dma || !dma->ops || !dma->ops->channel_get)
		return NULL;

	struct dma_chan_data *chan = dma->ops->channel_get(dma, req_channel);

	return chan;
}

static inline void dma_channel_put_legacy(struct dma_chan_data *channel)
{
	channel->dma->ops->channel_put(channel);
}

static inline int dma_start_legacy(struct dma_chan_data *channel)
{
	return channel->dma->ops->start(channel);
}

static inline int dma_stop_legacy(struct dma_chan_data *channel)
{
	if (channel->dma->ops->stop)
		return channel->dma->ops->stop(channel);

	return 0;
}

static inline int dma_stop_delayed_legacy(struct dma_chan_data *channel)
{
	if (channel->dma->ops->stop_delayed)
		return channel->dma->ops->stop_delayed(channel);

	return 0;
}

/** \defgroup sof_dma_copy_func static int dma_copy (struct dma_chan_data * channel, int bytes, uint32_t flags)
 *
 * This function is in a separate subgroup to solve a name clash with
 * struct dma_copy {}
 * @{
 */
static inline int dma_copy_legacy(struct dma_chan_data *channel, int bytes,
				  uint32_t flags)
{
	return channel->dma->ops->copy(channel, bytes, flags);
}
/** @} */

static inline int dma_pause_legacy(struct dma_chan_data *channel)
{
	if (channel->dma->ops->pause)
		return channel->dma->ops->pause(channel);

	return 0;
}

static inline int dma_release_legacy(struct dma_chan_data *channel)
{
	if (channel->dma->ops->release)
		return channel->dma->ops->release(channel);

	return 0;
}

static inline int dma_status_legacy(struct dma_chan_data *channel,
				    struct dma_chan_status *status, uint8_t direction)
{
	return channel->dma->ops->status(channel, status, direction);
}

static inline int dma_set_config_legacy(struct dma_chan_data *channel,
					struct dma_sg_config *config)
{
	return channel->dma->ops->set_config(channel, config);
}

static inline int dma_probe_legacy(struct dma *dma)
{
	return dma->ops->probe(dma);
}

static inline int dma_remove_legacy(struct dma *dma)
{
	return dma->ops->remove(dma);
}

static inline int dma_get_data_size_legacy(struct dma_chan_data *channel,
					   uint32_t *avail, uint32_t *free)
{
	return channel->dma->ops->get_data_size(channel, avail, free);
}

static inline int dma_get_attribute_legacy(struct dma *dma, uint32_t type,
					   uint32_t *value)
{
	return dma->ops->get_attribute(dma, type, value);
}

static inline int dma_interrupt_legacy(struct dma_chan_data *channel,
				       enum dma_irq_cmd cmd)
{
	return channel->dma->ops->interrupt(channel, cmd);
}

#define dma_set_drvdata(dma, data) \
	(dma->priv_data = data)
#define dma_get_drvdata(dma) \
	dma->priv_data
#define dma_base(dma) \
	dma->plat_data.base
#define dma_irq(dma) \
	dma->plat_data.irq
#define dma_irq_name(dma) \
	dma->plat_data.irq_name
#define dma_chan_size(dma) \
	dma->plat_data.chan_size
#define dma_chan_base(dma, chan) \
	(dma->plat_data.base + chan * dma->plat_data.chan_size)
#define dma_chan_get_data(chan)	\
	((chan)->priv_data)
#define dma_chan_set_data(chan, data) \
	((chan)->priv_data = data)

/* DMA hardware register operations */
static inline uint32_t dma_reg_read(struct dma *dma, uint32_t reg)
{
	return io_reg_read(dma_base(dma) + reg);
}

static inline uint16_t dma_reg_read16(struct dma *dma, uint32_t reg)
{
	return io_reg_read16(dma_base(dma) + reg);
}

static inline void dma_reg_write(struct dma *dma, uint32_t reg, uint32_t value)
{
	io_reg_write(dma_base(dma) + reg, value);
}

static inline void dma_reg_write16(struct dma *dma, uint32_t reg,
				   uint16_t value)
{
	io_reg_write16(dma_base(dma) + reg, value);
}

static inline void dma_reg_update_bits(struct dma *dma, uint32_t reg,
				       uint32_t mask, uint32_t value)
{
	io_reg_update_bits(dma_base(dma) + reg, mask, value);
}

static inline uint32_t dma_chan_reg_read(struct dma_chan_data *channel,
					 uint32_t reg)
{
	return io_reg_read(dma_chan_base(channel->dma, channel->index) + reg);
}

static inline uint16_t dma_chan_reg_read16(struct dma_chan_data *channel,
					   uint32_t reg)
{
	return io_reg_read16(dma_chan_base(channel->dma, channel->index) + reg);
}

static inline void dma_chan_reg_write(struct dma_chan_data *channel,
				      uint32_t reg, uint32_t value)
{
	io_reg_write(dma_chan_base(channel->dma, channel->index) + reg, value);
}

static inline void dma_chan_reg_write16(struct dma_chan_data *channel,
					uint32_t reg, uint16_t value)
{
	io_reg_write16(dma_chan_base(channel->dma, channel->index) + reg,
		       value);
}

static inline void dma_chan_reg_update_bits(struct dma_chan_data *channel,
					    uint32_t reg, uint32_t mask,
					    uint32_t value)
{
	io_reg_update_bits(dma_chan_base(channel->dma, channel->index) + reg,
			   mask, value);
}

static inline void dma_chan_reg_update_bits16(struct dma_chan_data *channel,
					      uint32_t reg, uint16_t mask,
					      uint16_t value)
{
	io_reg_update_bits16(dma_chan_base(channel->dma, channel->index) + reg,
			     mask, value);
}

/* init dma copy context */
int dma_copy_new(struct dma_copy *dc);

/* free dma copy context resources */
static inline void dma_copy_free(struct dma_copy *dc)
{
	dma_channel_put_legacy(dc->chan);
}

/* DMA copy data from host to DSP */
int dma_copy_from_host(struct dma_copy *dc, struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);
int dma_copy_from_host_nowait(struct dma_copy *dc,
			      struct dma_sg_config *host_sg,
			      int32_t host_offset, void *local_ptr,
			      int32_t size);

/* DMA copy data from DSP to host */
int dma_copy_to_host(struct dma_copy *dc, struct dma_sg_config *host_sg,
		     int32_t host_offset, void *local_ptr, int32_t size);
int dma_copy_to_host_nowait(struct dma_copy *dc, struct dma_sg_config *host_sg,
			    int32_t host_offset, void *local_ptr, int32_t size);


int dma_copy_set_stream_tag(struct dma_copy *dc, uint32_t stream_tag);

#endif /* __SOF_LIB_DMA_LEGACY_H__ */
