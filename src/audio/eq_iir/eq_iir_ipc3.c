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

#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE
static void eq_iir_s32_16_default(struct processing_module *mod,
				  struct input_stream_buffer *bsource,
				  struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct iir_state_df1 *filter;
	int32_t *x0;
	int16_t *y0;
	int32_t *x;
	int16_t *y;
	int nmax;
	int n1;
	int n2;
	int i;
	int j;
	int n;
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	x = audio_stream_get_rptr(source);
	y = audio_stream_get_wptr(sink);
	while (processed < samples) {
		nmax = samples - processed;
		n1 = audio_stream_bytes_without_wrap(source, x) >> 2; /* divide 4 */
		n2 = audio_stream_bytes_without_wrap(sink, y) >> 1; /* divide 2 */
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1_s32_s16(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE
static void eq_iir_s32_24_default(struct processing_module *mod,
				  struct input_stream_buffer *bsource,
				  struct output_stream_buffer *bsink, uint32_t frames)
{
	struct comp_data *cd = module_get_private_data(mod);
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	struct iir_state_df1 *filter;
	int32_t *x0;
	int32_t *y0;
	int32_t *x;
	int32_t *y;
	int nmax;
	int n1;
	int n2;
	int i;
	int j;
	int n;
	const int nch = audio_stream_get_channels(source);
	const int samples = frames * nch;
	int processed = 0;

	x = audio_stream_get_rptr(source);
	y = audio_stream_get_wptr(sink);
	while (processed < samples) {
		nmax = samples - processed;
		n1 = audio_stream_bytes_without_wrap(source, x) >> 2;
		n2 = audio_stream_bytes_without_wrap(sink, y) >> 2;
		n = MIN(n1, n2);
		n = MIN(n, nmax);
		for (i = 0; i < nch; i++) {
			x0 = x + i;
			y0 = y + i;
			filter = &cd->iir[i];
			for (j = 0; j < n; j += nch) {
				*y0 = iir_df1_s32_s24(filter, *x0);
				x0 += nch;
				y0 += nch;
			}
		}
		processed += n;
		x = audio_stream_wrap(source, x + n);
		y = audio_stream_wrap(sink, y + n);
	}
}
#endif /* CONFIG_FORMAT_S32LE && CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
static void eq_iir_s32_s16_pass(struct processing_module *mod, struct input_stream_buffer *bsource,
				struct output_stream_buffer *bsink, uint32_t frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	int32_t *x = audio_stream_get_rptr(source);
	int16_t *y = audio_stream_get_wptr(sink);
	int nmax;
	int n;
	int i;
	int remaining_samples = frames * audio_stream_get_channels(source);

	while (remaining_samples) {
		nmax = EQ_IIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(source, x));
		n = MIN(remaining_samples, nmax);
		nmax = EQ_IIR_BYTES_TO_S16_SAMPLES(audio_stream_bytes_without_wrap(sink, y));
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*y = sat_int16(Q_SHIFT_RND(*x, 31, 15));
			x++;
			y++;
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE */

#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
static void eq_iir_s32_s24_pass(struct processing_module *mod, struct input_stream_buffer *bsource,
				struct output_stream_buffer *bsink, uint32_t frames)
{
	struct audio_stream *source = bsource->data;
	struct audio_stream *sink = bsink->data;
	int32_t *x = audio_stream_get_rptr(source);
	int32_t *y = audio_stream_get_wptr(sink);
	int nmax;
	int n;
	int i;
	int remaining_samples = frames * audio_stream_get_channels(source);

	while (remaining_samples) {
		nmax = EQ_IIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(source, x));
		n = MIN(remaining_samples, nmax);
		nmax = EQ_IIR_BYTES_TO_S32_SAMPLES(audio_stream_bytes_without_wrap(sink, y));
		n = MIN(n, nmax);
		for (i = 0; i < n; i++) {
			*y = sat_int24(Q_SHIFT_RND(*x, 31, 23));
			x++;
			y++;
		}
		remaining_samples -= n;
		x = audio_stream_wrap(source, x);
		y = audio_stream_wrap(sink, y);
	}
}
#endif /* CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE */

const struct eq_iir_func_map fm_configured[] = {
#if CONFIG_FORMAT_S16LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s16_default},
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S24_4LE, NULL},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,  NULL},

#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s32_16_default},
#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, eq_iir_s24_default},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S24_4LE, eq_iir_s32_24_default},
#endif /* CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE */
#if CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S32_LE,  eq_iir_s32_default},
#endif /* CONFIG_FORMAT_S32LE */
};

const struct eq_iir_func_map fm_passthrough[] = {
#if CONFIG_FORMAT_S16LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_pass},
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S24_4LE, NULL},
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S16_LE,  NULL},

#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S24LE*/
#if CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S16_LE,  SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S16_LE,  eq_iir_s32_s16_pass},
#endif /* CONFIG_FORMAT_S16LE && CONFIG_FORMAT_S32LE*/
#if CONFIG_FORMAT_S24LE
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S24_4LE, eq_iir_pass},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S24LE && CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S24_4LE, SOF_IPC_FRAME_S32_LE,  NULL},
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S24_4LE, eq_iir_s32_s24_pass},
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	{SOF_IPC_FRAME_S32_LE,  SOF_IPC_FRAME_S32_LE,  eq_iir_pass},
#endif /* CONFIG_FORMAT_S32LE */
};

static eq_iir_func eq_iir_find_func(enum sof_ipc_frame source_format,
				    enum sof_ipc_frame sink_format,
				    const struct eq_iir_func_map *map,
				    int n)
{
	int i;

	/* Find suitable processing function from map. */
	for (i = 0; i < n; i++) {
		if ((uint8_t)source_format != map[i].source)
			continue;
		if ((uint8_t)sink_format != map[i].sink)
			continue;

		return map[i].func;
	}

	return NULL;
}

static int eq_iir_verify_params(struct comp_dev *dev,
				struct sof_ipc_stream_params *params)
{
	struct comp_buffer *sourceb, *sinkb;
	uint32_t buffer_flag;
	int ret;

	comp_dbg(dev, "eq_iir_verify_params()");

	/* EQ component will only ever have 1 source and 1 sink buffer */
	sourceb = comp_dev_get_first_data_producer(dev);
	sinkb = comp_dev_get_first_data_consumer(dev);

	/* we check whether we can support frame_fmt conversion (whether we have
	 * such conversion function) due to source and sink buffer frame_fmt's.
	 * If not, we will overwrite sink (playback) and source (capture) with
	 * pcm frame_fmt and will not make any conversion (sink and source
	 * frame_fmt will be equal).
	 */
	buffer_flag = eq_iir_find_func(audio_stream_get_frm_fmt(&sourceb->stream),
				       audio_stream_get_frm_fmt(&sinkb->stream), fm_configured,
				       ARRAY_SIZE(fm_configured)) ?
				       BUFF_PARAMS_FRAME_FMT : 0;

	ret = comp_verify_params(dev, buffer_flag, params);
	if (ret < 0) {
		comp_err(dev, "eq_iir_verify_params(): comp_verify_params() failed.");
		return ret;
	}

	return 0;
}

int eq_iir_new_blob(struct processing_module *mod, struct comp_data *cd,
		    enum sof_ipc_frame source_format, enum sof_ipc_frame sink_format,
		    int channels)
{
	int ret;

	ret = eq_iir_setup(mod, channels);
	if (ret < 0) {
		comp_err(mod->dev, "eq_iir_new_blob(), failed IIR setup");
		return ret;
	} else if (cd->iir_delay_size) {
		comp_dbg(mod->dev, "eq_iir_new_blob(), active");
		cd->eq_iir_func = eq_iir_find_func(source_format, sink_format, fm_configured,
						   ARRAY_SIZE(fm_configured));
	} else {
		comp_dbg(mod->dev, "eq_iir_new_blob(), pass-through");
		cd->eq_iir_func = eq_iir_find_func(source_format, sink_format, fm_passthrough,
						   ARRAY_SIZE(fm_passthrough));
	}

	return 0;
}

void eq_iir_set_passthrough_func(struct comp_data *cd,
				 enum sof_ipc_frame source_format,
				 enum sof_ipc_frame sink_format)
{
	cd->eq_iir_func = eq_iir_find_func(source_format, sink_format, fm_passthrough,
					   ARRAY_SIZE(fm_passthrough));
}

int eq_iir_prepare_sub(struct processing_module *mod)
{
	return eq_iir_verify_params(mod->dev, mod->stream_params);
}
