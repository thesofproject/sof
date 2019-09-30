// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2019 Intel Corporation. All rights reserved.
//
// Author: Artur Kloniecki <arturx.kloniecki@linux.intel.com>
// Author: Adrian Bonislawski <adrian.bonislawski@linux.intel.com>

#include <config.h>
#include <sof/probe/probe.h>
#include <sof/trace/trace.h>
#include <user/trace.h>
#include <sof/lib/alloc.h>
#include <sof/lib/dma.h>
#include <ipc/topology.h>
#include <sof/drivers/ipc.h>

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
};

static struct probe_pdata *_probe;

/**
 * \brief Allocate and initialize probe buffer with correct alignment.
 * \param[out] probe buffer.
 * \param[in] buffer size.
 * \param[in] buffer address alignment.
 * \return 0 on success, error code otherwise.
 */
static int probe_dma_buffer_init(struct probe_dma_buf *buffer, uint32_t size, uint32_t align)
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
 * \return 0 on success, error code otherwise.
 */
static int probe_dma_init(struct probe_dma_ext *dma)
{
	struct dma_sg_config config;
	uint32_t elem_addr, addr_align;
	const uint32_t elem_size = sizeof(uint64_t) * DMA_ELEM_SIZE;
	const uint32_t elem_num = PROBE_BUFFER_LOCAL_SIZE / elem_size;
	int err = 0;

	/* request DMA in the dir LMEM->HMEM with shared access */
	dma->dc.dmac = dma_get(DMA_DIR_LMEM_TO_HMEM, 0, DMA_DEV_HOST,
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

	config.direction = DMA_DIR_LMEM_TO_HMEM;
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

	err = dma_start(dma->dc.chan);
	if (err < 0)
		return err;

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

	rfree((void *)dma->dmapb.addr);
	dma->dmapb.addr = 0;

	dma->stream_tag = PROBE_DMA_INVALID;

	return 0;
}

int probe_init(struct probe_dma *probe_dma)
{
	uint32_t i;
	int err;

	tracev_probe("probe_init()");

	if (_probe) {
		trace_probe_error("probe_init() error: Probes already initialized.");
		return -EINVAL;
	}

	/* alloc probes main struct */
	_probe = rzalloc(SOF_MEM_ZONE_SYS_RUNTIME, 0, SOF_MEM_CAPS_RAM,
			 sizeof(*_probe));

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

		err = probe_dma_init(&_probe->ext_dma);
		if (err < 0) {
			trace_probe_error("probe_init() error: probe_dma_init() failed");
			_probe->ext_dma.stream_tag = PROBE_DMA_INVALID;
			return err;
		}
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
		tracev_probe("probe_deinit() Freeing extraction DMA.");
		err = probe_dma_deinit(&_probe->ext_dma);
		if (err < 0)
			return err;
	}

	rfree(_probe);
	_probe = NULL;

	return 0;
}

int probe_dma_add(uint32_t count, struct probe_dma *probe_dma)
{
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

		err = probe_dma_init(&_probe->inject_dma[first_free]);
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

	return 1;
}

/**
 * \brief Check if stream_tag is used by probes.
 * \param[in] DMA stream_tag.
 * \return 0 if not used, 1 otherwise.
 */
static int is_probe_stream_used(uint32_t stream_tag)
{
	uint32_t i;

	for (i = 0; i < CONFIG_PROBE_POINTS_MAX; i++) {
		if (_probe->probe_points[i].stream_tag == stream_tag)
			return 1;
	}

	return 0;
}

int probe_dma_remove(uint32_t count, uint32_t *stream_tag)
{
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

int probe_point_add(uint32_t count, struct probe_point *probe)
{
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
		}

		/* probe point valid, save it */
		_probe->probe_points[first_free].buffer_id = probe[i].buffer_id;
		_probe->probe_points[first_free].purpose = probe[i].purpose;
		_probe->probe_points[first_free].stream_tag =
			probe[i].stream_tag;

		/* TODO: Hook up callbacks to buffer (dev->cb) and DMA */
	}

	return 0;
}

int probe_point_info(struct sof_ipc_probe_info_params *data, uint32_t max_size)
{
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

	return 1;
}

int probe_point_remove(uint32_t count, uint32_t *buffer_id)
{
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
			if (_probe->probe_points[j].buffer_id == buffer_id[i]) {
				/* TODO: Remove callbacks from buffer and DMA */

				_probe->probe_points[j].stream_tag =
					PROBE_POINT_INVALID;
			}
		}
	}

	return 0;
}
