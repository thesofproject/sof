/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2017 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_EQ_FIR_EQ_FIR_H__
#define __SOF_AUDIO_EQ_FIR_EQ_FIR_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/format.h>
#include <sof/math/fir_config.h>
#include <sof/common.h>

#if SOF_USE_HIFI(NONE, FILTER)
#include <sof/math/fir_generic.h>
#endif
#if SOF_USE_HIFI(2, FILTER)
#include <sof/math/fir_hifi2ep.h>
#endif
#if SOF_USE_MIN_HIFI(3, FILTER)
#include <sof/math/fir_hifi3.h>
#endif
#include <user/fir.h>
#include <stdint.h>

/** \brief Macros to convert without division bytes count to samples count */
#define EQ_FIR_BYTES_TO_S16_SAMPLES(b)	((b) >> 1)
#define EQ_FIR_BYTES_TO_S32_SAMPLES(b)	((b) >> 2)

/* fir component private data */
struct comp_data {
	struct fir_state_32x16 fir[PLATFORM_MAX_CHANNELS]; /**< filters state */
	struct comp_data_blob_handler *model_handler;
	struct sof_eq_fir_config *config;
	int32_t *fir_delay;			/**< pointer to allocated RAM */
	size_t fir_delay_size;			/**< allocated size */
	void (*eq_fir_func)(struct fir_state_32x16 fir[],
			    struct input_stream_buffer *bsource,
			    struct output_stream_buffer *bsink,
			    int frames);
	int nch;
};

#if CONFIG_FORMAT_S16LE
void eq_fir_s16(struct fir_state_32x16 *fir, struct input_stream_buffer *bsource,
		struct output_stream_buffer *bsink, int frames);

void eq_fir_2x_s16(struct fir_state_32x16 *fir, struct input_stream_buffer *bsource,
		   struct output_stream_buffer *bsink, int frames);
#endif /* CONFIG_FORMAT_S16LE */

#if CONFIG_FORMAT_S24LE
void eq_fir_s24(struct fir_state_32x16 *fir, struct input_stream_buffer *bsource,
		struct output_stream_buffer *bsink, int frames);

void eq_fir_2x_s24(struct fir_state_32x16 *fir, struct input_stream_buffer *bsource,
		   struct output_stream_buffer *bsink, int frames);
#endif /* CONFIG_FORMAT_S24LE */

#if CONFIG_FORMAT_S32LE
void eq_fir_s32(struct fir_state_32x16 *fir, struct input_stream_buffer *bsource,
		struct output_stream_buffer *bsink, int frames);

void eq_fir_2x_s32(struct fir_state_32x16 *fir, struct input_stream_buffer *bsource,
		   struct output_stream_buffer *bsink, int frames);
#endif /* CONFIG_FORMAT_S32LE */

int set_fir_func(struct processing_module *mod, enum sof_ipc_frame fmt);

int eq_fir_params(struct processing_module *mod);

/*
 * The optimized FIR functions variants need to be updated into function
 * set_fir_func.
 */

#if SOF_USE_MIN_HIFI(2, FILTER)
#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s16;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s24;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_2x_s32;
}
#endif /* CONFIG_FORMAT_S32LE */

#else
/* FIR_GENERIC */
#if CONFIG_FORMAT_S16LE
static inline void set_s16_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s16;
}
#endif /* CONFIG_FORMAT_S16LE */
#if CONFIG_FORMAT_S24LE
static inline void set_s24_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s24;
}
#endif /* CONFIG_FORMAT_S24LE */
#if CONFIG_FORMAT_S32LE
static inline void set_s32_fir(struct comp_data *cd)
{
	cd->eq_fir_func = eq_fir_s32;
}
#endif /* CONFIG_FORMAT_S32LE */
#endif

#ifdef UNIT_TEST
void sys_comp_module_eq_fir_interface_init(void);
#endif

#endif /* __SOF_AUDIO_EQ_FIR_EQ_FIR_H__ */
