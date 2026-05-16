/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 */
#ifndef __SOF_AUDIO_PHASE_VOCODER_H__
#define __SOF_AUDIO_PHASE_VOCODER_H__

#include <sof/audio/module_adapter/module/generic.h>
#include <sof/math/auditory.h>
#include <sof/math/dct.h>
#include <sof/math/fft.h>

#include <stdbool.h>
#include <stdint.h>

#if CONFIG_LIBRARY
#define STFT_DEBUG 0 /* Keep zero, produces large files with fprintf() */
#else
#define STFT_DEBUG 0
#endif

#define PHASE_VOCODER_MAX_FRAMES_MARGIN 0                                /* Adds to buffer size */
#define PHASE_VOCODER_MIN_SPEED_Q29 Q_CONVERT_FLOAT(0.5, 29)             /* Min. speed is 0.5 */
#define PHASE_VOCODER_MAX_SPEED_Q29 Q_CONVERT_FLOAT(2.0, 29)             /* Max. speed is 2.0 */
#define PHASE_VOCODER_SPEED_STEP_Q31 Q_CONVERT_FLOAT((2 - 0.5) / 15, 31) /* Steps for enum ctrl */
#define PHASE_VOCODER_SPEED_NORMAL Q_CONVERT_FLOAT(1.0, 29)		 /* Default to speed 1 */
#define PHASE_VOCODER_ONE_Q29 Q_CONVERT_FLOAT(1.0, 29)			 /* One as Q29 */
#define PHASE_VOCODER_HALF_Q29 Q_CONVERT_FLOAT(0.5, 29)			 /* 0.5 as Q29 */

#define PHASE_VOCODER_PI_Q28 843314857	    /* int32(pi * 2^28), Q28 */
#define PHASE_VOCODER_TWO_PI_Q28 1686629713 /* int32(2 * pi * 2^28), Q28 */

#define PHASE_VOCODER_PI_Q27 421657428	   /* int32(pi * 2^27) */
#define PHASE_VOCODER_TWO_PI_Q27 843314857 /* int32(2 * pi * 2^27) */

enum sof_phase_vocoder_fft_pad_type {
	STFT_PAD_END = 0,
	STFT_PAD_CENTER = 1,
	STFT_PAD_START = 2,
};

enum sof_phase_vocoder_fft_window_type {
	STFT_RECTANGULAR_WINDOW = 0,
	STFT_BLACKMAN_WINDOW = 1,
	STFT_HAMMING_WINDOW = 2,
	STFT_HANN_WINDOW = 3,
	STFT_POVEY_WINDOW = 4,
};

/**
 * struct sof_phase_vocoder_config - IPC configuration blob for phase vocoder
 * @size: Size of this struct in bytes
 * @reserved: Reserved for future use
 * @sample_frequency: Sample rate in Hz, e.g. 16000
 * @window_gain_comp: Q1.31 gain for IFFT
 * @reserved_32: Reserved for future use
 * @mono: Set to 1 for mono, zero for all channels
 * @frame_length: Frame length in samples, e.g. 400 for 25 ms @ 16 kHz
 * @frame_shift: Frame shift in samples, e.g. 160 for 10 ms @ 16 kHz
 * @reserved_16: Reserved for future use
 * @pad: Padding type, use PAD_END, PAD_CENTER, or PAD_START
 * @window: Window type, use RECTANGULAR_WINDOW, etc.
 */
struct sof_phase_vocoder_config {
	uint32_t size;
	uint32_t reserved[8];
	int32_t sample_frequency;
	int32_t window_gain_comp;
	int32_t reserved_32;
	int16_t mono;
	int16_t frame_length;
	int16_t frame_shift;
	int16_t reserved_16;
	enum sof_phase_vocoder_fft_pad_type pad;
	enum sof_phase_vocoder_fft_window_type window;
} __attribute__((packed));

/**
 * struct phase_vocoder_buffer - Circular buffer for phase vocoder audio data
 * @addr: Start address of the buffer
 * @end_addr: End address of the buffer
 * @r_ptr: Read pointer
 * @w_ptr: Write pointer
 * @s_avail: Available samples count
 * @s_free: Free samples count
 * @s_length: Buffer length in samples for wrap
 */
struct phase_vocoder_buffer {
	int32_t *addr;
	int32_t *end_addr;
	int32_t *r_ptr;
	int32_t *w_ptr;
	int s_avail;
	int s_free;
	int s_length;
};

/**
 * struct phase_vocoder_fft - FFT processing state
 * @fft_buf: FFT input buffer, size is fft_size
 * @fft_out: FFT output buffer, size is fft_size
 * @fft_plan: FFT plan instance
 * @ifft_plan: Inverse FFT plan instance
 * @fft_size: FFT length in samples
 * @fft_hop_size: FFT hop size in samples
 * @half_fft_size: Half of the FFT size
 * @fft_buffer_size: FFT buffer size in bytes
 */
struct phase_vocoder_fft {
	struct icomplex32 *fft_buf;
	struct icomplex32 *fft_out;
	struct fft_plan *fft_plan;
	struct fft_plan *ifft_plan;
	int fft_size;
	int fft_hop_size;
	int half_fft_size;
	size_t fft_buffer_size;
};

/**
 * struct phase_vocoder_polar - Polar domain processing data
 * @polar: Current polar representation per channel
 * @polar_prev: Previous polar representation per channel
 * @polar_tmp: Temporary polar buffer
 * @angle_delta_prev: Previous angle delta per channel
 * @angle_delta: Current angle delta per channel
 * @output_phase: Output phase per channel
 */
struct phase_vocoder_polar {
	struct ipolar32 *polar[PLATFORM_MAX_CHANNELS];
	struct ipolar32 *polar_prev[PLATFORM_MAX_CHANNELS];
	struct ipolar32 *polar_tmp;
	int32_t *angle_delta_prev[PLATFORM_MAX_CHANNELS];
	int32_t *angle_delta[PLATFORM_MAX_CHANNELS];
	int32_t *output_phase[PLATFORM_MAX_CHANNELS];
};

/**
 * struct phase_vocoder_state - Phase vocoder algorithm state
 * @ibuf: Circular buffers for input data, per channel
 * @obuf: Circular buffers for output data, per channel
 * @fft: FFT instance, common for all channels
 * @polar: Processing state in polar domain
 * @prev_data: Previous frame data per channel, size is prev_data_size
 * @buffers: Pointer to allocated memory for all buffers
 * @window: Window function coefficients, size is fft_size
 * @num_input_fft_to_use: Number of input FFTs to use for processing
 * @num_input_fft: Total input FFTs count
 * @num_output_ifft: Total output IFFTs count
 * @gain_comp: Gain to compensate window gain
 * @interpolate_fraction: Q3.29 interpolation coefficient
 * @speed: Q3.29 actual render speed
 * @prev_data_size: Size of previous data buffer
 * @first_output_ifft_done: True after first output IFFT is completed
 */
struct phase_vocoder_state {
	struct phase_vocoder_buffer ibuf[PLATFORM_MAX_CHANNELS];
	struct phase_vocoder_buffer obuf[PLATFORM_MAX_CHANNELS];
	struct phase_vocoder_fft fft;
	struct phase_vocoder_polar polar;
	size_t phase_vocoder_polar_bytes;
	int32_t *prev_data[PLATFORM_MAX_CHANNELS];
	int32_t *buffers;
	int32_t *window;
	int32_t num_input_fft_to_use;
	int32_t num_input_fft;
	int32_t num_output_ifft;
	int32_t gain_comp;
	int32_t interpolate_fraction;
	int32_t speed;
	int prev_data_size;
	bool first_output_ifft_done;
};

/**
 * typedef phase_vocoder_func - Function pointer for process function
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @source_frames: Number of source audio data frames to process.
 * @sink_frames: Number of sink audio data frames to produce.
 */
typedef int (*phase_vocoder_func)(const struct processing_module *mod, struct sof_source *source,
				  struct sof_sink *sink, uint32_t source_frames,
				  uint32_t sink_frames);

/**
 * struct phase_vocoder_comp_data - Phase vocoder component data
 * @phase_vocoder_func: Pointer to used processing function.
 * @state: Phase vocoder algorithm state.
 * @config: Configuration blob for the module.
 * @mono_mix_coef: Gain for channel for mixing, Q1.15
 * @sample_rate: Audio sample rate in Hz
 * @speed_ctrl: Speed Q3.29, allowed range 0.5 to 2.0
 * @speed_enum: Speed control value as enum 0-15
 * @frame_bytes: Number of bytes in an audio frame.
 * @max_input_frames: Maximum number of input frames per processing call
 * @max_output_frames: Maximum number of output frames per processing call
 * @stream_channels: Channels count to consume/produce
 * @process_channels: Channels count to process.
 * @enable: Control processing on/off, off is pass-through
 */
struct phase_vocoder_comp_data {
	phase_vocoder_func phase_vocoder_func;
	struct phase_vocoder_state state;
	struct sof_phase_vocoder_config *config;
	int32_t mono_mix_coef;
	int32_t sample_rate;
	int32_t speed_ctrl;
	int32_t speed_enum;
	size_t frame_bytes;
	int max_input_frames;
	int max_output_frames;
	int stream_channels;
	int process_channels;
	bool enable;
};

static inline int phase_vocoder_buffer_samples_without_wrap(struct phase_vocoder_buffer *buffer,
							    int32_t *ptr)
{
	return buffer->end_addr - ptr;
}

static inline int32_t *phase_vocoder_buffer_wrap(struct phase_vocoder_buffer *buffer, int32_t *ptr)
{
	if (ptr >= buffer->end_addr)
		ptr -= buffer->s_length;

	return ptr;
}

/**
 * struct phase_vocoder_proc_fnmap - processing functions for frame formats
 * @frame_fmt: Current frame format
 * @phase_vocoder_proc_func: Function pointer for the suitable processing function
 */
struct phase_vocoder_proc_fnmap {
	enum sof_ipc_frame frame_fmt;
	phase_vocoder_func phase_vocoder_function;
};

/**
 * phase_vocoder_find_proc_func() - Find suitable processing function.
 * @src_fmt: Enum value for PCM format.
 *
 * This function finds the suitable processing function to use for
 * the used PCM format. If not found, return NULL.
 *
 * Return: Pointer to processing function for the requested PCM format.
 */
phase_vocoder_func phase_vocoder_find_proc_func(enum sof_ipc_frame src_fmt);

#if CONFIG_IPC_MAJOR_4
/**
 * phase_vocoder_set_config() - Handle controls set
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
int phase_vocoder_set_config(struct processing_module *mod, uint32_t param_id,
			     enum module_cfg_fragment_position pos, uint32_t data_offset_size,
			     const uint8_t *fragment, size_t fragment_size, uint8_t *response,
			     size_t response_size);
/**
 * phase_vocoder_get_config() - Handle controls get
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
int phase_vocoder_get_config(struct processing_module *mod, uint32_t config_id,
			     uint32_t *data_offset_size, uint8_t *fragment, size_t fragment_size);
#else
static inline int phase_vocoder_set_config(struct processing_module *mod, uint32_t param_id,
					   enum module_cfg_fragment_position pos,
					   uint32_t data_offset_size, const uint8_t *fragment,
					   size_t fragment_size, uint8_t *response,
					   size_t response_size)
{
	return 0;
}

static inline int phase_vocoder_get_config(struct processing_module *mod, uint32_t config_id,
					   uint32_t *data_offset_size, uint8_t *fragment,
					   size_t fragment_size)
{
	return 0;
}
#endif

void phase_vocoder_apply_window(struct phase_vocoder_state *state);

void phase_vocoder_fill_fft_buffer(struct phase_vocoder_state *state, int ch);

void phase_vocoder_free_buffers(struct processing_module *mod);

int phase_vocoder_overlap_add_ifft_buffer(struct phase_vocoder_state *state, int ch);

void phase_vocoder_reset_for_new_speed(struct phase_vocoder_comp_data *cd);

int phase_vocoder_setup(struct processing_module *mod);

int phase_vocoder_sink_s16(struct phase_vocoder_comp_data *cd, struct sof_sink *sink, int frames);

int phase_vocoder_sink_s32(struct phase_vocoder_comp_data *cd, struct sof_sink *sink, int frames);

int phase_vocoder_source_s16(struct phase_vocoder_comp_data *cd, struct sof_source *source,
			     int frames);

int phase_vocoder_source_s32(struct phase_vocoder_comp_data *cd, struct sof_source *source,
			     int frames);

#endif //  __SOF_AUDIO_PHASE_VOCODER_H__
