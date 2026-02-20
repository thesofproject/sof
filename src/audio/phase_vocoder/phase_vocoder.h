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

#define PHASE_VOCODER_MAX_FRAMES_MARGIN 2		     /* Samples margin for buffer */
#define PHASE_VOCODER_MIN_SPEED_Q29 Q_CONVERT_FLOAT(0.5, 29) /* Min. speed is 0.5 */
#define PHASE_VOCODER_MAX_SPEED_Q29 Q_CONVERT_FLOAT(2.0, 29) /* Max. speed is 2.0 */
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

struct sof_phase_vocoder_config {
	uint32_t size; /**< Size of this struct in bytes */
	uint32_t reserved[8];
	int32_t sample_frequency; /**< Hz. e.g. 16000 */
	int32_t window_gain_comp; /**< Q1.31 gain for IFFT */
	int32_t reserved_32;
	int16_t channel;      /**< -1 expect mono, 0 left, 1 right, ... */
	int16_t frame_length; /**< samples, e.g. 400 for 25 ms @ 16 kHz*/
	int16_t frame_shift;  /**< samples, e.g. 160 for 10 ms @ 16 kHz */
	int16_t reserved_16;
	enum sof_phase_vocoder_fft_pad_type pad;       /**< Use PAD_END, PAD_CENTER, PAD_START */
	enum sof_phase_vocoder_fft_window_type window; /**< Use RECTANGULAR_WINDOW, etc. */
} __attribute__((packed));

struct phase_vocoder_buffer {
	int32_t *addr;
	int32_t *end_addr;
	int32_t *r_ptr;
	int32_t *w_ptr;
	int s_avail;  /**< samples count */
	int s_free;   /**< samples count */
	int s_length; /**< length in samples for wrap */
};

struct phase_vocoder_fft {
	struct icomplex32 *fft_buf; /**< fft_size */
	struct icomplex32 *fft_out; /**< fft_size */
	struct fft_plan *fft_plan;
	struct fft_plan *ifft_plan;
	int fft_size;
	int fft_hop_size;
	int half_fft_size;
	size_t fft_buffer_size; /**< bytes */
};

struct phase_vocoder_polar {
	struct ipolar32 *polar[PLATFORM_MAX_CHANNELS];
	struct ipolar32 *polar_prev[PLATFORM_MAX_CHANNELS];
	struct ipolar32 *polar_tmp;
	int32_t *angle_delta_prev[PLATFORM_MAX_CHANNELS];
	int32_t *angle_delta[PLATFORM_MAX_CHANNELS];
	int32_t *output_phase[PLATFORM_MAX_CHANNELS];
};

struct phase_vocoder_state {
	struct phase_vocoder_buffer ibuf[PLATFORM_MAX_CHANNELS]; /**< Buffer for input data */
	struct phase_vocoder_buffer obuf[PLATFORM_MAX_CHANNELS]; /**< Buffer for output data */
	struct phase_vocoder_fft fft;				 /**< FFT instance, common */
	struct phase_vocoder_polar polar;			 /**< Processing in polar domain */
	int32_t *prev_data[PLATFORM_MAX_CHANNELS];	/**< prev_data_size */
	int32_t *buffers;
	int32_t *window;				/**< fft_size */
	int32_t num_input_fft_to_use;
	int32_t num_input_fft;				/**< Total input FFTs count */
	int32_t num_output_ifft;			/**< Total output IFFTs count */
	int32_t gain_comp;				/**< Gain to compensate window gain */
	int32_t interpolate_fraction;			/**< Q3.29 coefficient */
	int32_t speed;					/**< Q3.29 actual render speed */
	int source_channel;
	int prev_data_size;
	int sample_rate;
	bool first_output_ifft_done;
};

/**
 * struct phase_vocoder_func - Function call pointer for process function
 * @mod: Pointer to module data.
 * @source: Source for PCM samples data.
 * @sink: Sink for PCM samples data.
 * @frames: Number of audio data frames to process.
 */
typedef int (*phase_vocoder_func)(const struct processing_module *mod, struct sof_source *source,
				  struct sof_sink *sink, uint32_t source_frames,
				  uint32_t sink_frames);

/**
 * struct phase_vocoder_comp_data
 * @phase_vocoder_func: Pointer to used processing function.
 * @channels_order[]: Vector with desired sink channels order.
 * @source_format: Source samples format.
 * @frame_bytes: Number of bytes in an audio frame.
 * @channels: Channels count.
 * @enable: Control processing on/off, on - reorder channels
 */
struct phase_vocoder_comp_data {
	phase_vocoder_func phase_vocoder_func; /**< processing function */
	struct phase_vocoder_state state;
	struct sof_phase_vocoder_config *config;
	int32_t speed_ctrl;	/**< Speed Q3.29, allowed range 0.5 to 2.0 */
	int32_t speed_enum;	/**< Speed control from enum 0-15 */
	size_t frame_bytes;
	int source_channel;
	int max_input_frames;
	int max_output_frames;
	int channels;
	bool enable; /**< Processing enable flag */
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
 * phase_vocoder_set_config() - Handle controls get
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

int phase_vocoder_setup(struct processing_module *mod, int rate, int channels);

int phase_vocoder_sink_s16(struct phase_vocoder_comp_data *cd, struct sof_sink *sink, int frames);

int phase_vocoder_sink_s32(struct phase_vocoder_comp_data *cd, struct sof_sink *sink, int frames);

int phase_vocoder_source_s16(struct phase_vocoder_comp_data *cd, struct sof_source *source,
			     int frames);

int phase_vocoder_source_s32(struct phase_vocoder_comp_data *cd, struct sof_source *source,
			     int frames);

#endif //  __SOF_AUDIO_PHASE_VOCODER_H__
