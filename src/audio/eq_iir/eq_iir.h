/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 *         Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __SOF_AUDIO_EQ_IIR_EQ_IIR_H__
#define __SOF_AUDIO_EQ_IIR_EQ_IIR_H__

#include <stdint.h>
#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/iir_df2t.h>
#include <sof/math/iir_df1.h>

/** \brief Macros to convert without division bytes count to samples count */
#define EQ_IIR_BYTES_TO_S16_SAMPLES(b)	((b) >> 1)
#define EQ_IIR_BYTES_TO_S32_SAMPLES(b)	((b) >> 2)

struct audio_stream;
struct comp_dev;

/** \brief Type definition for processing function select return value. */
typedef void (*eq_iir_func)(struct processing_module *mod, struct input_stream_buffer *bsource,
			   struct output_stream_buffer *bsink, uint32_t frames);

/** \brief IIR EQ processing functions map item. */
struct eq_iir_func_map {
	uint8_t source;				/**< source frame format */
	uint8_t sink;				/**< sink frame format */
	eq_iir_func func;			/**< processing function */
};

/* IIR component private data */
struct comp_data {
	struct iir_state_df1 iir[PLATFORM_MAX_CHANNELS]; /**< filters state */
	struct comp_data_blob_handler *model_handler;
	struct sof_eq_iir_config *config;
	int32_t *iir_delay;			/**< pointer to allocated RAM */
	size_t iir_delay_size;			/**< allocated size */
	eq_iir_func eq_iir_func;		/**< processing function */
};

#ifdef UNIT_TEST
void sys_comp_module_eq_iir_interface_init(void);
#endif

void eq_iir_s16_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			struct output_stream_buffer *bsink, uint32_t frames);

void eq_iir_s24_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			struct output_stream_buffer *bsink, uint32_t frames);

void eq_iir_s32_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			struct output_stream_buffer *bsink, uint32_t frames);

int eq_iir_new_blob(struct processing_module *mod, struct comp_data *cd,
		    enum sof_ipc_frame source_format, enum sof_ipc_frame sink_format,
		    int channels);

void eq_iir_set_passthrough_func(struct comp_data *cd,
				 enum sof_ipc_frame source_format,
				 enum sof_ipc_frame sink_format);

int eq_iir_prepare_sub(struct processing_module *mod);

void eq_iir_pass(struct processing_module *mod, struct input_stream_buffer *bsource,
		 struct output_stream_buffer *bsink, uint32_t frames);

int eq_iir_setup(struct processing_module *mod, int nch);

void eq_iir_free_delaylines(struct processing_module *mod);
#endif /* __SOF_AUDIO_EQ_IIR_EQ_IIR_H__ */
