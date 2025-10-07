/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Intel Corporation.
 *
 */
#ifndef __SOF_AUDIO_STFT_PROCESS_H__
#define __SOF_AUDIO_STFT_PROCESS_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/auditory.h>
#include <sof/math/dct.h>
#include <sof/math/fft.h>

#include <stdbool.h>
#include <stdint.h>

#define STFT_DEBUG	1

#define STFT_BLACKMAN_A0 Q_CONVERT_FLOAT(0.42, 15) /* For MFCC */

#define SOF_STFT_PROCESS_CONFIG_MAX_SIZE 256 /* Max size for configuration data in bytes */

enum sof_stft_process_fft_pad_type {
	STFT_PAD_END = 0,
	STFT_PAD_CENTER = 1,
	STFT_PAD_START = 2,
};

enum sof_stft_process_fft_window_type {
	STFT_RECTANGULAR_WINDOW = 0,
	STFT_BLACKMAN_WINDOW = 1,
	STFT_HAMMING_WINDOW = 2,
	STFT_HANN_WINDOW = 3,
	STFT_POVEY_WINDOW = 4,
};

enum sof_stft_process_mel_log_type {
	MEL_LOG_IS_LOG = 0,
	MEL_LOG_IS_LOG10 = 1,
	MEL_LOG_IS_DB = 2
};

enum sof_stft_process_mel_norm_type {
	STFT_MEL_NORM_NONE = 0,
	STFT_MEL_NORM_SLANEY = 1,
};

enum sof_stft_process_dct_type {
	STFT_DCT_I = 0,
	STFT_DCT_II = 1,
};

struct sof_stft_process_config {
	uint32_t size; /**< Size of this struct in bytes */
	uint32_t reserved[8];
	int32_t sample_frequency; /**< Hz. e.g. 16000 */
	int32_t pmin; /**< Q1.31 linear power, limit minimum Mel energy, e.g. 1e-9 */
	enum sof_stft_process_mel_log_type mel_log; /**< Use MEL_LOG_IS_LOG, LOG10 or DB*/
	enum sof_stft_process_mel_norm_type norm; /**< Use MEL_NORM_SLANEY or MEL_NORM_NONE */
	enum sof_stft_process_fft_pad_type pad; /**< Use PAD_END, PAD_CENTER, PAD_START */
	enum sof_stft_process_fft_window_type window; /**< Use RECTANGULAR_WINDOW, etc. */
	enum sof_stft_process_dct_type dct; /**< Must be DCT_II */
	int16_t blackman_coef; /**< Q1.15, typically set to 0.42 for BLACKMAN_WINDOW */
	int16_t cepstral_lifter; /**< Q7.9, e.g. 22.0 */
	int16_t channel; /**< -1 expect mono, 0 left, 1 right, ... */
	int16_t dither; /**< Reserved, no support */
	int16_t frame_length; /**< samples, e.g. 400 for 25 ms @ 16 kHz*/
	int16_t frame_shift; /**< samples, e.g. 160 for 10 ms @ 16 kHz */
	int16_t high_freq;  /**< Hz, set 0 for Nyquist frequency  */
	int16_t low_freq; /**< Hz, eg. 20 */
	int16_t num_ceps; /**< Number of cepstral coefficients, e.g. 13 */
	int16_t num_mel_bins; /**< Number of internal Mel bands, e.g. 23 */
	int16_t preemphasis_coefficient; /**< Q1.15, e.g. 0.97, or 0 for disable */
	int16_t top_db; /**< Q8.7 dB, limit Mel energies to this value e.g. 200 */
	int16_t vtln_high; /**< Reserved, no support */
	int16_t vtln_low; /**< Reserved, no support */
	int16_t vtln_warp; /**< Reserved, no support */
	bool htk_compat; /**< Must be false */
	bool raw_energy; /**< Reserved, no support */
	bool remove_dc_offset; /**< Reserved, no support */
	bool round_to_power_of_two; /**< Must be true (1) */
	bool snip_edges; /**< Must be true (1) */
	bool subtract_mean; /**< Must be false (0) */
	bool use_energy; /**< Must be false (0) */
	bool reserved_bool1;
	bool reserved_bool2;
	bool reserved_bool3;
} __attribute__((packed));

struct stft_process_buffer {
	int32_t *addr;
	int32_t *end_addr;
	int32_t *r_ptr;
	int32_t *w_ptr;
	int s_avail; /**< samples count */
	int s_free; /**< samples count */
	int s_length; /**< length in samples for wrap */
};

struct stft_process_fft {
	struct icomplex32 *fft_buf; /**< fft_padded_size */
	struct icomplex32 *fft_out; /**< fft_padded_size */
	struct fft_plan *fft_plan;
	struct fft_plan *ifft_plan;
	int fft_fill_start_idx; /**< Set to 0 for pad left, etc. */
	int fft_size;
	int fft_padded_size;
	int fft_hop_size;
	int fft_buf_size;
	int half_fft_size;
	size_t fft_buffer_size; /**< bytes */
};

struct stft_process_state {
	struct stft_process_buffer ibuf; /**< Circular buffer for input data */
	struct stft_process_buffer obuf; /**< Circular buffer for output data */
	struct stft_process_fft fft; /**< FFT related */
	int32_t *power_spectra; /**< Pointer to scratch */
	int32_t *buffers;
	int32_t *prev_data; /**< prev_data_size */
	int16_t *window; /**< fft_size */
	int source_channel;
	int prev_data_size;
	int sample_rate;
	bool waiting_fill; /**< booleans */
	bool prev_samples_valid;
};

/**
 * struct stft_process_func - Function call pointer for process function
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 */
typedef int (*stft_process_func)(const struct processing_module *mod,
			     struct sof_source *source,
			     struct sof_sink *sink,
			     uint32_t frames);

/**
 * struct stft_comp_data
 * @stft_process_func: Pointer to used processing function.
 * @channels_order[]: Vector with desired sink channels order.
 * @source_format: Source samples format.
 * @frame_bytes: Number of bytes in an audio frame.
 * @channels: Channels count.
 * @enable: Control processing on/off, on - reorder channels
 */
struct stft_comp_data {
	stft_process_func stft_process_func;		/**< processing function */
	struct stft_process_state state;
	struct sof_stft_process_config *config;
	size_t frame_bytes;
	int source_channel;
	int max_frames;
	int channels;
	bool fft_done;
};

static inline int stft_process_buffer_samples_without_wrap(struct stft_process_buffer *buffer,
							   int32_t *ptr)
{
	return buffer->end_addr - ptr;
}

static inline int32_t *stft_process_buffer_wrap(struct stft_process_buffer *buffer, int32_t *ptr)
{
	if (ptr >= buffer->end_addr)
		ptr -= buffer->s_length;

	return ptr;
}

/**
 * struct stft_process_proc_fnmap - processing functions for frame formats
 * @frame_fmt: Current frame format
 * @stft_process_proc_func: Function pointer for the suitable processing function
 */
struct stft_process_proc_fnmap {
	enum sof_ipc_frame frame_fmt;
	stft_process_func stft_process_function;
};

/**
 * stft_process_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
stft_process_func stft_process_find_proc_func(enum sof_ipc_frame src_fmt);

/**
 * stft_process_set_config() - Handle controls set
 * @mod: Pointer to module data.
 * @param_id: Id to know control type, used to know ALSA control type.
 * @pos: Position of the fragment in the large message.
 * @data_offset_size: Size of the whole configuration if it is the first or only
 *                    fragment. Otherwise it is offset of the fragment.
 * @fragment: Message payload data.
 * @fragment_size: Size of this fragment.
 * @response_size: Size of response.
 *
 * This function handles the real-time controls. The ALSA controls have the
 * param_id set to indicate the control type. The control ID, from topology,
 * is used to separate the controls instances of same type. In control payload
 * the num_elems defines to how many channels the control is applied to.
 *
 * Return: Zero if success, otherwise error code.
 */
int stft_process_set_config(struct processing_module *mod, uint32_t param_id,
			    enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			    const uint8_t *fragment, size_t fragment_size,
			    uint8_t *response, size_t response_size);

/**
 * stft_process_set_config() - Handle controls get
 * @mod: Pointer to module data.
 * @config_id: Configuration ID.
 * @data_offset_size: Size of the whole configuration if it is the first or only
 *                    fragment. Otherwise it is offset of the fragment.
 * @fragment: Message payload data.
 * @fragment_size: Size of this fragment.
 *
 * This function is used for controls get.
 *
 * Return: Zero if success, otherwise error code.
 */
int stft_process_get_config(struct processing_module *mod, uint32_t config_id,
			    uint32_t *data_offset_size, uint8_t *fragment, size_t fragment_size);

int stft_process_setup(struct processing_module *mod, int max_frames, int rate, int channels);

int stft_process_source_s16(struct stft_comp_data *cd, struct sof_source *source, int frames);

int stft_process_sink_s16(struct stft_comp_data *cd, struct sof_sink *sink, int frames);

int stft_process_source_s32(struct stft_comp_data *cd, struct sof_source *source, int frames);

int stft_process_sink_s32(struct stft_comp_data *cd, struct sof_sink *sink, int frames);

void stft_process_free_buffers(struct processing_module *mod);

void stft_process_s16_default(struct processing_module *mod, struct input_stream_buffer *bsource,
			      struct output_stream_buffer *bsink, int frames);

void stft_process_fill_prev_samples(struct stft_process_buffer *buf, int32_t *prev_data,
				    int prev_data_length);

void stft_process_fill_fft_buffer(struct stft_process_state *state);

void stft_process_apply_window(struct stft_process_state *state);

void stft_process_overlap_add_ifft_buffer(struct stft_process_state *state);

#endif //  __SOF_AUDIO_STFT_PROCESS_H__
