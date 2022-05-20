/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_MFCC_MFCC_COMP_H__
#define __SOF_AUDIO_MFCC_MFCC_COMP_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/auditory.h>
#include <sof/math/dct.h>
#include <sof/math/fft.h>
#include <stddef.h>
#include <stdint.h>

#define MFCC_DEBUG_WITH_TESTBENCH

#if defined(CONFIG_LIBRARY) && defined(MFCC_DEBUG_WITH_TESTBENCH)
#define MFCC_DEBUGFILES
#endif

#define MFCC_MAGIC 0x6d666363 /* ASCII for "mfcc" */

/* Set to 16 for lower RAM and MCPS with slightly lower quality. Set to 32 for best
 * quality but higher MCPS and RAM. The MFCC input is currently 16 bits. With this option
 * set to 32 the FFT and Mel filterbank are computed with better 32 bit precision. There
 * is also need to enable 32 bit FFT from Kconfig if set.
 */
#define MFCC_FFT_BITS	16

struct audio_stream;
struct comp_dev;

/** \brief Type definition for processing function select return value. */
typedef void (*mfcc_func)(struct processing_module *mod,
			  struct input_stream_buffer *bsource,
			  struct output_stream_buffer *bsink,
			  int frames);

/** \brief MFCC processing functions map item. */
struct mfcc_func_map {
	uint8_t source;		/**< source frame format */
	mfcc_func func;		/**< processing function */
};

struct mfcc_buffer {
	int16_t *addr;
	int16_t *end_addr;
	int16_t *r_ptr;
	int16_t *w_ptr;
	int s_avail; /**< samples count */
	int s_free; /**< samples count */
	int s_length; /**< length in samples for wrap */
};

struct mfcc_pre_emph {
	int16_t coef;
	int16_t delay;
	int enable;
};

struct mfcc_fft {
#if MFCC_FFT_BITS == 16
	struct icomplex16 *fft_buf; /**< fft_padded_size */
	struct icomplex16 *fft_out; /**< fft_padded_size */
#elif MFCC_FFT_BITS == 32
	struct icomplex32 *fft_buf; /**< fft_padded_size */
	struct icomplex32 *fft_out; /**< fft_padded_size */
#else
#error "MFCC_FFT_BITS needs to be 16 or 32"
#endif
	struct fft_plan *fft_plan;
	int fft_fill_start_idx; /**< Set to 0 for pad left, etc. */
	int fft_size;
	int fft_padded_size;
	int fft_hop_size;
	int fft_buf_size;
	int half_fft_size;
	size_t fft_buffer_size; /**< bytes */
};

struct mfcc_cepstral_lifter {
	struct mat_matrix_16b *matrix;
	int16_t cepstral_lifter;
	int num_ceps;
};

struct mfcc_state {
	struct mfcc_buffer buf; /**< Circular buffer for input data */
	struct mfcc_pre_emph emph; /**< Pre-emphasis filter */
	struct mfcc_fft fft; /**< FFT related */
	struct dct_plan_16 dct; /**< DCT related */
	struct psy_mel_filterbank melfb; /**< Mel filter bank */
	struct mfcc_cepstral_lifter lifter; /**< Cepstral lifter coefficients */
	struct mat_matrix_16b *mel_spectra; /**< Pointer to scratch */
	struct mat_matrix_16b *cepstral_coef; /**< Pointer to scratch */
	int32_t *power_spectra; /**< Pointer to scratch */
	int16_t buf_avail;
	int16_t *buffers;
	int16_t *prev_data; /**< prev_data_size */
	int16_t *window; /**< fft_size */
	int16_t *triangles;
	int source_channel;
	int buffer_size;
	int prev_data_size;
	int low_freq;
	int high_freq;
	int sample_rate;
	int waiting_fill:1; /**< booleans */
	int prev_samples_valid:1;
	size_t sample_buffers_size; /**< bytes */
};

/* MFCC component private data */
struct mfcc_comp_data {
	struct mfcc_state state;
	struct comp_data_blob_handler *model_handler;
	struct sof_mfcc_config *config;
	int max_frames;
	mfcc_func mfcc_func;		/**< processing function */
};

static inline int mfcc_buffer_samples_without_wrap(struct mfcc_buffer *buffer, int16_t *ptr)
{
	return buffer->end_addr - ptr;
}

static inline int16_t *mfcc_buffer_wrap(struct mfcc_buffer *buffer, int16_t *ptr)
{
	if (ptr >= buffer->end_addr)
		ptr -= buffer->s_length;

	return ptr;
}

int mfcc_setup(struct processing_module *mod, int max_frames, int rate, int channels);

void mfcc_free_buffers(struct mfcc_comp_data *cd);

void mfcc_s16_default(struct processing_module *mod, struct input_stream_buffer *bsource,
		      struct output_stream_buffer *bsink, int frames);

#ifdef MFCC_DEBUGFILES
void mfcc_generic_debug_open(void);
void mfcc_generic_debug_close(void);
#endif

#ifdef UNIT_TEST
void sys_comp_mfcc_init(void);
#endif

#endif /* __SOF_AUDIO_MFCC_MFCC_COMP_H__ */
