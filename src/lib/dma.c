// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2016 Intel Corporation. All rights reserved.
//
// Author: Ranjani Sridharan <ranjani.sridharan@linux.intel.com>

#include <rtos/atomic.h>
#include <rtos/kernel.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#if CONFIG_INTEL_ADSP_MIC_PRIVACY
#include <sof/audio/mic_privacy_manager.h>
#endif
#include <rtos/alloc.h>
#include <rtos/cache.h>
#include <sof/lib/dma.h>
#include <sof/lib/uuid.h>
#include <sof/trace/trace.h>
#include <ipc/topology.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <module/module/base.h>
#include "../audio/copier/copier.h"

/* The DMA controller lookup and its system calls, which run in kernel mode, are in
 * src/kernel/lib/dma-kernel.c
 */

int dma_sg_alloc(struct k_heap *heap,
		 struct dma_sg_elem_array *elem_array,
		 uint32_t flags,
		 uint32_t direction,
		 uint32_t buffer_count, uint32_t buffer_bytes,
		 uintptr_t dma_buffer_addr, uintptr_t external_addr)
{
	int i;

	elem_array->elems = sof_heap_alloc(heap, SOF_MEM_FLAG_USER,
					   sizeof(struct dma_sg_elem) * buffer_count, 0);
	if (!elem_array->elems)
		return -ENOMEM;

	memset(elem_array->elems, 0, sizeof(struct dma_sg_elem) * buffer_count);

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

void dma_sg_free(struct k_heap *heap, struct dma_sg_elem_array *elem_array)
{
	sof_heap_free(heap, elem_array->elems);
	dma_sg_init(elem_array);
}

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

	/* process data; the converter consumes cir_buf descriptors */
	struct cir_buf_source cir_src = {
		.buf_start = audio_stream_get_addr(istream),
		.buf_end = audio_stream_get_end_addr(istream),
		.ptr = audio_stream_get_rptr(istream),
	};
	struct cir_buf_sink cir_snk = {
		.buf_start = audio_stream_get_addr(&sink->stream),
		.buf_end = audio_stream_get_end_addr(&sink->stream),
		.ptr = audio_stream_get_wptr(&sink->stream),
	};

	ret = process(&cir_src, source_channels, &cir_snk, sink_channels, source_samples, chmap);

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

	/* process data; the converter consumes cir_buf descriptors */
	struct cir_buf_source cir_src = {
		.buf_start = audio_stream_get_addr(&source->stream),
		.buf_end = audio_stream_get_end_addr(&source->stream),
		.ptr = audio_stream_get_rptr(&source->stream),
	};
	struct cir_buf_sink cir_snk = {
		.buf_start = audio_stream_get_addr(ostream),
		.buf_end = audio_stream_get_end_addr(ostream),
		.ptr = audio_stream_get_wptr(ostream),
	};

	ret = process(&cir_src, source_channels, &cir_snk, sink_channels, source_samples, chmap);

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

	/* process data; the converter consumes cir_buf descriptors */
	struct cir_buf_source cir_src = {
		.buf_start = audio_stream_get_addr(istream),
		.buf_end = audio_stream_get_end_addr(istream),
		.ptr = audio_stream_get_rptr(istream),
	};
	struct cir_buf_sink cir_snk = {
		.buf_start = audio_stream_get_addr(&sink->stream),
		.buf_end = audio_stream_get_end_addr(&sink->stream),
		.ptr = audio_stream_get_wptr(&sink->stream),
	};

	ret = process(&cir_src, source_channels, &cir_snk, sink_channels, source_samples, chmap);

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
