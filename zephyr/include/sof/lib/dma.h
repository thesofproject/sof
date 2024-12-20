/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2016,2024 Intel Corporation.
 */

#ifndef __SOF_LIB_DMA_H__
#define __SOF_LIB_DMA_H__

#if defined(CONFIG_SCHEDULE_DMA_MULTI_CHANNEL) || \
	defined(CONFIG_SCHEDULE_DMA_SINGLE_CHANNEL)
/*
 * The platform/lib/dma.h definitions are only needed
 * when using old dma_{single,multi}_chan_domain
 * implementations. For new SOF build targets, it is
 * recommended to use CONFIG_DMA_DOMAIN instead if
 * DMA-driven scheduling is needed.
 */
#include <platform/lib/dma.h>
#endif

#include <rtos/atomic.h>
#include <rtos/bit.h>
#include <rtos/alloc.h>
#include <sof/lib/io.h>
#include <sof/lib/memory.h>
#include <rtos/sof.h>
#include <rtos/spinlock.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/dma.h>

struct comp_buffer;

/** \addtogroup sof_dma_drivers DMA Drivers
 *  SOF DMA Drivers API specification (deprecated interface, to be
 *  replaced by Zephyr zephyr/drivers/dma.h interface).
 *  @{
 */

/*
 * There is significant overlap with SOF DMA interface and
 * Zephyr zephyr/drivers/dma.h. Neither uses a unique namespace
 * prefix, leading to at times confusing mix of DMA_ and dma_
 * definitions, some coming from legacy SOF definitions, while
 * some from Zephyr.
 *
 * Definitions in this file are used by following SOF code:
 *  - Some use of SOF DMA data types and definitions exists in
 *    generic application code (in IPC, host handling and a few other
 *    places). To support both XTOS and Zephyr builds, these
 *    definitions must be provided by the RTOS layer.
 *  - SOF builds where Zephyr is used but platform has not yet
 *    moved to native Zephyr drivers, so legacy/XTOS DMA are used.
 *  - Linking DMA resources to audio usages. Even SOF builds with
 *    native Zephyr build still use some SOF side DMA definitions
 *    to describe system DMA resources in terms of their capabilities
 *    for audio use. See sof/zephyr/lib/dma.c for most of this logic.
 */

/* DMA direction bitmasks used to define DMA copy direction */
#define SOF_DMA_DIR_MEM_TO_MEM		BIT(0) /**< local memory copy */
#define SOF_DMA_DIR_HMEM_TO_LMEM	BIT(1) /**< host memory to local mem copy */
#define SOF_DMA_DIR_LMEM_TO_HMEM	BIT(2) /**< local mem to host mem copy */
#define SOF_DMA_DIR_MEM_TO_DEV		BIT(3) /**< local mem to dev copy */
#define SOF_DMA_DIR_DEV_TO_MEM		BIT(4) /**< dev to local mem copy */
#define SOF_DMA_DIR_DEV_TO_DEV		BIT(5) /**< dev to dev copy */

/* DMA capabilities bitmasks used to define the type of DMA */
#define SOF_DMA_CAP_HDA	BIT(0) /**< HDA DMA */
#define SOF_DMA_CAP_GP_LP	BIT(1) /**< GP LP DMA */
#define SOF_DMA_CAP_GP_HP	BIT(2) /**< GP HP DMA */
#define SOF_DMA_CAP_BT		BIT(3) /**< BT DMA */
#define SOF_DMA_CAP_SP		BIT(4) /**< SP DMA */
#define SOF_DMA_CAP_DMIC	BIT(5) /**< ACP DMA DMIC > */
#define SOF_DMA_CAP_SP_VIRTUAL	BIT(6) /**< SP VIRTUAL DMA */
#define SOF_DMA_CAP_HS_VIRTUAL	BIT(7) /**< HS VIRTUAL DMA */
#define SOF_DMA_CAP_HS		BIT(8) /**< HS DMA */
#define SOF_DMA_CAP_SW		BIT(9) /**< SW DMA */

/* DMA dev type bitmasks used to define the type of DMA */

#define SOF_DMA_DEV_HOST	BIT(0) /**< connectable to host */
#define SOF_DMA_DEV_HDA	BIT(1) /**< connectable to HD/A link */
#define SOF_DMA_DEV_SSP	BIT(2) /**< connectable to SSP fifo */
#define SOF_DMA_DEV_DMIC	BIT(3) /**< connectable to DMIC fifo */
#define SOF_DMA_DEV_SSI	BIT(4) /**< connectable to SSI / SPI fifo */
#define SOF_DMA_DEV_ALH	BIT(5) /**< connectable to ALH link */
#define SOF_DMA_DEV_SAI	BIT(6) /**< connectable to SAI fifo */
#define SOF_DMA_DEV_ESAI	BIT(7) /**< connectable to ESAI fifo */
#define SOF_DMA_DEV_BT		BIT(8) /**< connectable to ACP BT I2S */
#define SOF_DMA_DEV_SP		BIT(9) /**< connectable to ACP SP I2S */
#define SOF_DMA_DEV_AFE_MEMIF	BIT(10) /**< connectable to AFE fifo */
#define SOF_DMA_DEV_SP_VIRTUAL	BIT(11) /**< connectable to ACP SP VIRTUAL I2S */
#define SOF_DMA_DEV_HS_VIRTUAL	BIT(12) /**< connectable to ACP HS VIRTUAL I2S */
#define SOF_DMA_DEV_HS		BIT(13) /**< connectable to ACP HS I2S */
#define SOF_DMA_DEV_MICFIL	BIT(14) /**< connectable to MICFIL fifo */
#define SOF_DMA_DEV_SW		BIT(15) /**< connectable to ACP SW */

/* DMA access privilege flag */
#define SOF_DMA_ACCESS_EXCLUSIVE	1
#define SOF_DMA_ACCESS_SHARED	0

/* DMA copy flags */
#define SOF_DMA_COPY_BLOCKING	BIT(0)
#define SOF_DMA_COPY_ONE_SHOT	BIT(1)

/* We will use this enum in cb handler to inform dma what
 * action we need to perform.
 */
enum sof_dma_cb_status {
	SOF_DMA_CB_STATUS_RELOAD = 0,
	SOF_DMA_CB_STATUS_END,
};

#define SOF_DMA_CHAN_INVALID	0xFFFFFFFF
#define SOF_DMA_CORE_INVALID	0xFFFFFFFF

/* Attributes have been ported to Zephyr. This condition is necessary until full support of
 * CONFIG_SOF_ZEPHYR_STRICT_HEADERS.
 */
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
	enum sof_dma_cb_status status;
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

	/* dma position info for ipc4 */
	void *ipc_posn_data;
};

struct dma_ops;
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
	uint32_t period_count;
};

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

	/* device specific data set by the device that requests the DMA channel */
	void *dev_data;

	void *priv_data;
};

struct dma_info {
	struct dma *dma_array;
	size_t num_dmas;
};

/* generic DMA DSP <-> Host copier */
struct dma_copy {
	struct dma_chan_data *chan;
	struct dma *dmac;
};

struct audio_stream;
typedef int (*dma_process_func)(const struct audio_stream *source,
				uint32_t ioffset, struct audio_stream *sink,
				uint32_t ooffset, uint32_t source_samples, uint32_t chmap);

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
struct dma *sof_dma_get(uint32_t dir, uint32_t caps, uint32_t dev, uint32_t flags);

/**
 * \brief API to release a platform DMAC.
 *
 * @param[in] dma DMAC to relese.
 */
void sof_dma_put(struct dma *dma);

#ifndef CONFIG_ZEPHYR_NATIVE_DRIVERS
#include "dma-legacy.h"
#endif /* !CONFIG_ZEPHYR_NATIVE_DRIVERS */

#if defined(CONFIG_SCHEDULE_DMA_MULTI_CHANNEL) || \
	defined(CONFIG_SCHEDULE_DMA_SINGLE_CHANNEL)

static inline bool dma_is_scheduling_source(struct dma_chan_data *channel)
{
	return channel->is_scheduling_source;
}
#endif

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

/* copies data from DMA buffer using provided processing function */
int dma_buffer_copy_from(struct comp_buffer *source,
			 struct comp_buffer *sink,
			 dma_process_func process, uint32_t source_bytes, uint32_t chmap);

/*
 * Used when copying stream audio into multiple sink buffers, one at a time using the provided
 * conversion function. DMA buffer consume should be performed after the data has been copied
 * to all sinks.
 */
int stream_copy_from_no_consume(struct comp_buffer *source,
				struct comp_buffer *sink,
				dma_process_func process,
				uint32_t source_bytes, uint32_t chmap);

/* copies data to DMA buffer using provided processing function */
int dma_buffer_copy_to(struct comp_buffer *source,
		       struct comp_buffer *sink,
		       dma_process_func process, uint32_t sink_bytes, uint32_t chmap);


static inline const struct dma_info *dma_info_get(void)
{
	return sof_get()->dma_info;
}

/** @}*/

#endif /* __SOF_LIB_DMA_H__ */
