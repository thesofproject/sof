/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 */
#ifndef __SOF_AUDIO_TEMPLATE_COMP_H__
#define __SOF_AUDIO_TEMPLATE_COMP_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <stdbool.h>
#include <stdint.h>

typedef void (*template_comp_func)(const struct processing_module *mod,
				   const struct audio_stream *source,
				   struct audio_stream *sink,
				   uint32_t frames);

/* Template_Comp component private data */
struct template_comp_comp_data {
	template_comp_func template_comp_func; /**< processing function */
	int channels_order[PLATFORM_MAX_CHANNELS];
	int source_format;
	int channels; /**< channels count */
	bool enable; /**< enable processing */
};

struct template_comp_proc_fnmap {
	enum sof_ipc_frame frame_fmt;
	template_comp_func template_comp_proc_func;
};

#endif //  __SOF_AUDIO_TEMPLATE_COMP_H__
