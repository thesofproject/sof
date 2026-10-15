// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2017-2022 Intel Corporation. All rights reserved.
//
// Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
//         Liam Girdwood <liam.r.girdwood@linux.intel.com>
//         Keyon Jie <yang.jie@linux.intel.com>

#include "eq_iir.h"
#include <sof/audio/component.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/audio/buffer.h>
#include <sof/audio/format.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/ipc-config.h>
#include <sof/common.h>
#include <rtos/panic.h>
#include <sof/ipc/msg.h>
#include <rtos/alloc.h>
#include <rtos/init.h>
#include <sof/lib/uuid.h>
#include <sof/list.h>
#include <sof/math/iir_df1.h>
#include <sof/platform.h>
#include <rtos/string.h>
#include <sof/ut.h>
#include <sof/trace/trace.h>
#include <ipc/control.h>
#include <ipc/stream.h>
#include <ipc/topology.h>
#include <user/eq.h>
#include <user/trace.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

LOG_MODULE_DECLARE(eq_iir, CONFIG_SOF_LOG_LEVEL);

#if CONFIG_FORMAT_S16LE
void eq_iir_s16_default(struct processing_module *mod, const struct cir_buf_source *source,
			struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct iir_state_df1 *filter;
	const int16_t *x0;
	int16_t *y0;
	const int16_t *x;
	int16_t *y;
	size_t nmax;
	size_t n1;
	size_t n2;
	unsigned int i;
	size_t j;
	size_t n;
	const unsigned int nch = cd->channels;
	const size_t samples = frames * nch;
	size_t processed = 0;

	x = source->ptr;
	y = sink->ptr;
	while (processed < samples) {
		nmax = samples - processed;
		n1 = cir_buf_samples_without_wrap_s16(x, source->buf_end);
		n2 = cir_buf_samples_without_wrap_s16(y, sink->buf_end);
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1_s16(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE

void eq_iir_s24_default(struct processing_module *mod, const struct cir_buf_source *source,
			struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct iir_state_df1 *filter;
	const int32_t *x0;
	int32_t *y0;
	const int32_t *x;
	int32_t *y;
	size_t nmax;
	size_t n1;
	size_t n2;
	unsigned int i;
	size_t j;
	size_t n;
	const unsigned int nch = cd->channels;
	const size_t samples = frames * nch;
	size_t processed = 0;

	x = source->ptr;
	y = sink->ptr;
	while (processed < samples) {
		nmax = samples - processed;
		n1 = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		n2 = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1_s24(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE

void eq_iir_s32_default(struct processing_module *mod, const struct cir_buf_source *source,
			struct cir_buf_sink *sink, size_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct iir_state_df1 *filter;
	const int32_t *x0;
	int32_t *y0;
	const int32_t *x;
	int32_t *y;
	size_t nmax;
	size_t n1;
	size_t n2;
	unsigned int i;
	size_t j;
	size_t n;
	const unsigned int nch = cd->channels;
	const size_t samples = frames * nch;
	size_t processed = 0;

	x = source->ptr;
	y = sink->ptr;
	while (processed < samples) {
		nmax = samples - processed;
		n1 = cir_buf_samples_without_wrap_s32(x, source->buf_end);
		n2 = cir_buf_samples_without_wrap_s32(y, sink->buf_end);
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = source_cir_buf_wrap(x + n, source->buf_start, source->buf_end);
		y = cir_buf_wrap(y + n, sink->buf_start, sink->buf_end);
	}
}
#endif /* CONFIG_FORMAT_S32LE */



