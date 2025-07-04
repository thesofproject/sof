// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org.com>
//

#include <sof/audio/rtnr/rtklib/include/RTK_MA_API.h>
#include <sof/audio/audio_stream.h>
#include <rtos/alloc.h>

#define RTNR_STUB_CONTEXT_SIZE	42	/* Just some random size to allocate */

void RTKMA_API_S16_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift)
{
	struct audio_stream sof_source;
	struct audio_stream sof_sink;

	rtnr_copy_to_sof_stream(&sof_source, sources[0]);
	rtnr_copy_to_sof_stream(&sof_sink, sink);
	audio_stream_copy(&sof_source, 0, &sof_sink, 0,
			  frames * audio_stream_get_channels(&sof_sink));
	rtnr_copy_from_sof_stream(sources[0], &sof_source);
	rtnr_copy_from_sof_stream(sink, &sof_sink);
}

void RTKMA_API_S24_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift)
{
	struct audio_stream sof_source;
	struct audio_stream sof_sink;

	rtnr_copy_to_sof_stream(&sof_source, sources[0]);
	rtnr_copy_to_sof_stream(&sof_sink, sink);
	audio_stream_copy(&sof_source, 0, &sof_sink, 0,
			  frames * audio_stream_get_channels(&sof_sink));
	rtnr_copy_from_sof_stream(sources[0], &sof_source);
	rtnr_copy_from_sof_stream(sink, &sof_sink);
}

void RTKMA_API_S32_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift)
{
	struct audio_stream sof_source;
	struct audio_stream sof_sink;

	rtnr_copy_to_sof_stream(&sof_source, sources[0]);
	rtnr_copy_to_sof_stream(&sof_sink, sink);
	audio_stream_copy(&sof_source, 0, &sof_sink, 0,
			  frames * audio_stream_get_channels(&sof_sink));
	rtnr_copy_from_sof_stream(sources[0], &sof_source);
	rtnr_copy_from_sof_stream(sink, &sof_sink);
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
