/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Realtek Corporation. All rights reserved.
 *
 * Author: Antz Cheng <antz0525@realtek.com>
 */
#ifndef _RTK_MA_API_H_
#define _RTK_MA_API_H_

#ifdef __cplusplus
extern "C" {
#endif

void RTKMA_API_S16_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift);
void RTKMA_API_S24_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift);
void RTKMA_API_S32_Default(void *Context, struct audio_stream_rtnr **sources,
						struct audio_stream_rtnr *sink, int frames,
						_Bool ref_active, int in_idx, int ref_idx,
						int ref_32bits, int ref_shift);

void RTKMA_API_First_Copy(void *Context, int SampleRate, int MicCh);

void RTKMA_API_Bypass(void *Context, _Bool Bypass);

void RTKMA_API_Process(void *Context, _Bool has_ref, int SampleRate, int MicCh);

void RTKMA_API_Prepare(void *Context);

void *RTKMA_API_Context_Create(int sample_rate);

void RTKMA_API_Context_Free(void *Context);

void RTKMA_API_Set_Default_Parameter(void *Context);

int RTKMA_API_Parameter_Size(void *Context, unsigned int IDs);

int RTKMA_API_Set(void *Context, const void *pParameters, int size, unsigned int IDs);

int	RTKMA_API_Get(void *Context, void *pParameters, int size, unsigned int IDs);

int	RTKMA_API_Set_Preset_Data(void *Context, unsigned int *data,
						unsigned int msg_index, unsigned int num_elms,
						unsigned int elems_remaining);

int	RTKMA_API_Set_Model_Data(void *Context, unsigned int *data,
						unsigned int msg_index, unsigned int num_elms,
						unsigned int elems_remaining);

#ifdef __cplusplus
}
#endif

#endif /* _RTK_MA_API_H_  */

