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

#if CONFIG_LIBRARY
#define STFT_DEBUG	0	/* Keep zero, produces large files with fprintf() */
#else
#define STFT_DEBUG	0
#endif

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

struct sof_stft_process_config {
	uint32_t size; /**< Size of this struct in bytes */
	uint32_t reserved[8];
	int32_t sample_frequency; /**< Hz. e.g. 16000 */
	int32_t window_gain_comp; /**< Q1.31 gain for IFFT */
	int32_t reserved_32;
	int16_t channel; /**< -1 expect mono, 0 left, 1 right, ... */
	int16_t frame_length; /**< samples, e.g. 400 for 25 ms @ 16 kHz*/
	int16_t frame_shift; /**< samples, e.g. 160 for 10 ms @ 16 kHz */
	int16_t reserved_16;
	enum sof_stft_process_fft_pad_type pad; /**< Use PAD_END, PAD_CENTER, PAD_START */
	enum sof_stft_process_fft_window_type window; /**< Use RECTANGULAR_WINDOW, etc. */
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
	struct ipolar32 *fft_polar;
	struct fft_multi_plan *fft_plan;
	struct fft_multi_plan *ifft_plan;
	int fft_fill_start_idx; /**< Set to 0 for pad left, etc. */
	int fft_size;
	int fft_padded_size;
	int fft_hop_size;
	int fft_buf_size;
	int half_fft_size;
	size_t fft_buffer_size; /**< bytes */
};

struct stft_process_state {
	struct stft_process_buffer ibuf[PLATFORM_MAX_CHANNELS]; /**< Buffer for input data */
	struct stft_process_buffer obuf[PLATFORM_MAX_CHANNELS]; /**< Buffer for output data */
	struct stft_process_fft fft; /**< FFT related */
	int32_t *prev_data[PLATFORM_MAX_CHANNELS]; /**< prev_data_size */
	int32_t gain_comp; /**< Gain to compensate window gain */
	int32_t *buffers;
	int32_t *window; /**< fft_size */
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

#if CONFIG_IPC_MAJOR_4
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
#else
static inline int stft_process_set_config(struct processing_module *mod, uint32_t param_id,
					  enum module_cfg_fragment_position pos,
					  uint32_t data_offset_size,
					  const uint8_t *fragment, size_t fragment_size,
					  uint8_t *response, size_t response_size)
{
	return 0;
}

static inline int stft_process_get_config(struct processing_module *mod, uint32_t config_id,
					  uint32_t *data_offset_size, uint8_t *fragment,
					  size_t fragment_size)
{
	return 0;
}
#endif

/**
 * stft_process_setup() - Initialize STFT processing state and allocate buffers.
 * @mod: Pointer to processing module.
 * @max_frames: Maximum number of frames per processing call.
 * @rate: Audio sample rate in Hz.
 * @channels: Number of audio channels.
 *
 * Configures FFT parameters, allocates aligned sample and FFT buffers,
 * sets up window function, and creates FFT/IFFT plans based on the
 * component configuration.
 *
 * Return: Zero on success, otherwise a negative error code.
 */
int stft_process_setup(struct processing_module *mod, int max_frames, int rate, int channels);

/**
 * stft_process_source_s16() - Copy S16_LE source data to STFT internal buffers.
 * @cd: STFT component data.
 * @source: Source for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * De-interleaves S16_LE audio frames from the source circular buffer
 * into per-channel internal circular buffers. Each 16-bit sample is
 * converted to Q1.31 format by left-shifting 16 bits.
 *
 * Return: Zero on success, otherwise an error code.
 */
int stft_process_source_s16(struct stft_comp_data *cd, struct sof_source *source, int frames);

/**
 * stft_process_sink_s16() - Copy STFT internal buffers to S16_LE sink.
 * @cd: STFT component data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to produce.
 *
 * Interleaves per-channel STFT output buffers into the sink circular
 * buffer in S16_LE format. Q1.31 samples are converted to Q1.15 with
 * rounding and saturation. Output buffer samples are cleared after
 * reading to prepare for the next overlap-add cycle.
 *
 * Return: Zero on success, otherwise an error code.
 */
int stft_process_sink_s16(struct stft_comp_data *cd, struct sof_sink *sink, int frames);

/**
 * stft_process_source_s32() - Copy S32_LE source data to STFT internal buffers.
 * @cd: STFT component data.
 * @source: Source for PCM samples data.
 * @frames: Number of audio data frames to process.
 *
 * De-interleaves S32_LE audio frames from the source circular buffer
 * into per-channel internal circular buffers.
 *
 * Return: Zero on success, otherwise an error code.
 */
int stft_process_source_s32(struct stft_comp_data *cd, struct sof_source *source, int frames);

/**
 * stft_process_sink_s32() - Copy STFT internal buffers to S32_LE sink.
 * @cd: STFT component data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to produce.
 *
 * Interleaves per-channel STFT output buffers into the sink circular
 * buffer in S32_LE format. The output buffer samples are cleared after
 * reading to prepare for the next overlap-add cycle.
 *
 * Return: Zero on success, otherwise an error code.
 */
int stft_process_sink_s32(struct stft_comp_data *cd, struct sof_sink *sink, int frames);

/**
 * stft_process_free_buffers() - Free all STFT processing buffers.
 * @mod: Pointer to processing module.
 *
 * Releases sample buffers, FFT buffers, and FFT/IFFT plans allocated
 * during stft_process_setup().
 */
void stft_process_free_buffers(struct processing_module *mod);

/**
 * stft_process_fill_prev_samples() - Save overlap samples for next STFT frame.
 * @buf: Circular buffer to read overlap samples from.
 * @prev_data: Destination array for the overlap data.
 * @prev_data_length: Number of samples to copy.
 *
 * Copies prev_data_length samples from the circular buffer into the
 * linear prev_data array, handling wrap-around as needed.
 */
void stft_process_fill_prev_samples(struct stft_process_buffer *buf, int32_t *prev_data,
				    int prev_data_length);

/**
 * stft_process_fill_fft_buffer() - Assemble FFT input from overlap and new data.
 * @state: STFT processing state.
 * @ch: Channel index.
 *
 * Constructs the FFT input buffer by concatenating the previous overlap
 * samples and one hop of new samples from the input circular buffer.
 * Imaginary parts are set to zero. The overlap buffer is updated with
 * data for the next frame.
 */
void stft_process_fill_fft_buffer(struct stft_process_state *state, int ch);

/**
 * stft_process_apply_window() - Multiply FFT buffer by the analysis window.
 * @state: STFT processing state that contains the FFT buffer and window.
 *
 * The real part of each complex sample in the FFT buffer is multiplied
 * by the corresponding Q1.31 window coefficient.
 */
void stft_process_apply_window(struct stft_process_state *state);

/**
 * stft_process_overlap_add_ifft_buffer() - Overlap-add IFFT output to circular output buffer.
 * @state: STFT processing state.
 * @ch: Channel index.
 *
 * Each IFFT output sample is multiplied by the gain compensation value
 * and added with saturation to the existing content of the circular
 * output buffer.
 */
void stft_process_overlap_add_ifft_buffer(struct stft_process_state *state, int ch);

#endif //  __SOF_AUDIO_STFT_PROCESS_H__
