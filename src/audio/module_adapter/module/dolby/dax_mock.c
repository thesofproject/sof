// SPDX-License-Identifier: BSD-3-Clause
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE
//
// Copyright(c) 2025 Dolby Laboratories. All rights reserved.
//
// Author: Jun Lai <jun.lai@dolby.com>
//

#include <dax_inf.h>
#include <rtos/string.h>

#define PLACEHOLDER_BUF_SZ 8

uint32_t dax_query_persist_memory(struct sof_dax *dax_ctx)
{
	return PLACEHOLDER_BUF_SZ;
}

uint32_t dax_query_scratch_memory(struct sof_dax *dax_ctx)
{
	return PLACEHOLDER_BUF_SZ;
}

uint32_t dax_query_period_frames(struct sof_dax *dax_ctx)
{
	return 256;
}

int dax_free(struct sof_dax *dax_ctx)
{
	return 0;
}

int dax_init(struct sof_dax *dax_ctx)
{
	return 0;
}

int dax_process(struct sof_dax *dax_ctx)
{
	uint32_t peroid_bytes = dax_query_period_frames(dax_ctx) *
				dax_ctx->input_media_format.num_channels *
				dax_ctx->input_media_format.bytes_per_sample;

	if (dax_ctx->input_buffer.avail < peroid_bytes ||
	    dax_ctx->output_buffer.free < peroid_bytes) {
		return 0;
	}
	memcpy_s((uint8_t *)dax_ctx->output_buffer.addr + dax_ctx->output_buffer.avail,
		 dax_ctx->output_buffer.free,
		 dax_ctx->input_buffer.addr,
		 peroid_bytes);
	return peroid_bytes;
}

int dax_set_param(uint32_t id, const void *val, uint32_t val_sz, struct sof_dax *dax_ctx)
{
	return 0;
}

int dax_set_enable(int32_t enable, struct sof_dax *dax_ctx)
{
	return 0;
}

int dax_set_volume(int32_t pregain, struct sof_dax *dax_ctx)
{
	return 0;
}

int dax_set_device(int32_t out_device, struct sof_dax *dax_ctx)
{
	return 0;
}

int dax_set_ctc_enable(int32_t enable, struct sof_dax *dax_ctx)
{
	return 0;
}

const char *dax_get_version(void)
{
	return "";
}

void *dax_find_params(uint32_t query_id,
		      int32_t query_val,
		      uint32_t *query_sz,
		      struct sof_dax *dax_ctx)
{
	return NULL;
}
