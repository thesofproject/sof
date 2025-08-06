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

/*
 * In early days of SOF the preference for pipelines was 16 bits to save RAM in platforms
 * like Baytrail. However in microphone paths if there was need to digitally boost the gain
 * the quality was bad in topologies where capture DAI was 16 bit and we applied with volume
 * or IIR about 20 dB gain. In practice a 16 bit word got left shifted by some bit positions
 * that effectively made signal like 12 bits. We could achieve a lot better quality by
 * capturing codec and DAI with 24 or 32bits and applying the gain in IIR for the larger word
 * length. Then all 16 bits in the pipelines after DAI and IIR had signal. The IIR was chosen for
 * format conversion because it also canceled the sometimes large DC component
 * (and some lowest non-audible frequencies) in signal.
 * It gave the headroom for signal for amplification.

 * If IPC4 systems ever need the memory save small 16 bit capture paths
 * the format conversion could be brought back.
 */

static eq_iir_func eq_iir_find_func(struct processing_module *mod)
{
	unsigned int valid_bit_depth = mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth;

	comp_dbg(mod->dev, "valid_bit_depth %d", valid_bit_depth);
	switch (valid_bit_depth) {
#if CONFIG_FORMAT_S16LE
	case IPC4_DEPTH_16BIT:
		return eq_iir_s16_default;
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
	case IPC4_DEPTH_24BIT:
		return eq_iir_s24_default;
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
	case IPC4_DEPTH_32BIT:
		return eq_iir_s32_default;
#endif /* CONFIG_FORMAT_S32LE */
	default:
		comp_err(mod->dev, "set_fir_func(), invalid valid_bith_depth");
	}
	return NULL;
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
		cd->eq_iir_func = eq_iir_find_func(mod);
	} else {
		comp_dbg(mod->dev, "eq_iir_new_blob(), pass-through");
		cd->eq_iir_func = eq_iir_pass;
	}

	return 0;
}

static int eq_iir_params(struct processing_module *mod)
{
	struct sof_ipc_stream_params *params = mod->stream_params;
	struct sof_ipc_stream_params comp_params;
	struct comp_dev *dev = mod->dev;
	struct comp_buffer *sinkb;
	enum sof_ipc_frame valid_fmt, frame_fmt;
	int i;

	comp_dbg(dev, "eq_iir_params()");
	comp_params = *params;
	comp_params.channels = mod->priv.cfg.base_cfg.audio_fmt.channels_count;
	comp_params.rate = mod->priv.cfg.base_cfg.audio_fmt.sampling_frequency;
	comp_params.buffer_fmt = mod->priv.cfg.base_cfg.audio_fmt.interleaving_style;

	audio_stream_fmt_conversion(mod->priv.cfg.base_cfg.audio_fmt.depth,
				    mod->priv.cfg.base_cfg.audio_fmt.valid_bit_depth,
				    &frame_fmt, &valid_fmt,
				    mod->priv.cfg.base_cfg.audio_fmt.s_type);

	comp_params.frame_fmt = valid_fmt;

	for (i = 0; i < SOF_IPC_MAX_CHANNELS; i++)
		comp_params.chmap[i] = (mod->priv.cfg.base_cfg.audio_fmt.ch_map >> i * 4) & 0xf;

	component_set_nearest_period_frames(dev, comp_params.rate);

	/* The caller has verified, that sink and source buffers are connected */
	sinkb = comp_dev_get_first_data_consumer(dev);
	return buffer_set_params(sinkb, &comp_params, true);
}

void eq_iir_set_passthrough_func(struct comp_data *cd,
				 enum sof_ipc_frame source_format,
				 enum sof_ipc_frame sink_format)
{
	cd->eq_iir_func = eq_iir_pass;
}

int eq_iir_prepare_sub(struct processing_module *mod)
{
	struct comp_dev *dev = mod->dev;
	int ret = 0;

	ret = eq_iir_params(mod);
	if (ret < 0)
		comp_set_state(dev, COMP_TRIGGER_RESET);

	return ret;
}

