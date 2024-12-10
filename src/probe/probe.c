// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019-2022 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
// Author: Adrian Bonislawski <adrian.bonislawski@linux.intel.com>

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/probe/probe.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/dma.h>
#include <sof/lib/notifier.h>
#include <sof/lib/uuid.h>
#include <sof/ipc/topology.h>
#include <sof/ipc/driver.h>
#include <rtos/timer.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <rtos/task.h>
#include <rtos/string_macro.h>
#if CONFIG_IPC_MAJOR_4
#include <sof/audio/module_adapter/module/generic.h>
#include <ipc4/gateway.h>
#include <ipc4/module.h>
#include <sof/audio/component.h>
#include <sof/audio/component_ext.h>
#include <sof/ut.h>

SOF_DEFINE_REG_UUID(probe4);
#define PROBE_UUID probe4_uuid

#elif CONFIG_IPC_MAJOR_3
SOF_DEFINE_REG_UUID(probe);
#define PROBE_UUID probe_uuid

#else
#error "No or invalid IPC MAJOR version selected."
#endif /* CONFIG_IPC_MAJOR_4 */

DECLARE_TR_CTX(pr_tr, SOF_UUID(PROBE_UUID), LOG_LEVEL_INFO);

SOF_DEFINE_REG_UUID(probe_task);

LOG_MODULE_REGISTER(probe, CONFIG_SOF_LOG_LEVEL);

#define PROBE_DMA_INVALID	0xFFFFFFFF
#define PROBE_POINT_INVALID	0xFFFFFFFF

#define PROBE_BUFFER_LOCAL_SIZE	8192
#define DMA_ELEM_SIZE		32

/**
 * DMA buffer
 */
struct probe_dma_buf {
	uintptr_t w_ptr;	/**< write pointer */
	uintptr_t r_ptr;	/**< read pointer */
	uintptr_t addr;		/**< buffer start pointer */
	uintptr_t end_addr;	/**< buffer end pointer */
	uint32_t size;		/**< buffer size */
	uint32_t avail;		/**< buffer avail data */
};

/**
 * Probe DMA
 */
struct probe_dma_ext {
	uint32_t stream_tag;		/**< DMA stream tag */
	uint32_t dma_buffer_size;	/**< DMA buffer size */
	struct dma_sg_config config;	/**< DMA SG config */
	struct probe_dma_buf dmapb;	/**< DMA buffer pointer */
	struct dma_copy dc;		/**< DMA copy */
};

/**
 * Probe main struct
 */
struct probe_pdata {
	struct probe_dma_ext ext_dma;				  /**< extraction DMA */
	struct probe_dma_ext inject_dma[CONFIG_PROBE_DMA_MAX];	  /**< injection DMA */
	struct probe_point probe_points[CONFIG_PROBE_POINTS_MAX]; /**< probe points */
	struct probe_data_packet header;			  /**< data packet header */
	struct task dmap_work;					  /**< probe task */
};

/**
 * \brief Allocate and initialize probe buffer with correct alignment.
 * \param[out] buffer return value.
 * \param[in] size of buffer.
 * \param[in] align the buffer.
 * \return 0 on success, error code otherwise.
 */
static int probe_dma_buffer_init(struct probe_dma_buf *buffer, uint32_t size,
				 uint32_t align)
{
	/* allocate new buffer */
	buffer->addr = (uintptr_t)rballoc_align(0, SOF_MEM_CAPS_DMA,
						size, align);

	if (!buffer->addr) {
		tr_err(&pr_tr, "probe_dma_buffer_init(): alloc failed");
		return -ENOMEM;
	}

	bzero((void *)buffer->addr, size);
	dcache_writeback_region((__sparse_force void __sparse_cache *)buffer->addr, size);

	/* initialise the DMA buffer */
	buffer->size = size;
	buffer->w_ptr = buffer->addr;
	buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + buffer->size;
	buffer->avail = 0;

	return 0;
}

#if !CONFIG_ZEPHYR_NATIVE_DRIVERS
/**
 * \brief Request DMA and initialize DMA for probes with correct alignment,
 *	  size and specific channel.
 *
 * \param[out] dma probe returned
 * \param[in] direction of the DMA
 * \return 0 on success, error code otherwise.
 */
static int probe_dma_init(struct probe_dma_ext *dma, uint32_t direction)
{
	uint32_t elem_addr, addr_align;
	const uint32_t elem_size = sizeof(uint64_t) * DMA_ELEM_SIZE;
	const uint32_t elem_num = PROBE_BUFFER_LOCAL_SIZE / elem_size;
	uint32_t channel;
	int err = 0;

#if CONFIG_IPC_MAJOR_4
	channel = ((union ipc4_connector_node_id)dma->stream_tag).f.v_index + 1;
#else
	channel = dma->stream_tag;
#endif
	/* request DMA in the dir LMEM->HMEM with shared access */
	dma->dc.dmac = dma_get(direction, 0, SOF_DMA_DEV_HOST,
			       SOF_DMA_ACCESS_SHARED);
	if (!dma->dc.dmac) {
		tr_err(&pr_tr, "probe_dma_init(): dma->dc.dmac = NULL");
		return -ENODEV;
	}
	dma->dc.dmac->priv_data = &dma->dc.dmac->chan->index;
	/* get required address alignment for dma buffer */
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	err = dma_get_attribute(dma->dc.dmac->z_dev, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
#else
	err = dma_get_attribute_legacy(dma->dc.dmac, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				       &addr_align);
#endif
	if (err < 0)
		return err;

	/* initialize dma buffer */
	err = probe_dma_buffer_init(&dma->dmapb, PROBE_BUFFER_LOCAL_SIZE, addr_align);
	if (err < 0)
		return err;

	err = dma_copy_set_stream_tag(&dma->dc, channel);
	if (err < 0)
		return err;

	elem_addr = (uint32_t)dma->dmapb.addr;

	dma->config.direction = direction;
	dma->config.src_width = sizeof(uint32_t);
	dma->config.dest_width = sizeof(uint32_t);
	dma->config.cyclic = 0;

	err = dma_sg_alloc(&dma->config.elem_array, SOF_MEM_ZONE_RUNTIME,
			   dma->config.direction, elem_num, elem_size, elem_addr, 0);
	if (err < 0)
		return err;

	err = dma_set_config_legacy(dma->dc.chan, &dma->config);
	if (err < 0)
		return err;
	return 0;
}
#else
static int probe_dma_init(struct probe_dma_ext *dma, uint32_t direction)
{
	uint32_t addr_align;
	uint32_t channel;
	struct dma_config dma_cfg;
	struct dma_block_config dma_block_cfg;
	int err = 0;

	channel = ((union ipc4_connector_node_id)dma->stream_tag).f.v_index;

	/* request DMA in the dir LMEM->HMEM with shared access */
	dma->dc.dmac = dma_get(direction, 0, SOF_DMA_DEV_HOST,
			       SOF_DMA_ACCESS_SHARED);
	if (!dma->dc.dmac) {
		tr_err(&pr_tr, "probe_dma_init(): dma->dc.dmac = NULL");
		return -ENODEV;
	}

	/* get required address alignment for dma buffer */
	err = dma_get_attribute(dma->dc.dmac->z_dev, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (err < 0)
		return err;

	channel = dma_request_channel(dma->dc.dmac->z_dev, &channel);
	if (channel < 0) {
		tr_err(&pr_tr, "probe_dma_init(): dma_request_channel() failed");
		return -EINVAL;
	}
	dma->dc.chan = &dma->dc.dmac->chan[channel];

	/* initialize dma buffer */
	err = probe_dma_buffer_init(&dma->dmapb, PROBE_BUFFER_LOCAL_SIZE, addr_align);
	if (err < 0)
		return err;

	dma_cfg.block_count = 1;
	dma_cfg.source_data_size = sizeof(uint32_t);
	dma_cfg.dest_data_size = sizeof(uint32_t);
	dma_cfg.head_block = &dma_block_cfg;
	dma_block_cfg.block_size = (uint32_t)dma->dmapb.size;

	switch (direction) {
	case SOF_DMA_DIR_LMEM_TO_HMEM:
		dma_cfg.channel_direction = MEMORY_TO_HOST;
		dma_block_cfg.source_address = (uint32_t)dma->dmapb.addr;
		break;
	case SOF_DMA_DIR_HMEM_TO_LMEM:
		dma_cfg.channel_direction = HOST_TO_MEMORY;
		dma_block_cfg.dest_address = (uint32_t)dma->dmapb.addr;
		break;
	}

	err = dma_config(dma->dc.dmac->z_dev, dma->dc.chan->index, &dma_cfg);
	if (err < 0)
		return err;

	return 0;
}
#endif
/**
 * \brief Stop, deinit and free DMA and buffer used by probes.
 *
 * \return 0 on success, error code otherwise.
 */
static int probe_dma_deinit(struct probe_dma_ext *dma)
{
	int err = 0;
	dma_sg_free(&dma->config.elem_array);
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	err = dma_stop(dma->dc.dmac->z_dev, dma->dc.chan->index);
#else
	err = dma_stop_legacy(dma->dc.chan);
#endif
	if (err < 0) {
		tr_err(&pr_tr, "probe_dma_deinit(): dma_stop() failed");
		return err;
	}
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	dma_release_channel(dma->dc.dmac->z_dev, dma->dc.chan->index);
#else
	dma_channel_put_legacy(dma->dc.chan);
#endif
	dma_put(dma->dc.dmac);

	rfree((void *)dma->dmapb.addr);
	dma->dmapb.addr = 0;

	dma->stream_tag = PROBE_DMA_INVALID;

	return 0;
}

/*
 * \brief Probe task for extraction.
 *
 * Copy extraction probes data to host if available.
 * Return err if dma copy failed.
 */
static enum task_state probe_task(void *data)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t copy_align, avail;
	int err;

	if (!_probe->ext_dma.dmapb.avail)
		return SOF_TASK_STATE_RESCHEDULE;
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
	err = dma_get_attribute(_probe->ext_dma.dc.dmac->z_dev, DMA_ATTR_COPY_ALIGNMENT,
				&copy_align);
#else
	err = dma_get_attribute_legacy(_probe->ext_dma.dc.dmac, DMA_ATTR_COPY_ALIGNMENT,
				       &copy_align);
#endif
	if (err < 0) {
		tr_err(&pr_tr, "probe_task(): dma_get_attribute failed.");
		return SOF_TASK_STATE_COMPLETED;
	}

	avail = ALIGN_DOWN(_probe->ext_dma.dmapb.avail, copy_align);
	if (avail + _probe->ext_dma.dmapb.r_ptr >= _probe->ext_dma.dmapb.end_addr)
		avail = _probe->ext_dma.dmapb.end_addr - _probe->ext_dma.dmapb.r_ptr;

	if (avail > 0)
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
		err = dma_reload(_probe->ext_dma.dc.dmac->z_dev,
				 _probe->ext_dma.dc.chan->index, 0, 0, avail);
#else
		err = dma_copy_to_host_nowait(&_probe->ext_dma.dc,
					      &_probe->ext_dma.config, 0,
					      (void *)_probe->ext_dma.dmapb.r_ptr,
					      avail);
#endif
	else
		return SOF_TASK_STATE_RESCHEDULE;

	if (err < 0) {
		tr_err(&pr_tr, "probe_task(): dma_copy_to_host() failed.");
		return err;
	}

	/* buffer data sent, set read pointer and clear avail bytes */
	_probe->ext_dma.dmapb.r_ptr += avail;
	if (_probe->ext_dma.dmapb.r_ptr >= _probe->ext_dma.dmapb.end_addr)
		_probe->ext_dma.dmapb.r_ptr -= _probe->ext_dma.dmapb.size;
	_probe->ext_dma.dmapb.avail -= avail;

	return SOF_TASK_STATE_RESCHEDULE;
}

int probe_init(const struct probe_dma *probe_dma)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;
	int err;

	tr_dbg(&pr_tr, "probe_init()");

	if (_probe) {
		tr_err(&pr_tr, "probe_init(): Probes already initialized.");
		return -EINVAL;
	}

	/* alloc probes main struct */
	sof_get()->probe = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				   sizeof(*_probe));
	_probe = probe_get();

	if (!_probe) {
		tr_err(&pr_tr, "probe_init(): Alloc failed.");
		return -ENOMEM;
	}

	/* setup extraction dma if requested */
	if (probe_dma) {
		tr_dbg(&pr_tr, "\tstream_tag = %u, dma_buffer_size = %u",
		       probe_dma->stream_tag, probe_dma->dma_buffer_size);

		_probe->ext_dma.stream_tag = probe_dma->stream_tag;
		_probe->ext_dma.dma_buffer_size = probe_dma->dma_buffer_size;

		err = probe_dma_init(&_probe->ext_dma, SOF_DMA_DIR_LMEM_TO_HMEM);
		if (err < 0) {
			tr_err(&pr_tr, "probe_init(): probe_dma_init() failed");
			_probe->ext_dma.stream_tag = PROBE_DMA_INVALID;
			return err;
		}
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
		err = dma_start(_probe->ext_dma.dc.dmac->z_dev, _probe->ext_dma.dc.chan->index);
#else
		err = dma_start_legacy(_probe->ext_dma.dc.chan);
#endif
		if (err < 0) {
			tr_err(&pr_tr, "probe_init(): failed to start extraction dma");

			return -EBUSY;
		}
		/* init task for extraction probes */
		schedule_task_init_ll(&_probe->dmap_work,
				      SOF_UUID(probe_task_uuid),
				      SOF_SCHEDULE_LL_TIMER, SOF_TASK_PRI_LOW,
				      probe_task, _probe, 0, 0);
	} else {
		tr_dbg(&pr_tr, "\tno extraction DMA setup");

		_probe->ext_dma.stream_tag = PROBE_DMA_INVALID;
	}

	/* initialize injection DMAs as invalid */
	for (i = 0; i < CONFIG_PROBE_DMA_MAX; i++)
		_probe->inject_dma[i].stream_tag = PROBE_DMA_INVALID;

	/* initialize probe points as invalid */
	for (i = 0; i < CONFIG_PROBE_POINTS_MAX; i++)
		_probe->probe_points[i].stream_tag = PROBE_POINT_INVALID;

	return 0;
}

int probe_deinit(void)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;
	int err;

	tr_dbg(&pr_tr, "probe_deinit()");

	if (!_probe) {
		tr_err(&pr_tr, "probe_deinit(): Not initialized.");

		return -EINVAL;
	}

	/* check for attached injection probe DMAs */
	for (i = 0; i < CONFIG_PROBE_DMA_MAX; i++) {
		if (_probe->inject_dma[i].stream_tag != PROBE_DMA_INVALID) {
			tr_err(&pr_tr, "probe_deinit(): Cannot deinitialize with injection DMAs attached.");
			return -EINVAL;
		}
	}

	/* check for connected probe points */
	for (i = 0; i < CONFIG_PROBE_POINTS_MAX; i++) {
		if (_probe->probe_points[i].stream_tag != PROBE_POINT_INVALID) {
			tr_err(&pr_tr, "probe_deinit(): Cannot deinitialize with probe points connected.");
			return -EINVAL;
		}
	}

	if (_probe->ext_dma.stream_tag != PROBE_DMA_INVALID) {
		tr_dbg(&pr_tr, "probe_deinit() Freeing task and extraction DMA.");
		schedule_task_free(&_probe->dmap_work);
		err = probe_dma_deinit(&_probe->ext_dma);
		if (err < 0)
			return err;
	}

	sof_get()->probe = NULL;
	rfree(_probe);

	return 0;
}

int probe_dma_add(uint32_t count, const struct probe_dma *probe_dma)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;
	uint32_t j;
	uint32_t stream_tag;
	uint32_t first_free;
	int err;

	tr_dbg(&pr_tr, "probe_dma_add() count = %u", count);

	if (!_probe) {
		tr_err(&pr_tr, "probe_dma_add(): Not initialized.");

		return -EINVAL;
	}

	/* Iterate over all (DMA) fields if there are multiple of them */
	/* add them if there is free place and they are not already attached */
	for (i = 0; i < count; i++) {
		tr_dbg(&pr_tr, "\tprobe_dma[%u] stream_tag = %u, dma_buffer_size = %u",
		       i, probe_dma[i].stream_tag,
		       probe_dma[i].dma_buffer_size);

		first_free = CONFIG_PROBE_DMA_MAX;

		/* looking for first free dma slot */
		for (j = 0; j < CONFIG_PROBE_DMA_MAX; j++) {
			stream_tag = _probe->inject_dma[j].stream_tag;

			if (stream_tag == PROBE_DMA_INVALID) {
				if (first_free == CONFIG_PROBE_DMA_MAX)
					first_free = j;

				continue;
			}

			if (stream_tag == probe_dma[i].stream_tag) {
				tr_err(&pr_tr, "probe_dma_add(): Probe DMA %u already attached.",
				       stream_tag);
				return -EINVAL;
			}
		}

		if (first_free == CONFIG_PROBE_DMA_MAX) {
			tr_err(&pr_tr, "probe_dma_add(): Exceeded maximum number of DMAs attached = "
			       STRINGIFY(CONFIG_PROBE_DMA_MAX));
			return -EINVAL;
		}

		_probe->inject_dma[first_free].stream_tag =
			probe_dma[i].stream_tag;
		_probe->inject_dma[first_free].dma_buffer_size =
			probe_dma[i].dma_buffer_size;

		err = probe_dma_init(&_probe->inject_dma[first_free],
				     SOF_DMA_DIR_HMEM_TO_LMEM);
		if (err < 0) {
			tr_err(&pr_tr, "probe_dma_add(): probe_dma_init() failed");
			_probe->inject_dma[first_free].stream_tag =
				PROBE_DMA_INVALID;
			return err;
		}
	}

	return 0;
}

/**
 * \brief Check if stream_tag is used by probes.
 * \param[in] stream_tag DMA stream tag.
 * \return 0 if not used, 1 otherwise.
 */
static int is_probe_stream_used(uint32_t stream_tag)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;

	for (i = 0; i < CONFIG_PROBE_POINTS_MAX; i++) {
		if (_probe->probe_points[i].stream_tag == stream_tag)
			return 1;
	}

	return 0;
}

int probe_dma_remove(uint32_t count, const uint32_t *stream_tag)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;
	uint32_t j;
	int err;

	tr_dbg(&pr_tr, "probe_dma_remove() count = %u", count);

	if (!_probe) {
		tr_err(&pr_tr, "probe_dma_remove(): Not initialized.");

		return -EINVAL;
	}

	/* remove each DMA if they are not used */
	for (i = 0; i < count; i++) {
		tr_dbg(&pr_tr, "\tstream_tag[%u] = %u", i, stream_tag[i]);

		if (is_probe_stream_used(stream_tag[i]))
			return -EINVAL;

		for (j = 0; j < CONFIG_PROBE_DMA_MAX; j++) {
			if (_probe->inject_dma[j].stream_tag == stream_tag[i]) {
				err = probe_dma_deinit(&_probe->inject_dma[j]);
				if (err < 0)
					return err;
			}
		}
	}

	return 0;
}

/**
 * \brief Copy data to probe buffer and update buffer pointers.
 * \param[out] pbuf DMA buffer.
 * \param[in] data pointer.
 * \param[in] bytes size.
 * \return 0 on success, error code otherwise.
 */
static int copy_to_pbuffer(struct probe_dma_buf *pbuf, void *data,
			   uint32_t bytes)
{
	uint32_t head;
	uint32_t tail;

	if (bytes == 0)
		return 0;

	/* check if there is free room in probe buffer */
	if (pbuf->size - pbuf->avail < bytes)
		return -EINVAL;

	/* check if it will not exceed end_addr */
	if ((char *)pbuf->end_addr - (char *)pbuf->w_ptr < bytes) {
		head = (char *)pbuf->end_addr - (char *)pbuf->w_ptr;
		tail = bytes - head;
	} else {
		head = bytes;
		tail = 0;
	}

	/* copy data to probe buffer */
	if (memcpy_s((void *)pbuf->w_ptr, pbuf->end_addr - pbuf->w_ptr, data, head)) {
		tr_err(&pr_tr, "copy_to_pbuffer(): memcpy_s() failed");
		return -EINVAL;
	}
	dcache_writeback_region((__sparse_force void __sparse_cache *)pbuf->w_ptr, head);

	/* buffer ended so needs to do a second copy */
	if (tail) {
		pbuf->w_ptr = pbuf->addr;
		if (memcpy_s((void *)pbuf->w_ptr, (char *)pbuf->end_addr - (char *)pbuf->w_ptr,
			     (char *)data + head, tail)) {
			tr_err(&pr_tr, "copy_to_pbuffer(): memcpy_s() failed");
			return -EINVAL;
		}
		dcache_writeback_region((__sparse_force void __sparse_cache *)pbuf->w_ptr, tail);
		pbuf->w_ptr = pbuf->w_ptr + tail;
	} else {
		pbuf->w_ptr = pbuf->w_ptr + head;
	}

	pbuf->avail = (uintptr_t)pbuf->avail + bytes;

	return 0;
}

/**
 * \brief Copy data from probe buffer and update buffer pointers.
 * \param[out] pbuf DMA buffer.
 * \param[out] data pointer.
 * \param[in] bytes size.
 * \return 0 on success, error code otherwise.
 */
static int copy_from_pbuffer(struct probe_dma_buf *pbuf, void *data,
			     uint32_t bytes)
{
	uint32_t head;
	uint32_t tail;

	if (bytes == 0)
		return 0;
	/* not enough data available so set it to 0 */
	if (pbuf->avail < bytes) {
		memset(data, 0, bytes);
		return 0;
	}

	/* check if memcpy needs to be divided into two stages */
	if (pbuf->end_addr - pbuf->r_ptr < bytes) {
		head = pbuf->end_addr - pbuf->r_ptr;
		tail = bytes - head;
	} else {
		head = bytes;
		tail = 0;
	}

	/* data from DMA so invalidate it */
	dcache_invalidate_region((__sparse_force void __sparse_cache *)pbuf->r_ptr, head);
	if (memcpy_s(data, bytes, (void *)pbuf->r_ptr, head)) {
		tr_err(&pr_tr, "copy_from_pbuffer(): memcpy_s() failed");
		return -EINVAL;
	}

	/* second stage copy */
	if (tail) {
		/* starting from the beginning of the buffer */
		pbuf->r_ptr = pbuf->addr;
		dcache_invalidate_region((__sparse_force void __sparse_cache *)pbuf->r_ptr, tail);
		if (memcpy_s((char *)data + head, tail, (void *)pbuf->r_ptr, tail)) {
			tr_err(&pr_tr, "copy_from_pbuffer(): memcpy_s() failed");
			return -EINVAL;
		}
		pbuf->r_ptr = pbuf->r_ptr + tail;
	} else {
		pbuf->r_ptr = pbuf->r_ptr + head;
	}

	/* subtract used bytes */
	pbuf->avail -= bytes;

	return 0;
}

/**
 * \brief Generate probe data packet header, update timestamp, calc crc
 *	  and copy data to probe buffer.
 * \param[in] buffer_id component buffer id
 * \param[in] size data size.
 * \param[in] format audio format.
 * \param[out] checksum.
 * \return 0 on success, error code otherwise.
 */
static int probe_gen_header(uint32_t buffer_id, uint32_t size,
			    uint32_t format, uint64_t *checksum)
{
	struct probe_pdata *_probe = probe_get();
	struct probe_data_packet *header;
	uint64_t timestamp;

	header = &_probe->header;
	timestamp = sof_cycle_get_64();

	header->sync_word = PROBE_EXTRACT_SYNC_WORD;
	header->buffer_id = buffer_id;
	header->format = format;
	header->timestamp_low = (uint32_t)timestamp;
	header->timestamp_high = (uint32_t)(timestamp >> 32);
	header->data_size_bytes = size;

	/* calc checksum to check validation by probe parse app */
	*checksum = header->sync_word +
		    header->buffer_id  +
		    header->format +
		    header->timestamp_high +
		    header->timestamp_low +
		    header->data_size_bytes;

	dcache_writeback_region((__sparse_force void __sparse_cache *)header, sizeof(*header));

	return copy_to_pbuffer(&_probe->ext_dma.dmapb, header,
			       sizeof(struct probe_data_packet));
}

/**
 * \brief Generate description of audio format for extraction probes.
 * \param[in] frame_fmt format
 * \param[in] rate sample rate.
 * \param[in] channels number of channels
 * \return format.
 */
static uint32_t probe_gen_format(uint32_t frame_fmt, uint32_t rate,
				 uint32_t channels)
{
	uint32_t format = 0;
	uint32_t sample_rate;
	uint32_t valid_bytes;
	uint32_t container_bytes;
	uint32_t float_fmt = 0;

	switch (frame_fmt) {
	case SOF_IPC_FRAME_S16_LE:
		valid_bytes = 2;
		container_bytes = 2;
		break;
	case SOF_IPC_FRAME_S24_4LE:
		valid_bytes = 3;
		container_bytes = 4;
		break;
	case SOF_IPC_FRAME_S32_LE:
		valid_bytes = 4;
		container_bytes = 4;
		break;
	case SOF_IPC_FRAME_FLOAT:
		valid_bytes = 4;
		container_bytes = 4;
		float_fmt = 1;
		break;
	default:
		tr_err(&pr_tr, "probe_gen_format(): Invalid frame format specified = 0x%08x",
		       frame_fmt);
		return 0;
	}

	switch (rate) {
	case 8000:
		sample_rate = 0;
		break;
	case 11025:
		sample_rate = 1;
		break;
	case 12000:
		sample_rate = 2;
		break;
	case 16000:
		sample_rate = 3;
		break;
	case 22050:
		sample_rate = 4;
		break;
	case 24000:
		sample_rate = 5;
		break;
	case 32000:
		sample_rate = 6;
		break;
	case 44100:
		sample_rate = 7;
		break;
	case 48000:
		sample_rate = 8;
		break;
	case 64000:
		sample_rate = 9;
		break;
	case 88200:
		sample_rate = 10;
		break;
	case 96000:
		sample_rate = 11;
		break;
	case 128000:
		sample_rate = 12;
		break;
	case 176400:
		sample_rate = 13;
		break;
	case 192000:
		sample_rate = 14;
		break;
	default:
		sample_rate = 15;
	}

	format |= (1 << PROBE_SHIFT_FMT_TYPE) & PROBE_MASK_FMT_TYPE;
	format |= (sample_rate << PROBE_SHIFT_SAMPLE_RATE) & PROBE_MASK_SAMPLE_RATE;
	format |= ((channels - 1) << PROBE_SHIFT_NB_CHANNELS) & PROBE_MASK_NB_CHANNELS;
	format |= ((valid_bytes - 1) << PROBE_SHIFT_SAMPLE_SIZE) & PROBE_MASK_SAMPLE_SIZE;
	format |= ((container_bytes - 1) << PROBE_SHIFT_CONTAINER_SIZE) & PROBE_MASK_CONTAINER_SIZE;
	format |= (float_fmt << PROBE_SHIFT_SAMPLE_FMT) & PROBE_MASK_SAMPLE_FMT;
	format |= (1 << PROBE_SHIFT_INTERLEAVING_ST) & PROBE_MASK_INTERLEAVING_ST;

	return format;
}

/*
 * Check if more than 75% of buffer is already used,
 * and if yes, rescheduled the probe task immediately.
 */
static void kick_probe_task(struct probe_pdata *_probe)
{
	if (_probe->ext_dma.dmapb.size - _probe->ext_dma.dmapb.avail <
		_probe->ext_dma.dmapb.size >> 2)
		reschedule_task(&_probe->dmap_work, 0);
}

#if CONFIG_LOG_BACKEND_SOF_PROBE
static void probe_logging_hook(uint8_t *buffer, size_t length)
{
	struct probe_pdata *_probe = probe_get();
	uint64_t checksum;
	int ret;

	ret = probe_gen_header(PROBE_LOGGING_BUFFER_ID, length, 0, &checksum);
	if (ret < 0)
		return;

	ret = copy_to_pbuffer(&_probe->ext_dma.dmapb,
			      buffer, length);
	if (ret < 0)
		return;

	ret = copy_to_pbuffer(&_probe->ext_dma.dmapb,
			      &checksum, sizeof(checksum));
	if (ret < 0)
		return;

	kick_probe_task(_probe);
}
#endif

/**
 * \brief General extraction probe callback, called from buffer produce.
 *	  It will search for probe point connected to this buffer.
 *	  Extraction probe: generate format, header and copy data to probe buffer.
 *	  Injection probe: find corresponding DMA, check avail data, copy data,
 *	  update pointers and request more data from host if needed.
 * \param[in] arg pointer (not used).
 * \param[in] type of notify.
 * \param[in] data pointer.
 */
static void probe_cb_produce(void *arg, enum notify_id type, void *data)
{
	struct probe_pdata *_probe = probe_get();
	struct buffer_cb_transact *cb_data = data;
	struct comp_buffer *buffer = cb_data->buffer;
	struct probe_dma_ext *dma;
	uint32_t buffer_id;
	uint32_t head, tail;
	uint32_t free_bytes = 0;
	int32_t copy_bytes = 0;
	int ret;
	uint32_t i, j;
	uint32_t format;
	uint64_t checksum;

	buffer_id = *(int *)arg;

	/* search for probe point connected to this buffer */
	for (i = 0; i < CONFIG_PROBE_POINTS_MAX; i++)
		if (_probe->probe_points[i].buffer_id.full_id == buffer_id)
			break;

	if (i == CONFIG_PROBE_POINTS_MAX) {
		tr_err(&pr_tr, "probe_cb_produce(): probe not found for buffer id: %d",
		       buffer_id);
		return;
	}

	if (_probe->probe_points[i].purpose == PROBE_PURPOSE_EXTRACTION) {
		format = probe_gen_format(audio_stream_get_frm_fmt(&buffer->stream),
					  audio_stream_get_rate(&buffer->stream),
					  audio_stream_get_channels(&buffer->stream));
		ret = probe_gen_header(buffer_id,
				       cb_data->transaction_amount,
				       format, &checksum);
		if (ret < 0)
			goto err;

		/* check if transaction amount exceeds component buffer end addr */
		/* if yes: divide copying into two stages, head and tail */
		if ((char *)cb_data->transaction_begin_address + cb_data->transaction_amount >
		    (char *)audio_stream_get_end_addr(&buffer->stream)) {
			head = (uintptr_t)audio_stream_get_end_addr(&buffer->stream) -
			       (uintptr_t)cb_data->transaction_begin_address;
			tail = (uintptr_t)cb_data->transaction_amount - head;
			ret = copy_to_pbuffer(&_probe->ext_dma.dmapb,
					      cb_data->transaction_begin_address,
					      head);
			if (ret < 0)
				goto err;

			ret = copy_to_pbuffer(&_probe->ext_dma.dmapb,
					      audio_stream_get_addr(&buffer->stream), tail);
			if (ret < 0)
				goto err;
		} else {
			ret = copy_to_pbuffer(&_probe->ext_dma.dmapb,
					      cb_data->transaction_begin_address,
					      cb_data->transaction_amount);
			if (ret < 0)
				goto err;
		}

		ret = copy_to_pbuffer(&_probe->ext_dma.dmapb,
				      &checksum, sizeof(checksum));
		if (ret < 0)
			goto err;

		kick_probe_task(_probe);
	} else {
		/* search for DMA used by this probe point */
		for (j = 0; j < CONFIG_PROBE_DMA_MAX; j++) {
			if (_probe->inject_dma[j].stream_tag !=
			    PROBE_DMA_INVALID &&
			    _probe->inject_dma[j].stream_tag ==
			    _probe->probe_points[i].stream_tag) {
				break;
			}
		}
		if (j == CONFIG_PROBE_DMA_MAX) {
			tr_err(&pr_tr, "probe_cb_produce(): dma not found");
			return;
		}
		dma = &_probe->inject_dma[j];
		/* get avail data info */
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
		struct dma_status stat;

		ret = dma_get_status(dma->dc.dmac->z_dev, dma->dc.chan->index, &stat);
		dma->dmapb.avail = stat.pending_length;
		free_bytes = stat.free;
#else
		ret = dma_get_data_size_legacy(dma->dc.chan,
					       &dma->dmapb.avail,
					       &free_bytes);
#endif
		if (ret < 0) {
			tr_err(&pr_tr, "probe_cb_produce(): dma_get_data_size() failed, ret = %u",
			       ret);
			goto err;
		}

		/* check if transaction amount exceeds component buffer end addr */
		/* if yes: divide copying into two stages, head and tail */
		if ((char *)cb_data->transaction_begin_address + cb_data->transaction_amount >
		    (char *)audio_stream_get_end_addr(&buffer->stream)) {
			head = (char *)audio_stream_get_end_addr(&buffer->stream) -
				(char *)cb_data->transaction_begin_address;
			tail = cb_data->transaction_amount - head;

			ret = copy_from_pbuffer(&dma->dmapb,
						cb_data->transaction_begin_address, head);
			if (ret < 0)
				goto err;

			ret = copy_from_pbuffer(&dma->dmapb,
						audio_stream_get_addr(&buffer->stream), tail);
			if (ret < 0)
				goto err;
		} else {
			ret = copy_from_pbuffer(&dma->dmapb,
						cb_data->transaction_begin_address,
						cb_data->transaction_amount);
			if (ret < 0)
				goto err;
		}

		/* calc how many data can be requested */
		copy_bytes = dma->dmapb.r_ptr - dma->dmapb.w_ptr;
		if (copy_bytes < 0)
			copy_bytes += dma->dmapb.size;

		/* align down to request at least 32 */
		copy_bytes = ALIGN_DOWN(copy_bytes, 32);

		/* check if copy_bytes is still valid for dma copy */
		if (copy_bytes > 0) {
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
			ret = dma_reload(dma->dc.dmac->z_dev,
					 dma->dc.chan->index, 0, 0, copy_bytes);
#else
			ret = dma_copy_to_host_nowait(&dma->dc,
						      &dma->config, 0,
						      (void *)dma->dmapb.r_ptr,
						      copy_bytes);
#endif
			if (ret < 0)
				goto err;

			/* update pointers */
			dma->dmapb.w_ptr = dma->dmapb.w_ptr + copy_bytes;
			if (dma->dmapb.w_ptr > dma->dmapb.end_addr)
				dma->dmapb.w_ptr = dma->dmapb.w_ptr - dma->dmapb.size;
		}
	}
	return;
err:
	tr_err(&pr_tr, "probe_cb_produce(): failed to generate probe data");
}

/**
 * \brief Callback for buffer free, it will remove probe point.
 * \param[in] arg pointer (not used).
 * \param[in] type of notify.
 * \param[in] data pointer.
 */
static void probe_cb_free(void *arg, enum notify_id type, void *data)
{
	uint32_t buffer_id = *(int *)arg;
	int ret;

	tr_dbg(&pr_tr, "probe_cb_free() buffer_id = %u", buffer_id);

	ret = probe_point_remove(1, &buffer_id);
	if (ret < 0)
		tr_err(&pr_tr, "probe_cb_free(): probe_point_remove() failed");
}

static bool probe_purpose_needs_ext_dma(uint32_t purpose)
{
#if CONFIG_IPC_MAJOR_4
	return purpose == PROBE_PURPOSE_EXTRACTION;
#else
	return purpose == PROBE_PURPOSE_EXTRACTION || purpose == PROBE_PURPOSE_LOGGING;
#endif
}

#if CONFIG_IPC_MAJOR_4
static struct comp_buffer *ipc4_get_buffer(struct ipc_comp_dev *dev, probe_point_id_t probe_point)
{
	struct comp_buffer *buf;
	unsigned int queue_id;

	switch (probe_point.fields.type) {
	case PROBE_TYPE_INPUT:
		comp_dev_for_each_producer(dev->cd, buf) {
			queue_id = IPC4_SRC_QUEUE_ID(buf_get_id(buf));

			if (queue_id == probe_point.fields.index)
				return buf;
		}
		break;
	case PROBE_TYPE_OUTPUT:
		comp_dev_for_each_consumer(dev->cd, buf) {
			queue_id = IPC4_SINK_QUEUE_ID(buf_get_id(buf));

			if (queue_id == probe_point.fields.index)
				return buf;
		}
	}

	return NULL;
}
#endif

static bool enable_logs(const struct probe_point *probe)
{
#if CONFIG_IPC_MAJOR_4
	return probe->buffer_id.full_id == 0;
#else
	return probe->purpose == PROBE_PURPOSE_LOGGING;
#endif
}

static bool verify_purpose(uint32_t purpose)
{
#if CONFIG_IPC_MAJOR_4
	return purpose == PROBE_PURPOSE_EXTRACTION ||
	       purpose == PROBE_PURPOSE_INJECTION;
#else
	return purpose == PROBE_PURPOSE_EXTRACTION ||
	       purpose == PROBE_PURPOSE_INJECTION ||
	       purpose == PROBE_PURPOSE_LOGGING;
#endif
}

int probe_point_add(uint32_t count, const struct probe_point *probe)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;
	uint32_t j;
	uint32_t buffer_id;
	uint32_t first_free;
	uint32_t dma_found;
	uint32_t fw_logs;
	struct ipc_comp_dev *dev = NULL;
#if CONFIG_IPC_MAJOR_4
	struct comp_buffer *buf = NULL;
#endif
	tr_dbg(&pr_tr, "probe_point_add() count = %u", count);

	if (!_probe) {
		tr_err(&pr_tr, "probe_point_add(): Not initialized.");

		return -EINVAL;
	}

	/* add all probe points if they are corresponding to valid component and DMA */
	for (i = 0; i < count; i++) {
		const probe_point_id_t *buf_id = &probe[i].buffer_id;
		uint32_t stream_tag;

		tr_dbg(&pr_tr, "\tprobe[%u] buffer_id = %u, purpose = %u, stream_tag = %u",
		       i, buf_id->full_id, probe[i].purpose,
		       probe[i].stream_tag);

		if (!verify_purpose(probe[i].purpose)) {
			tr_err(&pr_tr, "probe_point_add() error: invalid purpose %d",
			       probe[i].purpose);

			return -EINVAL;
		}

		if (_probe->ext_dma.stream_tag == PROBE_DMA_INVALID &&
		    probe_purpose_needs_ext_dma(probe[i].purpose)) {
			tr_err(&pr_tr, "probe_point_add(): extraction DMA not enabled.");
			return -EINVAL;
		}

		fw_logs = enable_logs(&probe[i]);

		if (!fw_logs) {
#if CONFIG_IPC_MAJOR_4
			dev = ipc_get_comp_by_id(ipc_get(),
						 IPC4_COMP_ID(buf_id->fields.module_id,
							      buf_id->fields.instance_id));
#else
			dev = ipc_get_comp_by_id(ipc_get(), buf_id->full_id);
#endif
			/* check if buffer exists */
			if (!dev) {
				tr_err(&pr_tr, "probe_point_add(): No device with ID %u found.",
				       buf_id->full_id);

				return -EINVAL;
			}
#if CONFIG_IPC_MAJOR_4
			buf = ipc4_get_buffer(dev, *buf_id);
			if (!buf) {
				tr_err(&pr_tr, "probe_point_add(): buffer %u not found.",
				       buf_id->full_id);

				return -EINVAL;
			}
#else
			if (dev->type != COMP_TYPE_BUFFER) {
				tr_err(&pr_tr, "probe_point_add(): Device ID %u is not a buffer.",
				       buf_id->full_id);

				return -EINVAL;
			}
#endif
		}

		first_free = CONFIG_PROBE_POINTS_MAX;

		/* search for first free probe slot */
		for (j = 0; j < CONFIG_PROBE_POINTS_MAX; j++) {
			if (_probe->probe_points[j].stream_tag ==
			    PROBE_POINT_INVALID) {
				if (first_free == CONFIG_PROBE_POINTS_MAX)
					first_free = j;

				continue;
			}
			/* and check if probe is already attached */
			buffer_id = _probe->probe_points[j].buffer_id.full_id;
			if (buffer_id == buf_id->full_id) {
				if (_probe->probe_points[j].purpose ==
				    probe[i].purpose) {
					tr_err(&pr_tr, "probe_point_add(): Probe already attached to buffer %u with purpose %u",
					       buffer_id,
					       probe[i].purpose);

					return -EINVAL;
				}
			}
		}

		if (first_free == CONFIG_PROBE_POINTS_MAX) {
			tr_err(&pr_tr, "probe_point_add(): Maximum number of probe points connected aleady: "
			       STRINGIFY(CONFIG_PROBE_POINTS_MAX));

			return -EINVAL;
		}

		/* if connecting injection probe, check for associated DMA */
		if (probe[i].purpose == PROBE_PURPOSE_INJECTION) {
			dma_found = 0;

			for (j = 0; j < CONFIG_PROBE_DMA_MAX; j++) {
				if (_probe->inject_dma[j].stream_tag !=
				    PROBE_DMA_INVALID &&
				    _probe->inject_dma[j].stream_tag ==
				    probe[i].stream_tag) {
					dma_found = 1;
					break;
				}
			}

			if (!dma_found) {
				tr_err(&pr_tr, "probe_point_add(): No DMA with stream tag %u found for injection.",
				       probe[i].stream_tag);

				return -EINVAL;
			}
#if CONFIG_ZEPHYR_NATIVE_DRIVERS
			if (dma_start(_probe->inject_dma[j].dc.dmac->z_dev,
				      _probe->inject_dma[j].dc.chan->index) < 0) {
#else
			if (dma_start_legacy(_probe->inject_dma[j].dc.chan) < 0) {
#endif
				tr_err(&pr_tr, "probe_point_add(): failed to start dma");

				return -EBUSY;
			}

			stream_tag = probe[i].stream_tag;
		} else {
			/* prepare extraction DMA */
			for (j = 0; j < CONFIG_PROBE_POINTS_MAX; j++) {
				if (_probe->probe_points[j].stream_tag != PROBE_DMA_INVALID &&
				    probe_purpose_needs_ext_dma(_probe->probe_points[j].purpose))
					break;
			}

			if (j == CONFIG_PROBE_POINTS_MAX) {
				tr_dbg(&pr_tr, "probe_point_add(): start probe task");
				schedule_task(&_probe->dmap_work, 1000, 1000);
			}
			/* ignore probe stream tag for extraction probes */
			stream_tag = _probe->ext_dma.stream_tag;
		}

		/* probe point valid, save it */
		_probe->probe_points[first_free].buffer_id = *buf_id;
		_probe->probe_points[first_free].purpose = probe[i].purpose;
		_probe->probe_points[first_free].stream_tag = stream_tag;

		if (fw_logs) {
#if CONFIG_LOG_BACKEND_SOF_PROBE
			probe_logging_init(probe_logging_hook);
#else
			return -EINVAL;
#endif
		} else {
			probe_point_id_t *new_buf_id = &_probe->probe_points[first_free].buffer_id;

#if CONFIG_IPC_MAJOR_4
			notifier_register(&new_buf_id->full_id, buf, NOTIFIER_ID_BUFFER_PRODUCE,
					  &probe_cb_produce, 0);
			notifier_register(&new_buf_id->full_id, buf, NOTIFIER_ID_BUFFER_FREE,
					  &probe_cb_free, 0);
#else
			notifier_register(&new_buf_id->full_id, dev->cb, NOTIFIER_ID_BUFFER_PRODUCE,
					  &probe_cb_produce, 0);
			notifier_register(&new_buf_id->full_id, dev->cb, NOTIFIER_ID_BUFFER_FREE,
					  &probe_cb_free, 0);
#endif
		}
	}

	return 0;
}

#if CONFIG_IPC_MAJOR_3
int probe_dma_info(struct sof_ipc_probe_info_params *data, uint32_t max_size)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i = 0;
	uint32_t j = 0;

	tr_dbg(&pr_tr, "probe_dma_info()");

	if (!_probe) {
		tr_err(&pr_tr, "probe_dma_info(): Not initialized.");

		return -EINVAL;
	}

	data->rhdr.hdr.size = sizeof(*data);

	/* search all injection DMAs to send them in reply */
	while (i < CONFIG_PROBE_DMA_MAX &&
	       data->rhdr.hdr.size + sizeof(struct probe_dma) < max_size) {
		/* save it if valid */
		if (_probe->inject_dma[i].stream_tag != PROBE_DMA_INVALID) {
			data->probe_dma[j].stream_tag =
				_probe->inject_dma[i].stream_tag;
			data->probe_dma[j].dma_buffer_size =
				_probe->inject_dma[i].dma_buffer_size;
			j++;
			/* and increase reply header size */
			data->rhdr.hdr.size += sizeof(struct probe_dma);
		}

		i++;
	}

	data->num_elems = j;

	return 1;
}

int probe_point_info(struct sof_ipc_probe_info_params *data, uint32_t max_size)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i = 0;
	uint32_t j = 0;

	tr_dbg(&pr_tr, "probe_point_info()");

	if (!_probe) {
		tr_err(&pr_tr, "probe_point_info(): Not initialized.");

		return -EINVAL;
	}

	data->rhdr.hdr.size = sizeof(*data);
	/* search for all probe points to send them in reply */
	while (i < CONFIG_PROBE_POINTS_MAX &&
	       data->rhdr.hdr.size + sizeof(struct probe_point) < max_size) {
		if (_probe->probe_points[i].stream_tag != PROBE_POINT_INVALID) {
			data->probe_point[j].buffer_id =
				_probe->probe_points[i].buffer_id;
			data->probe_point[j].purpose =
				_probe->probe_points[i].purpose;
			data->probe_point[j].stream_tag =
				_probe->probe_points[i].stream_tag;
			j++;
			data->rhdr.hdr.size += sizeof(struct probe_point);
		}

		i++;
	}

	data->num_elems = j;

	return 1;
}
#endif

int probe_point_remove(uint32_t count, const uint32_t *buffer_id)
{
	struct probe_pdata *_probe = probe_get();
	struct ipc_comp_dev *dev;
	uint32_t i;
	uint32_t j;
#if CONFIG_IPC_MAJOR_4
	struct comp_buffer *buf;
#endif

	tr_dbg(&pr_tr, "probe_point_remove() count = %u", count);

	if (!_probe) {
		tr_err(&pr_tr, "probe_point_remove(): Not initialized.");
		return -EINVAL;
	}
	/* remove each requested probe point */
	for (i = 0; i < count; i++) {
		tr_dbg(&pr_tr, "\tbuffer_id[%u] = %u", i, buffer_id[i]);

		for (j = 0; j < CONFIG_PROBE_POINTS_MAX; j++) {
			probe_point_id_t *buf_id = &_probe->probe_points[j].buffer_id;

			if (_probe->probe_points[j].stream_tag != PROBE_POINT_INVALID &&
			    buf_id->full_id == buffer_id[i]) {
#if CONFIG_IPC_MAJOR_4
				dev = ipc_get_comp_by_id(ipc_get(),
							 IPC4_COMP_ID(buf_id->fields.module_id,
								      buf_id->fields.instance_id));
				if (dev) {
					buf = ipc4_get_buffer(dev, *buf_id);
					if (buf) {
						notifier_unregister(NULL, buf,
								    NOTIFIER_ID_BUFFER_PRODUCE);
						notifier_unregister(NULL, buf,
								    NOTIFIER_ID_BUFFER_FREE);
					}
				}
#else
				dev = ipc_get_comp_by_id(ipc_get(), buffer_id[i]);
				if (dev) {
					notifier_unregister(&buf_id->full_id, dev->cb,
							    NOTIFIER_ID_BUFFER_PRODUCE);
					notifier_unregister(&buf_id->full_id, dev->cb,
							    NOTIFIER_ID_BUFFER_FREE);
				}
#endif
				_probe->probe_points[j].stream_tag =
					PROBE_POINT_INVALID;
			}
		}
	}
	for (j = 0; j < CONFIG_PROBE_POINTS_MAX; j++) {
		if (_probe->probe_points[j].stream_tag != PROBE_DMA_INVALID &&
		    _probe->probe_points[j].purpose == PROBE_PURPOSE_EXTRACTION)
			break;
	}
	if (j == CONFIG_PROBE_POINTS_MAX) {
		tr_dbg(&pr_tr, "probe_point_remove(): cancel probe task");
		schedule_task_cancel(&_probe->dmap_work);
	}

	return 0;
}

#if CONFIG_IPC_MAJOR_4
static int probe_mod_init(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	struct module_data *mod_data = &mod->priv;
	const struct ipc4_probe_module_cfg *probe_cfg = mod_data->cfg.init_data;
	int ret;

	comp_info(dev, "probe_mod_init()");

	ret = probe_init(&probe_cfg->gtw_cfg);
	if (ret < 0)
		return -EINVAL;

	return 0;
}

static int probe_free(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "probe_free()");

	probe_deinit();

	return 0;
}

static int probe_set_config(struct processing_module *mod, uint32_t param_id,
			    enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			    const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			    size_t response_size)
{
	struct comp_dev *dev = mod->dev;

	comp_info(dev, "probe_set_config()");

	switch (param_id) {
	case IPC4_PROBE_MODULE_PROBE_POINTS_ADD:
		return probe_point_add(fragment_size / sizeof(struct probe_point),
				       (const struct probe_point *)fragment);
	case IPC4_PROBE_MODULE_DISCONNECT_PROBE_POINTS:
		return probe_point_remove(fragment_size / sizeof(uint32_t),
					  (const uint32_t *)fragment);
	case IPC4_PROBE_MODULE_INJECTION_DMA_ADD:
		return probe_dma_add(fragment_size / (2 * sizeof(uint32_t)),
				     (const struct probe_dma *)fragment);
	case IPC4_PROBE_MODULE_INJECTION_DMA_DETACH:
		return probe_dma_remove(fragment_size / sizeof(uint32_t),
					(const uint32_t *)fragment);
	default:
		return -EINVAL;
	}
}

static int probe_add_point_info_params(struct sof_ipc_probe_info_params *info,
				       probe_point_id_t id, int index, size_t max_size)
{
	struct probe_pdata *_probe = probe_get();
	struct probe_point pp = {
		.buffer_id = id,
		.purpose = PROBE_PURPOSE_EXTRACTION,
	};
	int i;

	if (offsetof(struct sof_ipc_probe_info_params, probe_point[index]) +
	    sizeof(pp) > max_size) {
		info->num_elems = index;
		return -ENOENT;
	}

	for (i = 0; i < ARRAY_SIZE(_probe->probe_points); i++)
		if (_probe->probe_points[i].stream_tag != PROBE_POINT_INVALID &&
		    _probe->probe_points[i].buffer_id.full_id == id.full_id)
			pp.stream_tag = _probe->probe_points[i].stream_tag;

	info->probe_point[index] = pp;
	return 0;
}

static int probe_get_available_points(struct processing_module *mod,
				      struct sof_ipc_probe_info_params *info,
				      size_t max_size)
{
	struct ipc_comp_dev *icd;
	struct list_item *clist;
	int i = 0;

	list_for_item(clist, &ipc_get()->comp_list) {
		struct comp_buffer *buf;
		probe_point_id_t id;

		icd = container_of(clist, struct ipc_comp_dev, list);
		if (icd->type != COMP_TYPE_COMPONENT)
			continue;

		id.fields.module_id = IPC4_MOD_ID(icd->id);
		id.fields.instance_id = IPC4_INST_ID(icd->id);

		id.fields.type = PROBE_TYPE_INPUT;
		comp_dev_for_each_producer(icd->cd, buf) {
			id.fields.index = IPC4_SRC_QUEUE_ID(buf_get_id(buf));
			if (probe_add_point_info_params(info, id, i, max_size))
				return 0;
			i++;
		}
		id.fields.type = PROBE_TYPE_OUTPUT;
		comp_dev_for_each_consumer(icd->cd, buf) {
			id.fields.index = IPC4_SINK_QUEUE_ID(buf_get_id(buf));
			if (probe_add_point_info_params(info, id, i, max_size))
				return 0;
			i++;
		}
	}
	info->num_elems = i;
	return 0;
}

static int probe_get_config(struct processing_module *mod,
			    uint32_t config_id, uint32_t *data_offset_size,
			    uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_probe_info_params *info =
		(struct sof_ipc_probe_info_params *)ASSUME_ALIGNED(fragment, 8);
	struct probe_pdata *_probe = probe_get();
	struct comp_dev *dev = mod->dev;
	int i, j;

	comp_dbg(dev, "config_id %u", config_id);
	switch (config_id) {
	case IPC4_PROBE_MODULE_PROBE_POINTS_ADD:
		for (i = 0, j = 0; i < ARRAY_SIZE(_probe->probe_points); i++) {
			if (_probe->probe_points[i].stream_tag == PROBE_POINT_INVALID)
				continue;
			if (offsetof(struct sof_ipc_probe_info_params, probe_point[j]) +
			    sizeof(info->probe_point[0]) > fragment_size)
				break;
			info->probe_point[j++] = _probe->probe_points[i];
		}
		info->num_elems = j;
		comp_info(dev, "%u probe points sent", j);
		break;
	case IPC4_PROBE_MODULE_AVAILABLE_PROBE_POINTS:
		probe_get_available_points(mod, info, fragment_size);
		comp_info(dev, "%u available probe points sent",
			  info->num_elems);
		break;
	default:
		comp_err(dev, "unknown config_id %u", config_id);
		return -EINVAL;
	}
	return 0;
}

static int probe_dummy_process(struct processing_module *mod,
			       struct input_stream_buffer *input_buffers, int num_input_buffers,
			       struct output_stream_buffer *output_buffers, int num_output_buffers)
{
	struct comp_dev *dev = mod->dev;

	comp_warn(dev, "probe_dummy_process() called");

	return 0;
}

static const struct module_interface probe_interface = {
	.init = probe_mod_init,
	.process_audio_stream = probe_dummy_process,
	.set_configuration = probe_set_config,
	.get_configuration = probe_get_config,
	.free = probe_free,
};

DECLARE_MODULE_ADAPTER(probe_interface, PROBE_UUID, pr_tr);
SOF_MODULE_INIT(probe, sys_comp_module_probe_interface_init);

#if CONFIG_PROBE_MODULE
/* modular: llext dynamic link */

#include <module/module/api_ver.h>
#include <module/module/llext.h>
#include <rimage/sof/user/manifest.h>

#define UUID_PROBE 0x08, 0x08, 0xAD, 0x7C, 0x10, 0xAB, 0x23, 0xCD, 0xEF, 0x45, \
		0x12, 0xAB, 0x34, 0xCD, 0x56, 0xEF,

SOF_LLEXT_MOD_ENTRY(probe, &probe_interface);

static const struct sof_man_module_manifest mod_manifest __section(".module") __used =
	SOF_LLEXT_MODULE_MANIFEST("PROBE", probe_llext_entry, 1, UUID_PROBE, 40);

SOF_LLEXT_BUILDINFO;

#endif /* CONFIG_COMP_PROBE_MODULE */

#endif /* CONFIG_IPC_MAJOR_4 */
