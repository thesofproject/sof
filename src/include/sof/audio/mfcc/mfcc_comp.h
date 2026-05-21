/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022-2026 Intel Corporation.
 *
 * Author: Seppo Ingalsuo <seppo.ingalsuo@linux.intel.com>
 */

#ifndef __SOF_AUDIO_MFCC_MFCC_COMP_H__
#define __SOF_AUDIO_MFCC_MFCC_COMP_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/audio/data_blob.h>
#include <sof/math/auditory.h>
#include <sof/math/dct.h>
#include <sof/math/fft.h>
#include <sof/audio/mfcc/mfcc_vad.h>
#include <sof/ipc/msg.h>
#include <stddef.h>
#include <stdint.h>

/* __XCC__ is both for xt_xcc and xt_clang */
#if defined(__XCC__)
# include <xtensa/config/core-isa.h>
# if XCHAL_HAVE_HIFI4 || XCHAL_HAVE_HIFI5
#  define MFCC_HIFI4
# elif XCHAL_HAVE_HIFI3
#  define MFCC_HIFI3
# else
#  define MFCC_GENERIC
# endif
#else
# define MFCC_GENERIC
#endif

#define MFCC_MAGIC 0x6d666363 /* ASCII for "mfcc" */
#define MFCC_FFT_BITS	32
#define MFCC_MAX_SAMPLE_RATE 64000 /* Max sample rate in Hz, limited by int16_t Mel scale */

/** \brief Switch control index for VAD notification to user space */
#define MFCC_CTRL_INDEX_VAD	0

/**
 * \brief Data header prepended to every MFCC output frame.
 *
 * Written before the Mel spectrum or cepstral coefficient data in each
 * output frame.
 */
struct mfcc_data_header {
	uint32_t magic;		/**< Magic word MFCC_MAGIC (0x6d666363) */
	uint32_t frame_number;	/**< Frame number, counting calculated frames starting from 0 */
	int32_t reserved;	/**< Reserved for future use, set to 0 */
	int32_t energy;		/**< Weighted signal energy in Q9.23 */
	int32_t noise_energy;	/**< Weighted noise floor energy in Q9.23 */
	int32_t vad_flag;	/**< VAD decision: 1 = speech, 0 = silence */
};

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
	struct icomplex32 *fft_buf; /**< fft_padded_size */
	struct icomplex32 *fft_out; /**< fft_padded_size */
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
	int32_t *mel_log_32; /**< Pointer to scratch for 32-bit Mel output Q9.23 */
	int32_t mmax; /**< Maximum Mel value in Q9.23 */
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
	bool mel_only; /**< When true, output Mel spectra instead of cepstral coefficients */
	bool waiting_fill; /**< booleans */
	bool prev_samples_valid;
	bool header_pending; /**< True when data header not yet written for current output */
	struct mfcc_data_header header; /**< Data header for current output frame */
	size_t sample_buffers_size; /**< bytes */
	int16_t *out_data_ptr; /**< Read pointer into scratch data for multi-period output */
	int32_t *out_data_ptr_32; /**< Read pointer for 32-bit mel-only output */
	int out_remain; /**< Remaining int16_t samples to write to sink from scratch */
	uint32_t hop_count; /**< FFT hop counter, increments every processed hop */
};

/* MFCC component private data */
struct mfcc_comp_data {
	struct mfcc_state state;
	struct mfcc_vad_state vad;
	struct comp_data_blob_handler *model_handler;
	struct sof_mfcc_config *config;
	struct ipc_msg *msg;		/**< IPC notification for VAD switch control */
	int max_frames;
	bool vad_prev;			/**< Previous VAD state for edge detection */
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

void mfcc_free_buffers(struct processing_module *mod);

void mfcc_fill_prev_samples(struct mfcc_buffer *buf, int16_t *prev_data,
			    int prev_data_length);

void mfcc_fill_fft_buffer(struct mfcc_state *state);

void mfcc_apply_window(struct mfcc_state *state, int input_shift);

#if CONFIG_FORMAT_S16LE

void mfcc_source_copy_s16(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel);

void mfcc_s16_default(struct processing_module *mod, struct input_stream_buffer *bsource,
		      struct output_stream_buffer *bsink, int frames);
#endif

#if CONFIG_FORMAT_S24LE

void mfcc_source_copy_s24(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel);

void mfcc_s24_default(struct processing_module *mod, struct input_stream_buffer *bsource,
		      struct output_stream_buffer *bsink, int frames);
#endif

#if CONFIG_FORMAT_S32LE

void mfcc_source_copy_s32(struct input_stream_buffer *bsource, struct mfcc_buffer *buf,
			  struct mfcc_pre_emph *emph, int frames, int source_channel);

void mfcc_s32_default(struct processing_module *mod, struct input_stream_buffer *bsource,
		      struct output_stream_buffer *bsink, int frames);
#endif

#if CONFIG_IPC_MAJOR_4
int mfcc_ipc_notification_init(struct processing_module *mod);

void mfcc_send_vad_notification(struct processing_module *mod, uint32_t val);

int mfcc_get_config(struct processing_module *mod,
		    uint32_t config_id, uint32_t *data_offset_size,
		    uint8_t *fragment, size_t fragment_size);

int mfcc_set_config(struct processing_module *mod, uint32_t config_id,
		    enum module_cfg_fragment_position pos, uint32_t data_offset_size,
		    const uint8_t *fragment, size_t fragment_size, uint8_t *response,
		    size_t response_size);

#else
static inline int mfcc_ipc_notification_init(struct processing_module *mod)
{
	return 0;
}

static inline void mfcc_send_vad_notification(struct processing_module *mod, uint32_t val)
{
}

static inline int mfcc_get_config(struct processing_module *mod,
				  uint32_t config_id, uint32_t *data_offset_size,
				  uint8_t *fragment, size_t fragment_size)
{
	struct sof_ipc_ctrl_data *cdata = (struct sof_ipc_ctrl_data *)fragment;
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	return comp_data_blob_get_cmd(cd->model_handler, cdata, fragment_size);
}

static inline int mfcc_set_config(struct processing_module *mod, uint32_t config_id,
				  enum module_cfg_fragment_position pos, uint32_t data_offset_size,
				  const uint8_t *fragment, size_t fragment_size, uint8_t *response,
				  size_t response_size)
{
	struct mfcc_comp_data *cd = module_get_private_data(mod);

	return comp_data_blob_set(cd->model_handler, pos, data_offset_size,
				  fragment, fragment_size);
}
#endif

#ifdef UNIT_TEST
void sys_comp_module_mfcc_interface_init(void);
#endif

#endif /* __SOF_AUDIO_MFCC_MFCC_COMP_H__ */
