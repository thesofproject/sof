// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <rtos/atomic.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#if CONFIG_INTEL_ADSP_MIC_PRIVACY
#include <sof/audio/mic_privacy_manager.h>
#endif
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/dma.h>
#include <sof/lib/memory.h>
#include <sof/lib/uuid.h>
#include <rtos/spinlock.h>
#include <rtos/symbol.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <module/module/base.h>
#include "../audio/copier/copier.h"

LOG_MODULE_REGISTER(dma, CONFIG_SOF_LOG_LEVEL);

SOF_DEFINE_REG_UUID(dma);

DECLARE_TR_CTX(dma_tr, SOF_UUID(dma_uuid), LOG_LEVEL_INFO);

#if CONFIG_ZEPHYR_NATIVE_DRIVERS
static int dma_init(struct sof_dma *dma);

struct sof_dma *sof_dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags)
{
	const struct dma_info *info = dma_info_get();
	int users, ret = 0;
	int min_users = INT32_MAX;
	struct sof_dma *d = NULL, *dmin = NULL;
	k_spinlock_key_t key;

	if (!info->num_dmas) {
		tr_err(&dma_tr, "dma_get(): No DMACs installed");
		return NULL;
	}

	/* find DMAC with free channels that matches request */
	for (d = info->dma_array; d < info->dma_array + info->num_dmas;
	     d++) {
		/* skip if this DMAC does not support the requested dir */
		if (dir && (d->plat_data.dir & dir) == 0)
			continue;

		/* skip if this DMAC does not support the requested caps */
		if (cap && (d->plat_data.caps & cap) == 0)
			continue;

		/* skip if this DMAC does not support the requested dev */
		if (dev && (d->plat_data.devs & dev) == 0)
			continue;

		/* if exclusive access is requested */
		if (flags & SOF_DMA_ACCESS_EXCLUSIVE) {
			/* ret DMA with no users */
			if (!d->sref) {
				dmin = d;
				break;
			}
		} else {
			/* get number of users for this DMAC*/
			users = d->sref;

			/* pick DMAC with the least num of users */
			if (users < min_users) {
				dmin = d;
				min_users = users;
			}
		}
	}

	if (!dmin) {
		tr_err(&dma_tr, "No DMAC dir %d caps 0x%x dev 0x%x flags 0x%x",
		       dir, cap, dev, flags);

		for (d = info->dma_array;
		     d < info->dma_array + info->num_dmas;
		     d++) {
			tr_err(&dma_tr, " DMAC ID %d users %d busy channels %ld",
			       d->plat_data.id, d->sref,
			       atomic_read(&d->num_channels_busy));
			tr_err(&dma_tr, "  caps 0x%x dev 0x%x",
			       d->plat_data.caps, d->plat_data.devs);
		}

		return NULL;
	}

	/* return DMAC */
	tr_dbg(&dma_tr, "dma_get(), dma-probe id = %d",
	       dmin->plat_data.id);

	/* Shared DMA controllers with multiple channels
	 * may be requested many times, let the probe()
	 * do on-first-use initialization.
	 */
	key = k_spin_lock(&dmin->lock);

	if (!dmin->sref) {
		ret = dma_init(dmin);
		if (ret < 0) {
			tr_err(&dma_tr, "dma_get(): dma-probe failed id = %d, ret = %d",
			       dmin->plat_data.id, ret);
			goto out;
		}
	}

	dmin->sref++;

	tr_info(&dma_tr, "dma_get() ID %d sref = %d busy channels %ld",
		dmin->plat_data.id, dmin->sref,
		atomic_read(&dmin->num_channels_busy));
out:
	k_spin_unlock(&dmin->lock, key);
	return !ret ? dmin : NULL;
}

void sof_dma_put(struct sof_dma *dma)
{
	k_spinlock_key_t key;

	key = k_spin_lock(&dma->lock);
	if (--dma->sref == 0) {
		rfree(dma->chan);
		dma->chan = NULL;
	}

	tr_info(&dma_tr, "dma_put(), dma = %p, sref = %d",
		dma, dma->sref);
	k_spin_unlock(&dma->lock, key);
}

static int dma_init(struct sof_dma *dma)
{
	struct dma_chan_data *chan;
	int i;

	/* allocate dma channels */
	dma->chan = rzalloc(SOF_MEM_ZONE_RUNTIME_SHARED, 0, SOF_MEM_CAPS_RAM,
			    sizeof(struct dma_chan_data) * dma->plat_data.channels);

	if (!dma->chan) {
		tr_err(&dma_tr, "dma_probe_sof(): dma %d allocaction of channels failed",
		       dma->plat_data.id);
		return -ENOMEM;
	}

	/* init work */
	for (i = 0, chan = dma->chan; i < dma->plat_data.channels;
	     i++, chan++) {
		chan->dma = dma;
		chan->index = i;
	}

	return 0;
}
EXPORT_SYMBOL(sof_dma_get);
EXPORT_SYMBOL(sof_dma_put);
#else
struct dma *dma_get(uint32_t dir, uint32_t cap, uint32_t dev, uint32_t flags)
{
	const struct dma_info *info = dma_info_get();
	int users, ret;
	int min_users = INT32_MAX;
	struct dma *d = NULL, *dmin = NULL;
	k_spinlock_key_t key;

	if (!info->num_dmas) {
		tr_err(&dma_tr, "dma_get(): No DMACs installed");
		return NULL;
	}

	/* find DMAC with free channels that matches request */
	for (d = info->dma_array; d < info->dma_array + info->num_dmas;
	     d++) {
		/* skip if this DMAC does not support the requested dir */
		if (dir && (d->plat_data.dir & dir) == 0)
			continue;

		/* skip if this DMAC does not support the requested caps */
		if (cap && (d->plat_data.caps & cap) == 0)
			continue;

		/* skip if this DMAC does not support the requested dev */
		if (dev && (d->plat_data.devs & dev) == 0)
			continue;

		/* skip if this DMAC has 1 user per avail channel */
		/* TODO: this should be fixed in dai.c to allow more users */
		if (d->sref >= d->plat_data.channels)
			continue;

		/* if exclusive access is requested */
		if (flags & DMA_ACCESS_EXCLUSIVE) {
			/* ret DMA with no users */
			if (!d->sref) {
				dmin = d;
				break;
			}
		} else {
			/* get number of users for this DMAC*/
			users = d->sref;

			/* pick DMAC with the least num of users */
			if (users < min_users) {
				dmin = d;
				min_users = users;
			}
		}
	}

	if (!dmin) {
		tr_err(&dma_tr, "No DMAC dir %d caps 0x%x dev 0x%x flags 0x%x",
		       dir, cap, dev, flags);

		for (d = info->dma_array;
		     d < info->dma_array + info->num_dmas;
		     d++) {
			tr_err(&dma_tr, " DMAC ID %d users %d busy channels %ld",
			       d->plat_data.id, d->sref,
			       atomic_read(&d->num_channels_busy));
			tr_err(&dma_tr, "  caps 0x%x dev 0x%x",
			       d->plat_data.caps, d->plat_data.devs);
		}

		return NULL;
	}

	/* return DMAC */
	tr_dbg(&dma_tr, "dma_get(), dma-probe id = %d",
	       dmin->plat_data.id);

	/* Shared DMA controllers with multiple channels
	 * may be requested many times, let the probe()
	 * do on-first-use initialization.
	 */
	key = k_spin_lock(&dmin->lock);

	ret = 0;
	if (!dmin->sref) {
		ret = dma_probe_legacy(dmin);
		if (ret < 0) {
			tr_err(&dma_tr, "dma_get(): dma-probe failed id = %d, ret = %d",
			       dmin->plat_data.id, ret);
		}
	}
	if (!ret)
		dmin->sref++;

	tr_info(&dma_tr, "dma_get() ID %d sref = %d busy channels %ld",
		dmin->plat_data.id, dmin->sref,
		atomic_read(&dmin->num_channels_busy));

	k_spin_unlock(&dmin->lock, key);
	return !ret ? dmin : NULL;
}

void dma_put(struct dma *dma)
{
	k_spinlock_key_t key;
	int ret;

	key = k_spin_lock(&dma->lock);
	if (--dma->sref == 0) {
		ret = dma_remove_legacy(dma);
		if (ret < 0) {
			tr_err(&dma_tr, "dma_put(): dma_remove() failed id  = %d, ret = %d",
			       dma->plat_data.id, ret);
		}
	}
	tr_info(&dma_tr, "dma_put(), dma = %p, sref = %d",
		dma, dma->sref);
	k_spin_unlock(&dma->lock, key);
}
EXPORT_SYMBOL(dma_get);
EXPORT_SYMBOL(dma_put);
#endif

int dma_sg_alloc(struct dma_sg_elem_array *elem_array,
		 enum mem_zone zone,
		 uint32_t direction,
		 uint32_t buffer_count, uint32_t buffer_bytes,
		 uintptr_t dma_buffer_addr, uintptr_t external_addr)
{
	int i;

	elem_array->elems = rzalloc(zone, 0, SOF_MEM_CAPS_RAM,
				    sizeof(struct dma_sg_elem) * buffer_count);
	if (!elem_array->elems)
		return -ENOMEM;

	for (i = 0; i < buffer_count; i++) {
		elem_array->elems[i].size = buffer_bytes;
		// TODO: may count offsets once
		switch (direction) {
		case SOF_DMA_DIR_MEM_TO_DEV:
		case SOF_DMA_DIR_LMEM_TO_HMEM:
			elem_array->elems[i].src = dma_buffer_addr;
			elem_array->elems[i].dest = external_addr;
			break;
		default:
			elem_array->elems[i].src = external_addr;
			elem_array->elems[i].dest = dma_buffer_addr;
			break;
		}

		dma_buffer_addr += buffer_bytes;
	}
	elem_array->count = buffer_count;
	return 0;
}

void dma_sg_free(struct dma_sg_elem_array *elem_array)
{
	rfree(elem_array->elems);
	dma_sg_init(elem_array);
}
EXPORT_SYMBOL(dma_sg_free);

int dma_buffer_copy_from(struct comp_buffer *source,
			 struct comp_buffer *sink,
			 dma_process_func process, uint32_t source_bytes, uint32_t chmap)
{
	int source_channels = audio_stream_get_channels(&source->stream);
	int sink_channels = audio_stream_get_channels(&sink->stream);
	int source_samples;
	int sink_bytes;
	struct audio_stream *istream = &source->stream;
	int ret;

	/* WORKAROUND: Given that remapping conversion can alter the number of channels, it's
	 * necessary to use frames, not samples, to calculate sink_bytes for writeback/produce.
	 * Unfortunately, the Host DMA imposes size alignment requirement for chunks of data
	 * being copied. For certain frame sizes, this alignment constraint results in splitting
	 * the first and/or last frame in the buffer. Consequently, frame-based calculations
	 * cannot be used with Host DMA. Fortunately, remapping conversion is specifically
	 * designed for use with IPC4 DAI gateway's device posture feature. IPC4 DAI gateway
	 * does not have the same size alignment limitations. Therefore, frame-based calculations
	 * are employed only when necessary —- in the case of IPC4 DAI gateway with remapping
	 * conversion that modifies number of channels.
	 */
	if (source_channels == sink_channels) {
		/* Sample-based calculations can be used here since the source and sink have
		 * the same number of samples. Frame-based calculations, however, cannot be used
		 * in this scenario, as it might involve a gateway that does not supply full frames
		 * (e.g., the Host gateway).
		 */
		source_samples = source_bytes / audio_stream_sample_bytes(istream);
		sink_bytes = source_samples * audio_stream_sample_bytes(&sink->stream);
	} else {
		int frames;

		/* Sample-based calculations cannot be used here since source and sink have
		 * different number of samples. Fortunately, only IPC4 DAI gateway supports
		 * remapping conversion -- it's safe to use frame-based calculations in this
		 * context.
		 */
		assert(source_channels);
		assert(sink_channels);

		frames = source_bytes / audio_stream_frame_bytes(istream);
		sink_bytes = audio_stream_frame_bytes(&sink->stream) * frames;
		source_samples = frames * source_channels;
	}

	/* source buffer contains data copied by DMA */
	audio_stream_invalidate(istream, source_bytes);

	/* process data */
	ret = process(istream, 0, &sink->stream, 0, source_samples, chmap);

	buffer_stream_writeback(sink, sink_bytes);

	/*
	 * consume istream using audio_stream API because this buffer doesn't
	 * appear in topology so notifier event is not needed
	 */
	audio_stream_consume(istream, source_bytes);
	comp_update_buffer_produce(sink, sink_bytes);

	return ret;
}

int dma_buffer_copy_to(struct comp_buffer *source,
		       struct comp_buffer *sink,
		       dma_process_func process, uint32_t sink_bytes, uint32_t chmap)
{
	int source_channels = audio_stream_get_channels(&source->stream);
	int sink_channels = audio_stream_get_channels(&sink->stream);
	int source_samples;
	int source_bytes;
	struct audio_stream *ostream = &sink->stream;
	int ret;

	/* WORKAROUND: Given that remapping conversion can alter the number of channels, it's
	 * necessary to use frames, not samples, to calculate source_bytes for invalidate/consume.
	 * Unfortunately, the Host DMA imposes size alignment requirement for chunks of data
	 * being copied. For certain frame sizes, this alignment constraint results in splitting
	 * the first and/or last frame in the buffer. Consequently, frame-based calculations
	 * cannot be used with Host DMA. Fortunately, remapping conversion is specifically
	 * designed for use with IPC4 DAI gateway's device posture feature. IPC4 DAI gateway
	 * does not have the same size alignment limitations. Therefore, frame-based calculations
	 * are employed only when necessary —- in the case of IPC4 DAI gateway with remapping
	 * conversion that modifies number of channels.
	 */
	if (source_channels == sink_channels) {
		/* Sample-based calculations can be used here since the source and sink have
		 * the same number of samples. Frame-based calculations, however, cannot be used
		 * in this scenario, as it might involve a gateway that does not supply full frames
		 * (e.g., the Host gateway).
		 */
		source_samples = sink_bytes / audio_stream_sample_bytes(ostream);
		source_bytes = source_samples * audio_stream_sample_bytes(&source->stream);
	} else {
		int frames;

		/* Sample-based calculations cannot be used here since source and sink have
		 * different number of samples. Fortunately, only IPC4 DAI gateway supports
		 * remapping conversion -- it's safe to use frame-based calculations in this
		 * context.
		 */
		assert(source_channels);
		assert(sink_channels);

		frames = sink_bytes / audio_stream_frame_bytes(ostream);
		source_bytes = audio_stream_frame_bytes(&source->stream) * frames;
		source_samples = frames * source_channels;
	}

	buffer_stream_invalidate(source, source_bytes);

	/* process data */
	ret = process(&source->stream, 0, ostream, 0, source_samples, chmap);

	/* sink buffer contains data meant to copied to DMA */
	audio_stream_writeback(ostream, sink_bytes);

	/*
	 * produce ostream using audio_stream API because this buffer doesn't
	 * appear in topology so notifier event is not needed
	 */
	audio_stream_produce(ostream, sink_bytes);
	comp_update_buffer_consume(source, source_bytes);

	return ret;
}

int stream_copy_from_no_consume(struct comp_dev *dev, struct comp_buffer *source,
				struct comp_buffer *sink,
				dma_process_func process, uint32_t source_bytes, uint32_t chmap)
{
	int source_channels = audio_stream_get_channels(&source->stream);
	int sink_channels = audio_stream_get_channels(&sink->stream);
	int source_samples;
	int sink_bytes;
	struct audio_stream *istream = &source->stream;
	int ret;

	/* WORKAROUND: Given that remapping conversion can alter the number of channels, it's
	 * necessary to use frames, not samples, to calculate sink_bytes for writeback/produce.
	 * Unfortunately, the Host DMA imposes size alignment requirement for chunks of data
	 * being copied. For certain frame sizes, this alignment constraint results in splitting
	 * the first and/or last frame in the buffer. Consequently, frame-based calculations
	 * cannot be used with Host DMA. Fortunately, remapping conversion is specifically
	 * designed for use with IPC4 DAI gateway's device posture feature. IPC4 DAI gateway
	 * does not have the same size alignment limitations. Therefore, frame-based calculations
	 * are employed only when necessary —- in the case of IPC4 DAI gateway with remapping
	 * conversion that modifies number of channels.
	 */
	if (source_channels == sink_channels) {
		/* Sample-based calculations can be used here since the source and sink have
		 * the same number of samples. Frame-based calculations, however, cannot be used
		 * in this scenario, as it might involve a gateway that does not supply full frames
		 * (e.g., the Host gateway).
		 */
		source_samples = source_bytes / audio_stream_sample_bytes(istream);
		sink_bytes = source_samples * audio_stream_sample_bytes(&sink->stream);
	} else {
		int frames;

		/* Sample-based calculations cannot be used here since source and sink have
		 * different number of samples. Fortunately, only IPC4 DAI gateway supports
		 * remapping conversion -- it's safe to use frame-based calculations in this
		 * context.
		 */
		assert(source_channels);
		assert(sink_channels);

		frames = source_bytes / audio_stream_frame_bytes(istream);
		sink_bytes = audio_stream_frame_bytes(&sink->stream) * frames;
		source_samples = frames * source_channels;
	}

	/* process data */
	ret = process(istream, 0, &sink->stream, 0, source_samples, chmap);

#if CONFIG_INTEL_ADSP_MIC_PRIVACY
	struct processing_module *mod = comp_mod(dev);
	struct copier_data *cd = module_get_private_data(mod);

	if (cd->mic_priv)
		mic_privacy_process(dev, cd->mic_priv, sink, source_samples);
#endif

	buffer_stream_writeback(sink, sink_bytes);

	comp_update_buffer_produce(sink, sink_bytes);

	return ret;
}
