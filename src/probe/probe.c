// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
// Author: Adrian Bonislawski <adrian.bonislawski@linux.intel.com>

#include <config.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/probe/probe.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <sof/lib/alloc.h>
#include <sof/lib/dma.h>
#include <sof/lib/notifier.h>
#include <ipc/topology.h>
#include <sof/drivers/ipc.h>
#include <sof/drivers/timer.h>
#include <sof/schedule/ll_schedule.h>
#include <sof/schedule/schedule.h>
#include <sof/schedule/task.h>

#define trace_probe(__e, ...) \
	trace_event(TRACE_CLASS_PROBE, __e, ##__VA_ARGS__)
#define tracev_probe(__e, ...) \
	tracev_event(TRACE_CLASS_PROBE, __e, ##__VA_ARGS__)
#define trace_probe_error(__e, ...) \
	trace_error(TRACE_CLASS_PROBE, __e, ##__VA_ARGS__)

#define PROBE_DMA_INVALID	0xFFFFFFFF
#define PROBE_POINT_INVALID	0xFFFFFFFF

#define PROBE_BUFFER_LOCAL_SIZE		8192
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
 * \param[out] probe buffer.
 * \param[in] buffer size.
 * \param[in] buffer address alignment.
 * \return 0 on success, error code otherwise.
 */
static int probe_dma_buffer_init(struct probe_dma_buf *buffer, uint32_t size,
				 uint32_t align)
{
	/* allocate new buffer */
	buffer->addr = (uintptr_t)rballoc_align(SOF_MEM_ZONE_BUFFER,
						SOF_MEM_CAPS_DMA, size, align);

	if (!buffer->addr) {
		trace_probe_error("probe_dma_buffer_init() error: alloc failed");
		return -ENOMEM;
	}

	bzero((void *)buffer->addr, size);
	dcache_writeback_region((void *)buffer->addr, size);

	/* initialise the DMA buffer */
	buffer->size = size;
	buffer->w_ptr = buffer->addr;
	buffer->r_ptr = buffer->addr;
	buffer->end_addr = buffer->addr + buffer->size;
	buffer->avail = 0;

	return 0;
}

/**
 * \brief Request DMA and initialize DMA for probes with correct alignment,
 *	  size and specific channel.
 * \param[out] probe DMA.
 * \param[in] direction.
 * \return 0 on success, error code otherwise.
 */
static int probe_dma_init(struct probe_dma_ext *dma, uint32_t direction)
{
	struct dma_sg_config config;
	uint32_t elem_addr, addr_align;
	const uint32_t elem_size = sizeof(uint64_t) * DMA_ELEM_SIZE;
	const uint32_t elem_num = PROBE_BUFFER_LOCAL_SIZE / elem_size;
	int err = 0;

	/* request DMA in the dir LMEM->HMEM with shared access */
	dma->dc.dmac = dma_get(direction, 0, DMA_DEV_HOST,
			       DMA_ACCESS_SHARED);
	if (!dma->dc.dmac) {
		trace_probe_error("probe_dma_init() error: dma->dc.dmac = NULL");
		return -ENODEV;
	}

	/* get required address alignment for dma buffer */
	err = dma_get_attribute(dma->dc.dmac, DMA_ATTR_BUFFER_ADDRESS_ALIGNMENT,
				&addr_align);
	if (err < 0)
		return err;

	/* initialize dma buffer */
	err = probe_dma_buffer_init(&dma->dmapb, PROBE_BUFFER_LOCAL_SIZE, addr_align);
	if (err < 0)
		return err;

	err = dma_copy_set_stream_tag(&dma->dc, dma->stream_tag);
	if (err < 0)
		return err;

	elem_addr = (uint32_t)dma->dmapb.addr;

	config.direction = direction;
	config.src_width = sizeof(uint32_t);
	config.dest_width = sizeof(uint32_t);
	config.cyclic = 0;

	err = dma_sg_alloc(&config.elem_array, SOF_MEM_ZONE_RUNTIME,
			   config.direction, elem_num, elem_size, elem_addr, 0);
	if (err < 0)
		return err;

	err = dma_set_config(dma->dc.chan, &config);
	if (err < 0)
		return err;

	dma_sg_free(&config.elem_array);


	return 0;
}

/**
 * \brief Stop, deinit and free DMA and buffer used by probes.
 * \param[out] probe DMA.
 * \return 0 on success, error code otherwise.
 */
static int probe_dma_deinit(struct probe_dma_ext *dma)
{
	int err = 0;

	err = dma_stop(dma->dc.chan);
	if (err < 0) {
		trace_probe_error("probe_dma_deinit() error: dma_stop() failed");
		return err;
	}

	dma_channel_put(dma->dc.chan);
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
	int err;

	if (_probe->ext_dma.dmapb.avail > 0)
		err = dma_copy_to_host_nowait(&_probe->ext_dma.dc,
					      &_probe->ext_dma.config, 0,
					       (void *)_probe->ext_dma.dmapb.r_ptr,
					       _probe->ext_dma.dmapb.avail);
	else
		return SOF_TASK_STATE_RESCHEDULE;

	if (err < 0) {
		trace_probe_error("probe_task() error: dma_copy_to_host_nowait() failed.");
		return err;
	}

	/* buffer data sent, set read pointer and clear avail bytes */
	_probe->ext_dma.dmapb.r_ptr = _probe->ext_dma.dmapb.w_ptr;
	_probe->ext_dma.dmapb.avail = 0;

	return SOF_TASK_STATE_RESCHEDULE;
}

int probe_init(struct probe_dma *probe_dma)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;
	int err;

	tracev_probe("probe_init()");

	if (_probe) {
		trace_probe_error("probe_init() error: Probes already initialized.");
		return -EINVAL;
	}

	/* alloc probes main struct */
	sof_get()->probe = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
				   sizeof(*_probe));
	_probe = probe_get();

	if (!_probe) {
		trace_probe_error("probe_init() error: Alloc failed.");
		return -ENOMEM;
	}

	/* setup extraction dma if requested */
	if (probe_dma) {
		tracev_probe("\tstream_tag = %u, dma_buffer_size = %u",
			     probe_dma->stream_tag, probe_dma->dma_buffer_size);

		_probe->ext_dma.stream_tag = probe_dma->stream_tag;
		_probe->ext_dma.dma_buffer_size = probe_dma->dma_buffer_size;

		err = probe_dma_init(&_probe->ext_dma, DMA_DIR_LMEM_TO_HMEM);
		if (err < 0) {
			trace_probe_error("probe_init() error: probe_dma_init() failed");
			_probe->ext_dma.stream_tag = PROBE_DMA_INVALID;
			return err;
		}

		err = dma_start(_probe->ext_dma.dc.chan);
		if (err < 0) {
			trace_probe_error("probe_init() error: failed to start extraction dma");

			return -EBUSY;
		}
		/* init task for extraction probes */
		schedule_task_init_ll(&_probe->dmap_work, SOF_SCHEDULE_LL_TIMER,
				      SOF_TASK_PRI_LOW, probe_task, _probe, 0, 0);
	} else {
		tracev_probe("\tno extraction DMA setup");

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

	tracev_probe("probe_deinit()");

	if (!_probe) {
		trace_probe_error("probe_deinit() error: Not initialized.");

		return -EINVAL;
	}

	/* check for attached injection probe DMAs */
	for (i = 0; i < CONFIG_PROBE_DMA_MAX; i++) {
		if (_probe->inject_dma[i].stream_tag != PROBE_DMA_INVALID) {
			trace_probe_error("probe_deinit() error: Cannot deinitialize with injection DMAs attached.");
			return -EINVAL;
		}
	}

	/* check for connected probe points */
	for (i = 0; i < CONFIG_PROBE_POINTS_MAX; i++) {
		if (_probe->probe_points[i].stream_tag != PROBE_POINT_INVALID) {
			trace_probe_error("probe_deinit() error: Cannot deinitialize with probe points connected.");
			return -EINVAL;
		}
	}

	if (_probe->ext_dma.stream_tag != PROBE_DMA_INVALID) {
		tracev_probe("probe_deinit() Freeing task and extraction DMA.");
		schedule_task_free(&_probe->dmap_work);
		err = probe_dma_deinit(&_probe->ext_dma);
		if (err < 0)
			return err;
	}

	sof_get()->probe = NULL;
	rfree(_probe);

	return 0;
}

int probe_dma_add(uint32_t count, struct probe_dma *probe_dma)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;
	uint32_t j;
	uint32_t stream_tag;
	uint32_t first_free;
	int err;

	tracev_probe("probe_dma_add() count = %u", count);

	if (!_probe) {
		trace_probe_error("probe_dma_add() error: Not initialized.");

		return -EINVAL;
	}

	/* Iterate over all (DMA) fields if there are multiple of them */
	/* add them if there is free place and they are not already attached */
	for (i = 0; i < count; i++) {
		tracev_probe("\tprobe_dma[%u] stream_tag = %u, dma_buffer_size = %u",
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
				trace_probe_error("probe_dma_add() error: Probe DMA %u already attached.",
						  stream_tag);
				return -EINVAL;
			}
		}

		if (first_free == CONFIG_PROBE_DMA_MAX) {
			trace_probe_error("probe_dma_add() error: Exceeded maximum number of DMAs attached = " META_QUOTE(CONFIG_PROBE_DMA_MAX));
			return -EINVAL;
		}

		_probe->inject_dma[first_free].stream_tag =
			probe_dma[i].stream_tag;
		_probe->inject_dma[first_free].dma_buffer_size =
			probe_dma[i].dma_buffer_size;

		err = probe_dma_init(&_probe->inject_dma[first_free],
				     DMA_DIR_HMEM_TO_LMEM);
		if (err < 0) {
			trace_probe_error("probe_dma_add() error: probe_dma_init() failed");
			_probe->inject_dma[first_free].stream_tag =
				PROBE_DMA_INVALID;
			return err;
		}
	}

	return 0;
}

int probe_dma_info(struct sof_ipc_probe_info_params *data, uint32_t max_size)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i = 0;
	uint32_t j = 0;

	tracev_probe("probe_dma_info()");

	if (!_probe) {
		trace_probe_error("probe_dma_info() error: Not initialized.");

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

/**
 * \brief Check if stream_tag is used by probes.
 * \param[in] DMA stream_tag.
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

int probe_dma_remove(uint32_t count, uint32_t *stream_tag)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;
	uint32_t j;
	int err;

	tracev_probe("probe_dma_remove() count = %u", count);

	if (!_probe) {
		trace_probe_error("probe_dma_remove() error: Not initialized.");

		return -EINVAL;
	}

	/* remove each DMA if they are not used */
	for (i = 0; i < count; i++) {
		tracev_probe("\tstream_tag[%u] = %u", i, stream_tag[i]);

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
 * \param[out] probe DMA buffer.
 * \param[in] data pointer.
 * \param[in] size.
 * \return 0 on success, error code otherwise.
 */
static int copy_to_pbuffer(struct probe_dma_buf *pbuf, void *data,
			   uint32_t bytes)
{
	uint32_t head;
	uint32_t tail;

	if (bytes == 0)
		return 0;

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
		trace_probe_error("copy_to_pbuffer() error: memcpy_s() failed");
		return -EINVAL;
	}
	dcache_writeback_region((void *)pbuf->w_ptr, head);

	/* buffer ended so needs to do a second copy */
	if (tail) {
		pbuf->w_ptr = pbuf->addr;
		if (memcpy_s((void *)pbuf->w_ptr, (char *)pbuf->end_addr - (char *)pbuf->w_ptr,
			     (char *)data + head, tail)) {
			trace_probe_error("copy_to_pbuffer() error: memcpy_s() failed");
			return -EINVAL;
		}
		dcache_writeback_region((void *)pbuf->w_ptr, tail);
		pbuf->w_ptr = pbuf->w_ptr + tail;
	} else {
		pbuf->w_ptr = pbuf->w_ptr + head;
	}

	pbuf->avail = (uintptr_t)pbuf->avail + bytes;

	return 0;
}

/**
 * \brief Copy data from probe buffer and update buffer pointers.
 * \param[out] probe DMA buffer.
 * \param[out] data pointer.
 * \param[in] size.
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
	dcache_invalidate_region((void *)pbuf->r_ptr, head);
	if (memcpy_s(data, bytes, (void *)pbuf->r_ptr, head)) {
		trace_probe_error("copy_from_pbuffer() error: memcpy_s() failed");
		return -EINVAL;
	}

	/* second stage copy */
	if (tail) {
		/* starting from the beginning of the buffer */
		pbuf->r_ptr = pbuf->addr;
		dcache_invalidate_region((void *)pbuf->r_ptr, tail);
		if (memcpy_s((char *)data + head, tail, (void *)pbuf->r_ptr, tail)) {
			trace_probe_error("copy_from_pbuffer() error: memcpy_s() failed");
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
 * \param[in] component buffer pointer.
 * \param[in] data size.
 * \param[in] audio format.
 * \return 0 on success, error code otherwise.
 */
static int probe_gen_header(struct comp_buffer *buffer, uint32_t size,
			    uint32_t format)
{
	struct probe_pdata *_probe = probe_get();
	struct probe_data_packet *header;
	uint64_t timestamp;
	uint32_t crc;

	header = &_probe->header;
	timestamp = platform_timer_get(timer_get());

	header->sync_word = PROBE_EXTRACT_SYNC_WORD;
	header->buffer_id = buffer->id;
	header->format = format;
	header->timestamp_low = (uint32_t)timestamp;
	header->timestamp_high = (uint32_t)(timestamp >> 32);
	header->checksum = 0;
	header->data_size_bytes = size;

	/* calc crc to check validation by probe parse app */
	crc = crc32(0, header, sizeof(*header));
	header->checksum = crc;

	dcache_writeback_region(header, sizeof(*header));

	return copy_to_pbuffer(&_probe->ext_dma.dmapb, header,
			       sizeof(struct probe_data_packet));
}

/**
 * \brief Generate description of audio format for extraction probes.
 * \param[in] frame_fmt.
 * \param[in] sample rate.
 * \param[in] channels num.
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
		trace_probe_error("probe_gen_format() error: Invalid frame format specified = 0x%08x",
				  frame_fmt);
		assert(false);
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
	uint32_t ret, i, j;
	uint32_t format;

	buffer_id = buffer->id;

	/* search for probe point connected to this buffer */
	for (i = 0; i < CONFIG_PROBE_POINTS_MAX; i++)
		if (_probe->probe_points[i].buffer_id == buffer_id)
			break;

	if (i == CONFIG_PROBE_POINTS_MAX) {
		trace_probe_error("probe_cb_produce() error: probe not found for buffer id: %d",
				  buffer_id);
		return;
	}

	if (_probe->probe_points[i].purpose == PROBE_PURPOSE_EXTRACTION) {
		format = probe_gen_format(buffer->stream.frame_fmt,
					  buffer->stream.rate,
					  buffer->stream.channels);
		ret = probe_gen_header(buffer,
				       cb_data->transaction_amount,
				       format);
		if (ret < 0)
			goto err;

		/* check if transaction amount exceeds component buffer end addr */
		/* if yes: divide copying into two stages, head and tail */
		if ((char *)cb_data->transaction_begin_address +
		    cb_data->transaction_amount > (char *)buffer->stream.end_addr) {
			head = (uintptr_t)buffer->stream.end_addr -
			       (uintptr_t)cb_data->transaction_begin_address;
			tail = (uintptr_t)cb_data->transaction_amount - head;
			ret = copy_to_pbuffer(&_probe->ext_dma.dmapb,
					      cb_data->transaction_begin_address,
					      head);
			if (ret < 0)
				goto err;

			ret = copy_to_pbuffer(&_probe->ext_dma.dmapb,
					      buffer->stream.addr, tail);
			if (ret < 0)
				goto err;
		} else {
			ret = copy_to_pbuffer(&_probe->ext_dma.dmapb,
					      cb_data->transaction_begin_address,
					      cb_data->transaction_amount);
			if (ret < 0)
				goto err;
		}
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
			trace_probe_error("probe_cb_produce() error: dma not found");
			return;
		}
		dma = &_probe->inject_dma[j];
		/* get avail data info */
		ret = dma_get_data_size(dma->dc.chan,
					&dma->dmapb.avail,
					&free_bytes);
		if (ret < 0) {
			trace_probe_error("probe_cb_produce() error: dma_get_data_size() failed, ret = %u",
					  ret);
			goto err;
		}

		/* check if transaction amount exceeds component buffer end addr */
		/* if yes: divide copying into two stages, head and tail */
		if ((char *)cb_data->transaction_begin_address +
			cb_data->transaction_amount > (char *)cb_data->buffer->stream.end_addr) {
			head = (char *)cb_data->buffer->stream.end_addr -
				(char *)cb_data->transaction_begin_address;
			tail = cb_data->transaction_amount - head;

			ret = copy_from_pbuffer(&dma->dmapb,
						cb_data->transaction_begin_address, head);
			if (ret < 0)
				goto err;

			ret = copy_from_pbuffer(&dma->dmapb,
						cb_data->buffer->stream.addr, tail);
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
			ret = dma_copy_to_host_nowait(&dma->dc,
						      &dma->config, 0,
						      (void *)dma->dmapb.r_ptr,
						      copy_bytes);
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
	trace_probe_error("probe_cb_produce() error: failed to generate probe data");
}

/**
 * \brief Callback for buffer free, it will remove probe point.
 * \param[in] arg pointer (not used).
 * \param[in] type of notify.
 * \param[in] data pointer.
 */
static void probe_cb_free(void *arg, enum notify_id type, void *data)
{
	struct buffer_cb_free *cb_data = data;
	uint32_t buffer_id = cb_data->buffer->id;
	uint32_t ret;

	tracev_probe("probe_cb_free() buffer_id = %u", buffer_id);

	ret = probe_point_remove(1, &buffer_id);
	if (ret < 0)
		trace_probe_error("probe_cb_free() error: probe_point_remove() failed");
}

int probe_point_add(uint32_t count, struct probe_point *probe)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i;
	uint32_t j;
	uint32_t buffer_id;
	uint32_t first_free;
	uint32_t dma_found;
	struct ipc_comp_dev *dev;

	tracev_probe("probe_point_add() count = %u", count);

	if (!_probe) {
		trace_probe_error("probe_point_add() error: Not initialized.");

		return -EINVAL;
	}

	/* add all probe points if they are corresponding to valid component and DMA */
	for (i = 0; i < count; i++) {
		tracev_probe("\tprobe[%u] buffer_id = %u, purpose = %u, stream_tag = %u",
			     i, probe[i].buffer_id, probe[i].purpose,
			     probe[i].stream_tag);

		if (probe[i].purpose == PROBE_PURPOSE_EXTRACTION &&
		    _probe->ext_dma.stream_tag == PROBE_DMA_INVALID) {
			trace_probe_error("probe_point_add() error: Setting probe for extraction, while extraction DMA not enabled.");

			return -EINVAL;
		}

		/* check if buffer exists */
		dev = ipc_get_comp_by_id(ipc_get(), probe[i].buffer_id);
		if (!dev) {
			trace_probe_error("probe_point_add() error: No device with ID %u found.",
					  probe[i].buffer_id);

			return -EINVAL;
		}

		if (dev->type != COMP_TYPE_BUFFER) {
			trace_probe_error("probe_point_add() error: Device ID %u is not a buffer.",
					  probe[i].buffer_id);

			return -EINVAL;
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
			buffer_id = _probe->probe_points[j].buffer_id;
			if (buffer_id == probe[i].buffer_id) {
				if (_probe->probe_points[j].purpose ==
				    probe[i].purpose) {
					trace_probe_error("probe_point_add() error: Probe already attached to buffer %u with purpose %u",
							  buffer_id,
							  probe[i].purpose);

					return -EINVAL;
				}
			}
		}

		if (first_free == CONFIG_PROBE_POINTS_MAX) {
			trace_probe_error("probe_point_add() error: Maximum number of probe points connected aleady: " META_QUOTE(CONFIG_PROBE_POINTS_MAX));

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
				trace_probe_error("probe_point_add() error: No DMA with stream tag %u found for injection.",
						  probe[i].stream_tag);

				return -EINVAL;
			}
			if (dma_start(_probe->inject_dma[j].dc.chan) < 0) {
				trace_probe_error("probe_point_add() error: failed to start dma");

				return -EBUSY;
			}
		} else if (probe[i].purpose == PROBE_PURPOSE_EXTRACTION) {
			for (j = 0; j < CONFIG_PROBE_POINTS_MAX; j++) {
				if (_probe->probe_points[j].stream_tag != PROBE_DMA_INVALID &&
				    _probe->probe_points[j].purpose == PROBE_PURPOSE_EXTRACTION)
					break;
			}
			if (j == CONFIG_PROBE_POINTS_MAX) {
				tracev_probe("probe_point_add(): start probe task");
				schedule_task(&_probe->dmap_work, 1000, 1000);
			}
		}

		/* probe point valid, save it */
		_probe->probe_points[first_free].buffer_id = probe[i].buffer_id;
		_probe->probe_points[first_free].purpose = probe[i].purpose;
		_probe->probe_points[first_free].stream_tag =
			probe[i].stream_tag;

		notifier_register(_probe, dev->cb, NOTIFIER_ID_BUFFER_PRODUCE,
				  &probe_cb_produce);
		notifier_register(_probe, dev->cb, NOTIFIER_ID_BUFFER_FREE,
				  &probe_cb_free);
	}

	return 0;
}

int probe_point_info(struct sof_ipc_probe_info_params *data, uint32_t max_size)
{
	struct probe_pdata *_probe = probe_get();
	uint32_t i = 0;
	uint32_t j = 0;

	tracev_probe("probe_point_info()");

	if (!_probe) {
		trace_probe_error("probe_point_info() error: Not initialized.");

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

int probe_point_remove(uint32_t count, uint32_t *buffer_id)
{
	struct probe_pdata *_probe = probe_get();
	struct ipc_comp_dev *dev;
	uint32_t i;
	uint32_t j;

	tracev_probe("probe_point_remove() count = %u", count);

	if (!_probe) {
		trace_probe_error("probe_point_remove() error: Not initialized.");
		return -EINVAL;
	}
	/* remove each requested probe point */
	for (i = 0; i < count; i++) {
		tracev_probe("\tbuffer_id[%u] = %u", i, buffer_id[i]);

		for (j = 0; j < CONFIG_PROBE_POINTS_MAX; j++) {
			if (_probe->probe_points[j].stream_tag != PROBE_POINT_INVALID &&
			    _probe->probe_points[j].buffer_id == buffer_id[i]) {
				dev = ipc_get_comp_by_id(ipc_get(), buffer_id[i]);
				if (dev) {
					notifier_unregister(_probe, dev->cb,
							    NOTIFIER_ID_BUFFER_PRODUCE);
					notifier_unregister(_probe, dev->cb,
							    NOTIFIER_ID_BUFFER_FREE);
				}

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
		tracev_probe("probe_point_remove(): cancel probe task");
		schedule_task_cancel(&_probe->dmap_work);
	}

	return 0;
}
