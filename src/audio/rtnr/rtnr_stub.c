// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org.com>
//

#include <sof/audio/rtnr/rtklib/include/RTK_MA_API.h>
#include <sof/audio/audio_stream.h>
#include <rtos/alloc.h>
#include <stddef.h>
#include <stdint.h>

#define RTNR_STUB_CONTEXT_SIZE	42	/* Just some random size to allocate */

/*
 * The stub replaces the proprietary RTNR library with a plain passthrough: the requested frames
 * are copied straight from the source to the sink circular buffer, honouring wrap on both sides.
 * It works directly on the audio_stream_rtnr descriptors filled in by the component.
 */
static void rtnr_stub_passthrough(struct audio_stream_rtnr **sources,
				  struct audio_stream_rtnr *sink, int frames, size_t sample_bytes)
{
	struct audio_stream_rtnr *source = sources[0];

	cir_buf_copy(source->r_ptr, source->addr, source->end_addr,
		     sink->w_ptr, sink->addr, sink->end_addr,
		     (size_t)frames * sink->channels * sample_bytes);
}

void RTKMA_API_S16_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift)
{
	rtnr_stub_passthrough(sources, sink, frames, sizeof(int16_t));
}

void RTKMA_API_S24_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift)
{
	rtnr_stub_passthrough(sources, sink, frames, sizeof(int32_t));
}

void RTKMA_API_S32_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift)
{
	rtnr_stub_passthrough(sources, sink, frames, sizeof(int32_t));
}

void RTKMA_API_First_Copy(void *Context, int SampleRate, int MicCh)
{}

void RTKMA_API_Process(void *Context, _Bool has_ref, int SampleRate, int MicCh)
{}

void RTKMA_API_Prepare(void *Context)
{}

void *RTKMA_API_Context_Create(int sample_rate)
{
	/* Allocate something, to avoid return NULL and cause error
	 * in check of success of this.
	 */
	return rzalloc(SOF_MEM_FLAG_USER, RTNR_STUB_CONTEXT_SIZE);
}

void RTKMA_API_Context_Free(void *Context)
{
	rfree(Context);
}

int RTKMA_API_Parameter_Size(void *Context, unsigned int IDs)
{
	return 0;
}

int RTKMA_API_Set(void *Context, const void *pParameters, int size, unsigned int IDs)
{
	return 0;
}
