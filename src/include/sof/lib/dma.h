/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/sof/lib/dma.h
 * \brief DMA Drivers definition
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_LIB_DMA_H__
#define __SOF_LIB_DMA_H__

#include <platform/lib/dma.h>
#include <sof/atomic.h>
#include <sof/bit.h>
#include <sof/lib/alloc.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <sof/sof.h>
#include <sof/spinlock.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct comp_buffer;

/** \addtogroup sof_dma_drivers DMA Drivers
 *  DMA Drivers API specification.
 *  @{
 */

/* DMA direction bitmasks used to define DMA copy direction */
#define DMA_DIR_MEM_TO_MEM	BIT(0) /**< local memory copy */
#define DMA_DIR_HMEM_TO_LMEM	BIT(1) /**< host memory to local mem copy */
#define DMA_DIR_LMEM_TO_HMEM	BIT(2) /**< local mem to host mem copy */
#define DMA_DIR_MEM_TO_DEV	BIT(3) /**< local mem to dev copy */
#define DMA_DIR_DEV_TO_MEM	BIT(4) /**< dev to local mem copy */
#define DMA_DIR_DEV_TO_DEV	BIT(5) /**< dev to dev copy */

/* DMA capabilities bitmasks used to define the type of DMA */
#define DMA_CAP_HDA		BIT(0) /**< HDA DMA */
#define DMA_CAP_GP_LP		BIT(1) /**< GP LP DMA */
#define DMA_CAP_GP_HP		BIT(2) /**< GP HP DMA */

/* DMA dev type bitmasks used to define the type of DMA */

#define DMA_DEV_HOST		BIT(0) /**< connectable to host */
#define DMA_DEV_HDA		BIT(1) /**< connectable to HD/A link */
#define DMA_DEV_SSP		BIT(2) /**< connectable to SSP fifo */
#define DMA_DEV_DMIC		BIT(3) /**< connectable to DMIC fifo */
#define DMA_DEV_SSI		BIT(4) /**< connectable to SSI / SPI fifo */
#define DMA_DEV_ALH		BIT(5) /**< connectable to ALH link */
#define DMA_DEV_SAI		BIT(6) /**< connectable to SAI fifo */
#define DMA_DEV_ESAI		BIT(7) /**< connectable to ESAI fifo */

/* DMA access privilege flag */
#define DMA_ACCESS_EXCLUSIVE	1
#define DMA_ACCESS_SHARED	0

/* DMA copy flags */
#define DMA_COPY_BLOCKING	BIT(0)
#define DMA_COPY_ONE_SHOT	BIT(1)

/* We will use this enum in cb handler to inform dma what
 * action we need to perform.
 */
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

#define DMA_CHAN_INVALID	0xFFFFFFFF
#define DMA_CORE_INVALID	0xFFFFFFFF

/* DMA attributes */
#define DMA_ATTR_BUFFER_ALIGNMENT		0
#define DMA_ATTR_COPY_ALIGNMENT			1
#define DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT	2
#define DMA_ATTR_BUFFER_PERIOD_COUNT		3

struct dma;

/**
 *  \brief Element of SG list (as array item).
 */
struct dma_sg_elem {
	uint32_t src;	/**< source address */
	uint32_t dest;	/**< destination address */
	uint32_t size;	/**< size (in bytes) */
};

/**
 *  \brief Data used in DMA callbacks.
 */
struct dma_cb_data {
	struct dma_chan_data *channel;
	struct dma_sg_elem elem;
	enum dma_cb_status status;
};

/**
 * \brief SG elem array.
 */
struct dma_sg_elem_array {
	uint32_t count;			/**< number of elements in elems */
	struct dma_sg_elem *elems;	/**< elements */
};

/* DMA physical SG params */
struct dma_sg_config {
	uint32_t src_width;			/* in bytes */
	uint32_t dest_width;			/* in bytes */
	uint32_t burst_elems;
	uint32_t direction;
	uint32_t src_dev;
	uint32_t dest_dev;
	uint32_t cyclic;			/* circular buffer */
	uint64_t period;
	struct dma_sg_elem_array elem_array;	/* array of dma_sg elems */
	bool scatter;
	bool irq_disabled;
	/* true if configured DMA channel is the scheduling source */
	bool is_scheduling_source;
};

struct dma_chan_status {
	uint32_t state;
	uint32_t flags;
	uint32_t w_pos;
	uint32_t r_pos;
	uint32_t timestamp;
};

/* DMA operations */
struct dma_ops {

	struct dma_chan_data *(*channel_get)(struct dma *dma,
					     unsigned int req_channel);
	void (*channel_put)(struct dma_chan_data *channel);

	int (*start)(struct dma_chan_data *channel);
	int (*stop)(struct dma_chan_data *channel);
	int (*copy)(struct dma_chan_data *channel, int bytes, uint32_t flags);
	int (*pause)(struct dma_chan_data *channel);
	int (*release)(struct dma_chan_data *channel);
	int (*status)(struct dma_chan_data *channel,
		      struct dma_chan_status *status, uint8_t direction);

	int (*set_config)(struct dma_chan_data *channel,
			  struct dma_sg_config *config);

	int (*pm_context_restore)(struct dma *dma);
	int (*pm_context_store)(struct dma *dma);

	int (*probe)(struct dma *dma);
	int (*remove)(struct dma *dma);

	int (*get_data_size)(struct dma_chan_data *channel, uint32_t *avail,
			     uint32_t *free);

	int (*get_attribute)(struct dma *dma, uint32_t type, uint32_t *value);

	int (*interrupt)(struct dma_chan_data *channel, enum dma_irq_cmd cmd);
};

/* DMA platform data */
struct dma_plat_data {
	uint32_t id;
	uint32_t dir; /* bitmask of supported copy directions */
	uint32_t caps; /* bitmask of supported capabilities */
	uint32_t devs; /* bitmask of supported devs */
	uint32_t base;
	uint32_t channels;
	int irq;
	const char *irq_name;
	uint32_t chan_size;
	const void *drv_plat_data;
};

struct dma {
	struct dma_plat_data plat_data;
	spinlock_t lock;	/**< locking mechanism */
	int sref;		/**< simple ref counter, guarded by lock */
	const struct dma_ops *ops;
	atomic_t num_channels_busy; /* number of busy channels */
	struct dma_chan_data *chan; /* channels array */
	void *priv_data;
};

struct dma_chan_data {
	struct dma *dma;

	uint32_t status;
	uint32_t direction;
	uint32_t desc_count;
	uint32_t index;
	uint32_t core;
	uint64_t period;	/* DMA channel's transfer period in us */
	/* true if this DMA channel is the scheduling source */
	bool is_scheduling_source;

	void *priv_data;
};

struct dma_info {
	struct dma *dma_array;
	size_t num_dmas;
};

struct audio_stream;
typedef int (*dma_process_func)(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t frames);

/**
 * \brief API to initialize a platform DMA controllers.
 *
 * \param[in] sof Pointer to firmware main context.
 */
int dmac_init(struct sof *sof);

/**
 * \brief API to request a platform DMAC.
 *
 * Users can request DMAC based on dev type, copy direction, capabilities
 * and access privilege.
 * For exclusive access, ret DMAC with no channels draining.
 * For shared access, ret DMAC with the least number of channels draining.
 */
struct dma *dma_get(uint32_t dir, uint32_t caps, uint32_t dev, uint32_t flags);

/**
 * \brief API to release a platform DMAC.
 *
 * @param[in] dma DMAC to relese.
 */
void dma_put(struct dma *dma);

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

/* DMA API
 * Programming flow is :-
 *
 * 1) dma_channel_get()
 * 2) notifier_register()
 * 3) dma_set_config()
 * 4) dma_start()
 *   ... DMA now running ...
 * 5) dma_stop()
 * 6) dma_channel_put()
 */

static inline struct dma_chan_data *dma_channel_get(struct dma *dma,
						    int req_channel)
{
	struct dma_chan_data *chan = dma->ops->channel_get(dma, req_channel);

	return chan;
}

static inline void dma_channel_put(struct dma_chan_data *channel)
{
	channel->dma->ops->channel_put(channel);

}

static inline int dma_start(struct dma_chan_data *channel)
{
	int ret = channel->dma->ops->start(channel);

	return ret;
}

static inline int dma_stop(struct dma_chan_data *channel)
{
	int ret = channel->dma->ops->stop(channel);

	return ret;
}

/** \defgroup sof_dma_copy_func static int dma_copy (struct dma_chan_data * channel, int bytes, uint32_t flags)
 *
 * This function is in a separate subgroup to solve a name clash with
 * struct dma_copy {}
 * @{
 */
static inline int dma_copy(struct dma_chan_data *channel, int bytes,
			   uint32_t flags)
{
	int ret = channel->dma->ops->copy(channel, bytes, flags);

	return ret;
}
/** @} */

static inline int dma_pause(struct dma_chan_data *channel)
{
	int ret = channel->dma->ops->pause(channel);

	return ret;
}

static inline int dma_release(struct dma_chan_data *channel)
{
	int ret = channel->dma->ops->release(channel);

	return ret;
}

static inline int dma_status(struct dma_chan_data *channel,
			     struct dma_chan_status *status, uint8_t direction)
{
	int ret = channel->dma->ops->status(channel, status, direction);

	return ret;
}

static inline int dma_set_config(struct dma_chan_data *channel,
				 struct dma_sg_config *config)
{
	int ret = channel->dma->ops->set_config(channel, config);

	return ret;
}

static inline int dma_pm_context_restore(struct dma *dma)
{
	int ret = dma->ops->pm_context_restore(dma);

	return ret;
}

static inline int dma_pm_context_store(struct dma *dma)
{
	int ret = dma->ops->pm_context_store(dma);

	return ret;
}

static inline int dma_probe(struct dma *dma)
{
	int ret = dma->ops->probe(dma);

	return ret;
}

static inline int dma_remove(struct dma *dma)
{
	int ret = dma->ops->remove(dma);

	return ret;
}

static inline int dma_get_data_size(struct dma_chan_data *channel,
				    uint32_t *avail, uint32_t *free)
{
	int ret = channel->dma->ops->get_data_size(channel, avail, free);

	return ret;
}

static inline int dma_get_attribute(struct dma *dma, uint32_t type,
				    uint32_t *value)
{
	int ret = dma->ops->get_attribute(dma, type, value);

	return ret;
}

static inline int dma_interrupt(struct dma_chan_data *channel,
				enum dma_irq_cmd cmd)
{
	int ret = channel->dma->ops->interrupt(channel, cmd);

	return ret;
}

/* DMA hardware register operations */
static inline uint32_t dma_reg_read(struct dma *dma, uint32_t reg)
{
	uint32_t val;

	val = io_reg_read(dma_base(dma) + reg);

	return val;
}

static inline uint16_t dma_reg_read16(struct dma *dma, uint32_t reg)
{
	uint16_t val;

	val = io_reg_read16(dma_base(dma) + reg);

	return val;
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
	uint32_t val;

	val = io_reg_read(dma_chan_base(channel->dma, channel->index) + reg);

	return val;
}

static inline uint16_t dma_chan_reg_read16(struct dma_chan_data *channel,
					   uint32_t reg)
{
	uint16_t val;

	val = io_reg_read16(dma_chan_base(channel->dma, channel->index) + reg);

	return val;
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

static inline bool dma_is_scheduling_source(struct dma_chan_data *channel)
{
	bool ret = channel->is_scheduling_source;

	return ret;
}

static inline void dma_sg_init(struct dma_sg_elem_array *ea)
{
	ea->count = 0;
	ea->elems = NULL;
}

int dma_sg_alloc(struct dma_sg_elem_array *ea,
		 enum mem_zone zone,
		 uint32_t direction,
		 uint32_t buffer_count, uint32_t buffer_bytes,
		 uintptr_t dma_buffer_addr, uintptr_t external_addr);

void dma_sg_free(struct dma_sg_elem_array *ea);

/**
 * \brief Get the total size of SG buffer
 *
 * \param ea Array of SG elements.
 * \return Size of the buffer.
 */
static inline uint32_t dma_sg_get_size(struct dma_sg_elem_array *ea)
{
	int i;
	uint32_t size = 0;

	for (i = 0 ; i < ea->count; i++)
		size += ea->elems[i].size;

	return size;
}

struct audio_stream;
typedef void (*dma_process)(const struct audio_stream *,
			    struct audio_stream *, uint32_t);

/* copies data from DMA buffer using provided processing function */
int dma_buffer_copy_from(struct comp_buffer *source, struct comp_buffer *sink,
			 dma_process_func process, uint32_t source_bytes);

/* copies data to DMA buffer using provided processing function */
int dma_buffer_copy_to(struct comp_buffer *source, struct comp_buffer *sink,
		       dma_process_func process, uint32_t sink_bytes);

/* generic DMA DSP <-> Host copier */

struct dma_copy {
	struct dma_chan_data *chan;
	struct dma *dmac;
};

/* init dma copy context */
int dma_copy_new(struct dma_copy *dc);

/* free dma copy context resources */
static inline void dma_copy_free(struct dma_copy *dc)
{
	dma_channel_put(dc->chan);
}

/* DMA copy data from host to DSP */
int dma_copy_from_host(struct dma_copy *dc, struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);
int dma_copy_from_host_nowait(struct dma_copy *dc,
			      struct dma_sg_config *host_sg,
			      int32_t host_offset, void *local_ptr,
			      int32_t size);

/* DMA copy data from DSP to host */
int dma_copy_to_host_nowait(struct dma_copy *dc, struct dma_sg_config *host_sg,
	int32_t host_offset, void *local_ptr, int32_t size);

int dma_copy_set_stream_tag(struct dma_copy *dc, uint32_t stream_tag);

static inline const struct dma_info *dma_info_get(void)
{
	return sof_get()->dma_info;
}

/** @}*/

#endif /* __SOF_LIB_DMA_H__ */
