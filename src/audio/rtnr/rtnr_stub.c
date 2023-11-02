// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2023 Google LLC. All rights reserved.
//
// Author: Curtis Malainey <cujomalainey@chromium.org.com>
//

#include <sof/audio/rtnr/rtklib/include/RTK_MA_API.h>
#include <rtos/alloc.h>

void RTKMA_API_S16_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift)
{}

void RTKMA_API_S24_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift)
{}

void RTKMA_API_S32_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift)
{}

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
	return rzalloc(SOF_MEM_ZONE_RUNTIME, 0, SOF_MEM_CAPS_RAM, 42);
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
