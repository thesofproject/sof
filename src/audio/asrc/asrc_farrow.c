// SPDX-License-Identifier: BSD-3-Clause
//
// Copyright(c) 2012-2019 Intel Corporation. All rights reserved.

/* @brief    Implementation of the sample rate converter. */

#include <sof/common.h>
#include <sof/debug/panic.h>
#include <sof/math/numbers.h>
#include <sof/platform.h>
#include <sof/string.h>
#include <sof/trace/trace.h>
#include <sof/audio/format.h>
#include <user/trace.h>
#include <sof/audio/asrc/asrc_config.h>
#include <sof/audio/asrc/asrc_farrow.h>

#define CONVERT_COEFF(x) ((int32_t)(x))

/* Rate skew in Q2.30 format can be 0.5 - 2.0 x rate */
#define SKEW_MIN Q_CONVERT_FLOAT(0.5, 30)
#define SKEW_MAX INT32_MAX /* Near max. 2.0 value */

/* Time value is stored as Q5.27. The latter limit value of 0.0001 is an
 * empirically defined limit to issue warning of possibly non-successful
 * processing.
 */
#define TIME_VALUE_ONE Q_CONVERT_FLOAT(1, 27)
#define TIME_VALUE_LIMIT Q_CONVERT_FLOAT(0.0001, 27)

/* Generic scale value for Q27 fractional arithmetic */
#define ONE_Q27 Q_CONVERT_FLOAT(1, 27)

/*
 * GLOBALS
 */

/*
 * asrc_conversion_ratios: Enum which lists all conversion ratios with
 * varying coefficient sets.  E.g. CR_44100TO48000 covers the
 * coefficients for most upsample use cases.
 */
enum asrc_conversion_ratios {
	/* mandatory, used for many use cases with fs_in <= fs_out */
	CR_44100TO48000,
	/* mandatory, used for all use cases with fs_in = 48000 Hz <= fs_out */
	CR_48000TO48000,
#if (CONFIG_ASRC_SUPPORT_CONVERSION_24000_TO_08000)
	CR_24000TO08000,
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_24000_TO_16000)
	CR_24000TO16000,
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_08000)
	CR_48000TO08000,
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_11025)
	CR_48000TO11025,
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_12000)
	CR_48000TO12000,
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_16000)
	CR_48000TO16000,
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_22050)
	CR_48000TO22050,
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_24000)
	CR_48000TO24000,
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_32000)
	CR_48000TO32000,
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_44100)
	CR_48000TO44100,
#endif
	CR_NUM
};

/*
 * Filter_params:
 */
struct asrc_filter_params {
	int filter_length;	/*<! Length of each polyphase filter */
	int num_filters;	/*<! Number of polyphase filters */
};

/*
 * c_filter_params: Stores the filter parameters for every use case.
 * Filter parameters consist of the filter length and the number of
 * filters.
 */
static const struct asrc_filter_params c_filter_params[CR_NUM] = {
#include <sof/audio/coefficients/asrc/asrc_farrow_param_44100Hz_to_48000Hz.h>
#include <sof/audio/coefficients/asrc/asrc_farrow_param_48000Hz_to_48000Hz.h>

#if (CONFIG_ASRC_SUPPORT_CONVERSION_24000_TO_08000)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_24000Hz_to_08000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_24000_TO_16000)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_24000Hz_to_16000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_08000)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_48000Hz_to_08000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_11025)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_48000Hz_to_11025Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_12000)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_48000Hz_to_12000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_16000)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_48000Hz_to_16000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_22050)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_48000Hz_to_22050Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_24000)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_48000Hz_to_24000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_32000)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_48000Hz_to_32000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_44100)
#include <sof/audio/coefficients/asrc/asrc_farrow_param_48000Hz_to_44100Hz.h>
#endif

};

/*
 * Coefficients for each use case are included here The corresponding
 * coefficients will be attached to the _Src_farrow struct via the
 * initialise_filter function.
 */
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_44100Hz_to_48000Hz.h>

#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_48000Hz_to_48000Hz.h>

#if (CONFIG_ASRC_SUPPORT_CONVERSION_24000_TO_08000)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_24000Hz_to_08000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_24000_TO_16000)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_24000Hz_to_16000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_08000)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_48000Hz_to_08000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_11025)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_48000Hz_to_11025Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_12000)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_48000Hz_to_12000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_16000)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_48000Hz_to_16000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_22050)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_48000Hz_to_22050Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_24000)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_48000Hz_to_24000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_32000)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_48000Hz_to_32000Hz.h>
#endif

#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_44100)
#include <sof/audio/coefficients/asrc/asrc_farrow_coeff_48000Hz_to_44100Hz.h>
#endif

/*
 * FUNCTION DECLARATIONS
 */

/*
 * Initialises the pointers to the buffers and zeroes their content
 */
static enum asrc_error_code initialise_buffer(struct comp_dev *dev,
					      struct asrc_farrow *src_obj);

/*
 * Initialise the pointers to the filters, set the number of filters
 * and their length
 */
static enum asrc_error_code initialise_filter(struct comp_dev *dev,
					      struct asrc_farrow *src_obj);

/*
 * FUNCTION DEFINITIONS + GENERAL SETUP
 */

/*
 * The internal data will be stored as follows:
 * Address:                      |Item:                                |
 * ------------------------------|-------------------------------------|
 * 0x0000                        |asrc_farrow src_obj                  |
 * ------------------------------|-------------------------------------|
 * &src_obj + 1                  |int32 impulse_response[filter_length]|
 * ------------------------------|-------------------------------------|
 * &impulse_response[0] +        |int_x *buffer_pointer[num_channels]  |
 * filter_length                 |                                     |
 * ------------------------------|-------------------------------------|
 * &buffer_pointer[0] +          | int_x ring_buffer[num_channels *    |
 * + num_channels*sizeof(int_x *)|               *buffer_size]         |
 * ------------------------------|-------------------------------------|
 *
 * Info:
 *
 * buffer_pointer[num_channels]:
 * Pointers to each channels data stored internally. ring_buffers_x points to
 * the first of these pointers.
 *
 * int_x ring_buffer[num_channels * buffer_size]:
 * This is where the actual input data is buffered.
 */

enum asrc_error_code asrc_get_required_size(struct comp_dev *dev,
					    int *required_size,
					    int num_channels,
					    int bit_depth)
{
	int size;

	/* check for parameter errors */
	if (!required_size) {
		comp_err(dev, "asrc_get_required_size(), invalid required_size");
		return ASRC_EC_INVALID_POINTER;
	}

	if (num_channels < 1) {
		comp_err(dev, "asrc_get_required_size(), invalid num_channels = %d",
			 num_channels);
		return ASRC_EC_INVALID_NUM_CHANNELS;
	}

	if (bit_depth != 16 && bit_depth != 32) {
		comp_err(dev, "asrc_get_required_size_bytes(), invalid bit_depth = %d",
			 bit_depth);
		return ASRC_EC_INVALID_BIT_DEPTH;
	}

	/*
	 * At the current stage, memory is always allocated for the
	 * worst case scenario. Namely for the 48000Hz to 8000Hz use
	 * case, which corresponds to a filter length of 128. This
	 * allows switching between conversion ratios on the fly,
	 * without having to reallocate memory for the asrc_farrow
	 * instance and reinitialise the pointers.  If on the fly
	 * changes are not necessary, this function can be changed to
	 * give the minimum size needed for a certain conversion
	 * ratio.
	 */

	/* accumulate the size */
	size = sizeof(struct asrc_farrow);

	/* size of the impulse response */
	size += ASRC_MAX_FILTER_LENGTH * sizeof(int32_t);

	/* size of pointers to the buffers */
	size += sizeof(int32_t *) * num_channels;

	/* size of the ring buffers */
	size += ASRC_MAX_BUFFER_LENGTH * num_channels * (bit_depth / 8);

	*required_size = size;
	return ASRC_EC_OK;
}

enum asrc_error_code asrc_initialise(struct comp_dev *dev,
				     struct asrc_farrow *src_obj,
				     int num_channels,
				     int fs_prim,
				     int fs_sec,
				     enum asrc_io_format input_format,
				     enum asrc_io_format output_format,
				     enum asrc_buffer_mode buffer_mode,
				     int buffer_length,
				     int bit_depth,
				     enum asrc_control_mode control_mode,
				     enum asrc_operation_mode operation_mode)
{
	enum asrc_error_code error_code;

	/* check for parameter errors */
	if (!src_obj) {
		comp_err(dev, "asrc_initialise(), null src_obj");
		return ASRC_EC_INVALID_POINTER;
	}

	if (num_channels < 1) {
		comp_err(dev, "asrc_initialise(), num_channels = %d",
			 num_channels);
		return ASRC_EC_INVALID_NUM_CHANNELS;
	}

	if (fs_prim < 8000 || fs_prim > 192000) {
		comp_err(dev, "asrc_initialise(), fs_prim = %d", fs_prim);
		return ASRC_EC_INVALID_SAMPLE_RATE;
	}

	if (fs_sec < 8000 || fs_sec > 192000) {
		comp_err(dev, "asrc_initialise(), fs_src = %d", fs_sec);
		return ASRC_EC_INVALID_SAMPLE_RATE;
	}

	if (buffer_length < 2) {
		comp_err(dev, "asrc_initialise(), buffer_length = %d",
			 buffer_length);
		return ASRC_EC_INVALID_BUFFER_LENGTH;
	}

	if (bit_depth != 16 && bit_depth != 32) {
		comp_err(dev, "asrc_initialise(), bit_depth = %d",
			 bit_depth);
		return ASRC_EC_INVALID_BIT_DEPTH;
	}

	/* set up program values */
	src_obj->input_format = input_format;
	src_obj->output_format = output_format;
	src_obj->bit_depth = bit_depth;
	src_obj->io_buffer_mode = buffer_mode;
	src_obj->io_buffer_idx = 0;
	src_obj->io_buffer_length = buffer_length;
	src_obj->num_channels = num_channels;
	src_obj->fs_prim = fs_prim;
	src_obj->fs_sec = fs_sec;
	src_obj->fs_ratio = ((int64_t)ONE_Q27 * src_obj->fs_prim) /
		src_obj->fs_sec;    /* stored as Q5.27 fixed point value */
	src_obj->fs_ratio_inv = ((int64_t)ONE_Q27 * src_obj->fs_sec) /
		src_obj->fs_prim; /* stored as Q5.27 fixed point value */
	src_obj->time_value = 0;
	src_obj->time_value_pull = 0;
	src_obj->control_mode = control_mode;
	src_obj->operation_mode = operation_mode;
	src_obj->prim_num_frames = 0;
	src_obj->prim_num_frames_targ = 0;
	src_obj->sec_num_frames = 0;
	src_obj->sec_num_frames_targ = 0;
	src_obj->calc_ir = NULL;
	src_obj->is_initialised = false;
	src_obj->is_updated = false;

	/* check conversion ratios */
	if (src_obj->fs_ratio == 0 || src_obj->fs_ratio_inv == 0) {
		comp_err(dev, "asrc_initialise(), fail to calculate ratios");
		return ASRC_EC_INVALID_CONVERSION_RATIO;
	}

	/*
	 * Set the pointer for the impulse response. It is just after
	 * src_obj in memory.
	 */
	src_obj->impulse_response = (int32_t *)(src_obj + 1);

	/*
	 * Load the filter coefficients and parameters.  This function
	 * also sets the pointer to the corresponding
	 * calc_impulse_response_nX function.
	 */
	error_code = initialise_filter(dev, src_obj);

	/* check for errors */
	if (error_code != ASRC_EC_OK) {
		comp_err(dev, "asrc_initialise(), failed filter initialise");
		return error_code;
	}

	/*
	 * The pointer to the internal ring buffer pointers is
	 * after impulse_response. Only one of the buffers is initialised,
	 * depending on the specified bit depth.
	 */
	if (src_obj->bit_depth == 32) {
		src_obj->ring_buffers16 = NULL;
		src_obj->ring_buffers32 = (int32_t **)(src_obj->impulse_response +
			src_obj->filter_length);
	} else if (src_obj->bit_depth == 16) {
		src_obj->ring_buffers32 = NULL;
		src_obj->ring_buffers16 = (int16_t **)(src_obj->impulse_response +
			src_obj->filter_length);
	}

	/* set the channel pointers and fill buffers with zeros */
	error_code = initialise_buffer(dev, src_obj);

	/* check for errors */
	if (error_code != ASRC_EC_OK) {
		comp_err(dev, "asrc_initialise(), failed buffer initialise");
		return error_code;
	}

	/* return ok, if everything worked out */
	src_obj->is_initialised = true;
	return ASRC_EC_OK;
}

enum asrc_error_code asrc_set_fs_ratio(struct comp_dev *dev,
				       struct asrc_farrow *src_obj,
				       int32_t fs_prim, int32_t fs_sec)
{
	/* Check for parameter errors */
	if (!src_obj) {
		comp_err(dev, "asrc_set_fs_ratio(), null src_obj");
		return ASRC_EC_INVALID_POINTER;
	}

	if (!src_obj->is_initialised) {
		comp_err(dev, "asrc_set_fs_ratio(), not initialised");
		return ASRC_EC_INIT_FAILED;
	}

	if (fs_prim < 8000 || fs_prim > 192000) {
		comp_err(dev, "asrc_set_fs_ratio(), fs_prim = %d", fs_prim);
		return ASRC_EC_INVALID_SAMPLE_RATE;
	}

	if (fs_sec < 8000 || fs_sec > 192000) {
		comp_err(dev, "asrc_set_fs_ratio(), fs_sec = %d", fs_sec);
		return ASRC_EC_INVALID_SAMPLE_RATE;
	}

	/* set sampling rates */
	src_obj->fs_prim = fs_prim;
	src_obj->fs_sec = fs_sec;

	/* calculate conversion ratios as Q5.27. Note that ratio
	 * 192 / 8 = 24 is beyond signed 32 bit fractional integer
	 * but since the type for fs_ratio is unsigned, the ratio
	 * can be up to 31.9999... (but not 32).
	 */
	src_obj->fs_ratio = ((int64_t)ONE_Q27 * src_obj->fs_prim) /
		src_obj->fs_sec;
	src_obj->fs_ratio_inv = ((int64_t)ONE_Q27 * src_obj->fs_sec) /
		src_obj->fs_prim;

	/* check conversion ratios */
	if (src_obj->fs_ratio == 0 || src_obj->fs_ratio_inv == 0) {
		comp_err(dev, "asrc_set_fs_ratio(), failed to calculate ratios");
		return ASRC_EC_INVALID_CONVERSION_RATIO;
	}

	/* Reset the t values */
	src_obj->time_value = 0;
	src_obj->time_value_pull = 0;

	/* See initialise_asrc(...) for further information
	 * Update the filters accordingly
	 */
	enum asrc_error_code error_code = initialise_filter(dev, src_obj);
	/* check for errors */
	if (error_code != ASRC_EC_OK) {
		comp_err(dev, "asrc_set_fs_ratio(), failed filter initialise");
		return error_code;
	}

	/* Set the channel pointers and zero the buffers */
	error_code = initialise_buffer(dev, src_obj);
	/* check for errors */
	if (error_code != ASRC_EC_OK) {
		comp_err(dev, "asrc_set_fs_ratio(), failed buffer initialise");
		return error_code;
	}

	/* Set the pointer for the impulse response */
	if (src_obj->bit_depth == 32) {
		src_obj->impulse_response =
			(int32_t *)(src_obj->ring_buffers32 +
			src_obj->num_channels * (1 + src_obj->buffer_length));
	} else if (src_obj->bit_depth == 16) {
		src_obj->impulse_response =
			(int32_t *)(src_obj->ring_buffers16 +
				    src_obj->num_channels *
				    (1 + src_obj->buffer_length / 2));
	}

	return ASRC_EC_OK;
}

enum asrc_error_code asrc_set_input_format(struct comp_dev *dev,
					   struct asrc_farrow *src_obj,
					   enum asrc_io_format input_format)
{
	/* check for parameter errors */
	if (!src_obj) {
		comp_err(dev, "asrc_set_input_format(), null src_obj");
		return ASRC_EC_INVALID_POINTER;
	}

	if (!src_obj->is_initialised) {
		comp_err(dev, "asrc_set_input_format(), not initialised");
		return ASRC_EC_INIT_FAILED;
	}

	/* See header for further information */
	src_obj->input_format = input_format;
	return ASRC_EC_OK;
}

enum asrc_error_code asrc_set_output_format(struct comp_dev *dev,
					    struct asrc_farrow *src_obj,
					    enum asrc_io_format output_format)
{
	/* check for parameter errors */
	if (!src_obj) {
		comp_err(dev, "asrc_set_output_format(), null src_obj");
		return ASRC_EC_INVALID_POINTER;
	}

	if (!src_obj->is_initialised) {
		comp_err(dev, "asrc_set_output_format(), not initialised");
		return ASRC_EC_INIT_FAILED;
	}

	/* See header for further information */
	src_obj->output_format = output_format;
	return ASRC_EC_OK;
}

/*
 * BUFFER FUNCTIONS
 */
static enum asrc_error_code initialise_buffer(struct comp_dev *dev,
					      struct asrc_farrow *src_obj)
{
	int32_t *start_32;
	int16_t *start_16;
	int ch;

	/*
	 * Set buffer_length to filter_length * 2 to compensate for
	 * missing element wise wrap around while loading but allowing
	 * aligned loads.
	 */
	src_obj->buffer_length = src_obj->filter_length * 2;
	if (src_obj->buffer_length > ASRC_MAX_BUFFER_LENGTH) {
		comp_err(dev, "initialise_buffer(), buffer_length %d exceeds max.",
			 src_obj->buffer_length);
		return ASRC_EC_INVALID_BUFFER_LENGTH;
	}

	src_obj->buffer_write_position = src_obj->filter_length;

	/*
	 * Initialize the dynamically allocated 2D array and clear the
	 * buffers to zero.
	 */
	if (src_obj->bit_depth == 32) {
		start_32 = (int32_t *)(src_obj->ring_buffers32 +
			src_obj->num_channels);
		for (ch = 0; ch < src_obj->num_channels; ch++)
			src_obj->ring_buffers32[ch] = start_32 +
				ch * src_obj->buffer_length;

		/* initialise to zero */
		memset(start_32, 0, src_obj->num_channels *
			src_obj->buffer_length * sizeof(int32_t));
	} else {
		start_16 = (int16_t *)(src_obj->ring_buffers16 +
			src_obj->num_channels);
		for (ch = 0; ch < src_obj->num_channels; ch++)
			src_obj->ring_buffers16[ch] = start_16 +
				ch * src_obj->buffer_length;

		/* initialise to zero */
		memset(start_16, 0, src_obj->num_channels *
			src_obj->buffer_length * sizeof(int16_t));
	}

	return ASRC_EC_OK;
}

/*
 * FILTER FUNCTIONS
 */
static enum asrc_error_code initialise_filter(struct comp_dev *dev,
					      struct asrc_farrow *src_obj)
{
	int fs_in;
	int fs_out;

	if (src_obj->operation_mode == ASRC_OM_PUSH) {
		fs_in = src_obj->fs_prim;
		fs_out = src_obj->fs_sec;
	} else {
		fs_in = src_obj->fs_sec;
		fs_out = src_obj->fs_prim;
	}

	/*
	 * Store the filter length and number of filters from the
	 * c_filter_params constant in the src_obj. Also set the pointer
	 * to the polyphase filters coefficients.
	 */

	/* Reset coefficients for possible exit with error. */
	src_obj->filter_length = 0;
	src_obj->num_filters = 0;
	src_obj->polyphase_filters = NULL;

	if (fs_in == 0 || fs_out == 0) {
		/* Avoid possible divisions by zero. */
		comp_err(dev, "initialise_filter(), fs_in = %d, fs_out = %d",
			 fs_in, fs_out);
		return ASRC_EC_INVALID_SAMPLE_RATE;
	} else if (fs_in == 48000 && fs_out >= 48000) {
		/* For conversion from 48 kHz to 48 kHz (and above) we
		 * can apply shorter filters (M=48), due to the soft
		 * slope (transition bandwidth from 20 kHz to 24 kHz).
		 */
		src_obj->filter_length =
			c_filter_params[CR_48000TO48000].filter_length;
		src_obj->num_filters =
			c_filter_params[CR_48000TO48000].num_filters;
		src_obj->polyphase_filters = &coeff48000to48000[0];
	} else if (fs_in <= fs_out) {
		/* All upsampling use cases can share the same set of
		 * filter coefficients.
		 */
		src_obj->filter_length =
			c_filter_params[CR_44100TO48000].filter_length;
		src_obj->num_filters =
			c_filter_params[CR_44100TO48000].num_filters;
		src_obj->polyphase_filters = &coeff44100to48000[0];
	} else if (fs_in == 48000) {
		switch (fs_out) {
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_08000)
		case 8000:
			src_obj->filter_length =
				c_filter_params[CR_48000TO08000].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_48000TO08000].num_filters;
			src_obj->polyphase_filters = &coeff48000to08000[0];
			break;
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_11025)
		case 11025:
			src_obj->filter_length =
				c_filter_params[CR_48000TO11025].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_48000TO11025].num_filters;
			src_obj->polyphase_filters = &coeff48000to11025[0];
			break;
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_12000)
		case 12000:
			src_obj->filter_length =
				c_filter_params[CR_48000TO12000].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_48000TO12000].num_filters;
			src_obj->polyphase_filters = &coeff48000to12000[0];
			break;
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_16000)
		case 16000:
			src_obj->filter_length =
				c_filter_params[CR_48000TO16000].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_48000TO16000].num_filters;
			src_obj->polyphase_filters = &coeff48000to16000[0];
			break;
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_22050)
		case 22050:
			src_obj->filter_length =
				c_filter_params[CR_48000TO22050].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_48000TO22050].num_filters;
			src_obj->polyphase_filters = &coeff48000to22050[0];
			break;
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_24000)
		case 24000:
			src_obj->filter_length =
				c_filter_params[CR_48000TO24000].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_48000TO24000].num_filters;
			src_obj->polyphase_filters = &coeff48000to24000[0];
			break;
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_32000)
		case 32000:
			src_obj->filter_length =
				c_filter_params[CR_48000TO32000].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_48000TO32000].num_filters;
			src_obj->polyphase_filters = &coeff48000to32000[0];
			break;
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_48000_TO_44100)
		case 44100:
			src_obj->filter_length =
				c_filter_params[CR_48000TO44100].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_48000TO44100].num_filters;
			src_obj->polyphase_filters = &coeff48000to44100[0];
			break;
#endif
		default:
			comp_err(dev, "initialise_filter(), fs_out = %d",
				 fs_out);
			return ASRC_EC_INVALID_SAMPLE_RATE;
		}
	} else if (fs_in == 24000) {
		switch (fs_out) {
#if (CONFIG_ASRC_SUPPORT_CONVERSION_24000_TO_08000)
		case 8000:
			src_obj->filter_length =
				c_filter_params[CR_24000TO08000].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_24000TO08000].num_filters;
			src_obj->polyphase_filters = &coeff24000to08000[0];
			break;
#endif
#if (CONFIG_ASRC_SUPPORT_CONVERSION_24000_TO_16000)
		case 16000:
			src_obj->filter_length =
				c_filter_params[CR_24000TO16000].filter_length;
			src_obj->num_filters =
				c_filter_params[CR_24000TO16000].num_filters;
			src_obj->polyphase_filters = &coeff24000to16000[0];
			break;
#endif
		default:
			comp_err(dev, "initialise_filter(), fs_out = %d",
				 fs_out);
			return ASRC_EC_INVALID_SAMPLE_RATE;
		}
	} else {
		/* Conversion ratio is not supported. */
		comp_err(dev, "initialise_filter(), fs_in = %d", fs_in);
		return ASRC_EC_INVALID_SAMPLE_RATE;
	}

	/* Check that filter length does not exceed allocated */
	if (src_obj->filter_length > ASRC_MAX_FILTER_LENGTH) {
		comp_err(dev, "initialise_filter(), filter_length %d exceeds max",
			 src_obj->filter_length);
		return ASRC_EC_INVALID_FILTER_LENGTH;
	}

	/* The function pointer is set according to the number of polyphase
	 * filters
	 */
	switch (src_obj->num_filters) {
	case 4:
		src_obj->calc_ir = &asrc_calc_impulse_response_n4;
		break;
	case 5:
		src_obj->calc_ir = &asrc_calc_impulse_response_n5;
		break;
	case 6:
		src_obj->calc_ir = &asrc_calc_impulse_response_n6;
		break;
	case 7:
		src_obj->calc_ir = &asrc_calc_impulse_response_n7;
		break;
	default:
		comp_err(dev, "initialise_filter(), num_filters = %d",
			 src_obj->num_filters);
		return ASRC_EC_INVALID_CONVERSION_RATIO;
	}

	return ASRC_EC_OK;
}

enum asrc_error_code asrc_update_drift(struct comp_dev *dev,
				       struct asrc_farrow *src_obj,
				       uint32_t clock_skew)
{
	uint32_t fs_ratio;
	uint32_t fs_ratio_inv;

	/* check for parameter errors */
	if (!src_obj) {
		comp_err(dev, "asrc_update_drift(), null src_obj");
		return ASRC_EC_INVALID_POINTER;
	}

	if (!src_obj->is_initialised) {
		comp_err(dev, "asrc_update_drift(), not initialised");
		return ASRC_EC_INIT_FAILED;
	}

	if (src_obj->control_mode != ASRC_CM_FEEDBACK) {
		comp_err(dev, "update_drift(), need to use feedback mode");
		return ASRC_EC_INVALID_CONTROL_MODE;
	}

	/* Skew is Q2.30 */
	if (clock_skew < SKEW_MIN || clock_skew > SKEW_MAX) {
		comp_err(dev, "asrc_update_drift(), clock_skew = %d",
			 clock_skew);
		return ASRC_EC_INVALID_CLOCK_SKEW;
	}

	/* Update the fs ratios, result is Q5.27 */
	fs_ratio = ((uint64_t)ONE_Q27 * src_obj->fs_prim) /
		src_obj->fs_sec;
	fs_ratio_inv = ((uint64_t)ONE_Q27 * src_obj->fs_sec) /
		src_obj->fs_prim;

	/* Q5.27 x Q2.30 -> Q7.57, shift right by 30 to get Q5.27 */
	src_obj->fs_ratio = (uint32_t)(((uint64_t)fs_ratio * clock_skew) >> 30);

	/* Scale Q5.27 to Q5.54, scale Q2.30 to Q2.27,
	 * then Q5.54 / Q2.27 -> Q5.27
	 */
	src_obj->fs_ratio_inv = (uint32_t)(((uint64_t)fs_ratio_inv << 27) /
		(uint64_t)(clock_skew >> 3));

	return ASRC_EC_OK;
}

enum asrc_error_code asrc_update_fs_ratio(struct comp_dev *dev,
					  struct asrc_farrow *src_obj,
					  int primary_num_frames,
					  int secondary_num_frames)
{
	uint64_t tmp_fs_ratio;

	/* Check input for errors */
	if (!src_obj) {
		comp_err(dev, "asrc_update_fs_ratio(), null src_obj");
		return ASRC_EC_INVALID_POINTER;
	}

	if (!src_obj->is_initialised) {
		comp_err(dev, "asrc_update_fs_ratio(), not initialized");
		return ASRC_EC_INIT_FAILED;
	}

	if (src_obj->control_mode != ASRC_CM_FIXED) {
		comp_err(dev, "update_fs_ratio(), need to use fixed mode");
		return ASRC_EC_INVALID_CONTROL_MODE;
	}

	if (primary_num_frames < 1 || secondary_num_frames < 1) {
		comp_err(dev, "update_fs_ratio(), primary_num_frames = %d, secondary_num_frames = %d",
			 primary_num_frames, secondary_num_frames);
		return ASRC_EC_INVALID_FRAME_SIZE;
	}

	/* Calculate the new ratios as Q5.27 */
	tmp_fs_ratio = ((uint64_t)primary_num_frames) << 27;
	src_obj->fs_ratio = (tmp_fs_ratio / secondary_num_frames) + 1;
	tmp_fs_ratio = ((uint64_t)secondary_num_frames) << 27;
	src_obj->fs_ratio_inv = (tmp_fs_ratio / primary_num_frames) + 1;

	/* Reset the counter and set the new target level */
	src_obj->prim_num_frames = 0;
	src_obj->prim_num_frames_targ = primary_num_frames;
	src_obj->sec_num_frames = 0;
	src_obj->sec_num_frames_targ = secondary_num_frames;

	/* Reset the timer values, since we want start a new control cycle. */
	src_obj->time_value = 0;
	src_obj->time_value_pull = 0;

	/* set updated status */
	src_obj->is_updated = true;

	return ASRC_EC_OK;
}

void asrc_write_to_ring_buffer16(struct asrc_farrow  *src_obj,
				 int16_t **input_buffers, int index_input_frame)
{
	int ch;
	int j;
	int k;
	int m;

	/* update the buffer_write_position */
	(src_obj->buffer_write_position)++;

	/* since it's a ring buffer we need a wrap around */
	if (src_obj->buffer_write_position >= src_obj->buffer_length)
		src_obj->buffer_write_position -= (src_obj->buffer_length >> 1);

	/* handle input format */
	if (src_obj->input_format == ASRC_IOF_INTERLEAVED)
		m = src_obj->num_channels * index_input_frame;
	else
		m = index_input_frame; /* For SRC_IOF_DEINTERLEAVED */

	/* write data to each channel */
	j = src_obj->buffer_write_position;
	k = j - (src_obj->buffer_length >> 1);
	for (ch = 0; ch < src_obj->num_channels; ch++) {
		/*
		 * Since we want the filter function to load 64 bit of
		 * buffer data in one cycle, this function writes each
		 * input sample to the buffer twice, one with an
		 * offset of half the buffer size. This way we don't
		 * need a wrap around while loading #filter_length of
		 * buffered samples. The upper and lower half of the
		 * buffer are redundant. If the memory tradeoff is
		 * critical, the buffer can be reduced to half the
		 * size but therefore increased filter operations have
		 * to be expected.
		 */
		src_obj->ring_buffers16[ch][j] = input_buffers[ch][m];
		src_obj->ring_buffers16[ch][k] = input_buffers[ch][m];
	}
}

void asrc_write_to_ring_buffer32(struct asrc_farrow  *src_obj,
				 int32_t **input_buffers, int index_input_frame)
{
	int ch;
	int j;
	int k;
	int m;

	/* update the buffer_write_position */
	(src_obj->buffer_write_position)++;

	/* since it's a ring buffer we need a wrap around */
	if (src_obj->buffer_write_position >= src_obj->buffer_length)
		src_obj->buffer_write_position -= (src_obj->buffer_length >> 1);

	/* handle input format */
	if (src_obj->input_format == ASRC_IOF_INTERLEAVED)
		m = src_obj->num_channels * index_input_frame;
	else
		m = index_input_frame; /* For SRC_IOF_DEINTERLEAVED */

	/* write data to each channel */
	j = src_obj->buffer_write_position;
	k = j - (src_obj->buffer_length >> 1);
	for (ch = 0; ch < src_obj->num_channels; ch++) {
		/*
		 * Since we want the filter function to load 64 bit of
		 * buffer data in one cycle, this function writes each
		 * input sample to the buffer twice, one with an
		 * offset of half the buffer size. This way we don't
		 * need a wrap around while loading #filter_length of
		 * buffered samples. The upper and lower half of the
		 * buffer are redundant. If the memory tradeoff is
		 * critical, the buffer can be reduced to half the
		 * size but therefore increased filter operations have
		 * to be expected.
		 */
		src_obj->ring_buffers32[ch][j] = input_buffers[ch][m];
		src_obj->ring_buffers32[ch][k] = input_buffers[ch][m];
	}
}

enum asrc_error_code asrc_process_push16(struct comp_dev *dev,
					 struct asrc_farrow *src_obj,
					 int16_t **__restrict input_buffers,
					 int *input_num_frames,
					 int16_t **__restrict output_buffers,
					 int *output_num_frames,
					 int *write_index,
					 int read_index)
{
	int index_input_frame;
	int max_num_free_frames;

	/* parameter error handling */
	if (!src_obj || !input_buffers || !output_buffers ||
	    !output_num_frames || !write_index) {
		return ASRC_EC_INVALID_POINTER;
	}

	if (!src_obj->is_initialised)
		return ASRC_EC_INIT_FAILED;

	if (src_obj->control_mode == ASRC_CM_FIXED && !src_obj->is_updated)
		return ASRC_EC_UPDATE_FS_FAILED;

	if (src_obj->io_buffer_mode == ASRC_BM_LINEAR) {
		/* Write to the beginning of the output buffer */
		src_obj->io_buffer_idx = 0;
		max_num_free_frames = src_obj->io_buffer_length;
	} else {
		/* circular */
		if (read_index > *write_index)
			max_num_free_frames = read_index - *write_index;
		else
			max_num_free_frames = src_obj->io_buffer_length +
				read_index - *write_index;
	}

	*output_num_frames = 0;
	index_input_frame = 0;

	/* Run the state machine until all input samples are read */
	while (index_input_frame < *input_num_frames) {
		if (src_obj->time_value < TIME_VALUE_ONE) {
			if (*output_num_frames == max_num_free_frames)
				break;

			/* Calculate impulse response */
			(*src_obj->calc_ir)(src_obj);

			/* Filter and write one output sample for each
			 * channel to the output_buffer
			 */
			asrc_fir_filter16(src_obj, output_buffers,
					  src_obj->io_buffer_idx);

			/* Update time and buffer index */
			src_obj->time_value += src_obj->fs_ratio;
			src_obj->io_buffer_idx++;

			/* Wrap around */
			if (src_obj->io_buffer_mode == ASRC_BM_CIRCULAR &&
			    src_obj->io_buffer_idx >= src_obj->io_buffer_length)
				src_obj->io_buffer_idx = 0;

			(*output_num_frames)++;
		} else {
			/* Consume one input sample */
			asrc_write_to_ring_buffer16(src_obj, input_buffers,
						    index_input_frame);
			index_input_frame++;

			/* Update time */
			src_obj->time_value -= TIME_VALUE_ONE;
		}
	}
	*write_index = src_obj->io_buffer_idx;
	*input_num_frames = index_input_frame;

	/* if fixed control mode, update the frames counters and check
	 * if we have reached end of control cycle.
	 */
	if (src_obj->control_mode == ASRC_CM_FIXED) {
		src_obj->prim_num_frames += *input_num_frames;
		src_obj->sec_num_frames += *output_num_frames;
		if (src_obj->prim_num_frames >= src_obj->prim_num_frames_targ &&
		    src_obj->sec_num_frames >= src_obj->sec_num_frames_targ) {
			if (src_obj->sec_num_frames !=
			    src_obj->sec_num_frames_targ) {
				comp_err(dev, "process_push16(), Generated = %d, Target = %d",
					 src_obj->sec_num_frames,
					 src_obj->sec_num_frames_targ);
				return ASRC_EC_FAILED_PUSH_MODE;
			}

			if (src_obj->time_value > TIME_VALUE_LIMIT) {
				comp_err(dev, "process_push16(), Time value = %d",
					 src_obj->time_value);
				return ASRC_EC_FAILED_PUSH_MODE;
			}

			/* Force that update_fs_ratio() will be called
			 * (this will reset the time value).
			 */
			src_obj->is_updated = false;
		}
	}

	return ASRC_EC_OK;
}

enum asrc_error_code asrc_process_push32(struct comp_dev *dev,
					 struct asrc_farrow *src_obj,
					 int32_t **__restrict input_buffers,
					 int *input_num_frames,
					 int32_t **__restrict output_buffers,
					 int *output_num_frames,
					 int *write_index,
					 int read_index)
{
	/* See 'process_push16' for a more detailed description of the
	 * algorithm
	 */
	int index_input_frame;
	int max_num_free_frames;

	/* parameter error handling */
	if (!src_obj || !input_buffers || !output_buffers ||
	    !output_num_frames || !write_index)
		return ASRC_EC_INVALID_POINTER;

	if (!src_obj->is_initialised)
		return ASRC_EC_INIT_FAILED;

	if (src_obj->control_mode == ASRC_CM_FIXED && !src_obj->is_updated)
		return ASRC_EC_UPDATE_FS_FAILED;

	if (src_obj->io_buffer_mode == ASRC_BM_LINEAR) {
		src_obj->io_buffer_idx = 0;
		max_num_free_frames = src_obj->io_buffer_length;
	} else {
		if (read_index > *write_index)
			max_num_free_frames = read_index - *write_index;
		else
			max_num_free_frames = src_obj->io_buffer_length +
				read_index - *write_index;
	}

	*output_num_frames = 0;
	index_input_frame = 0;
	while (index_input_frame < *input_num_frames) {
		if (src_obj->time_value < TIME_VALUE_ONE) {
			if (*output_num_frames == max_num_free_frames)
				break;

			/* Calculate impulse response */
			(*src_obj->calc_ir)(src_obj);

			/* Filter and write output sample to
			 * output_buffer
			 */
			asrc_fir_filter32(src_obj, output_buffers,
					  src_obj->io_buffer_idx);

			/* Update time and index */
			src_obj->time_value += src_obj->fs_ratio;
			src_obj->io_buffer_idx++;

			/* Wrap around */
			if (src_obj->io_buffer_mode == ASRC_BM_CIRCULAR &&
			    src_obj->io_buffer_idx >= src_obj->io_buffer_length)
				src_obj->io_buffer_idx = 0;

			(*output_num_frames)++;
		} else {
			/* Consume input sample */
			asrc_write_to_ring_buffer32(src_obj, input_buffers,
						    index_input_frame);
			index_input_frame++;

			/* Update time */
			src_obj->time_value -= TIME_VALUE_ONE;
		}
	}
	*write_index = src_obj->io_buffer_idx;
	*input_num_frames = index_input_frame;

	/* if fixed control mode, update the frames counters and check if we
	 * have reached end of control cycle.
	 */
	if (src_obj->control_mode == ASRC_CM_FIXED) {
		src_obj->prim_num_frames += *input_num_frames;
		src_obj->sec_num_frames += *output_num_frames;
		if (src_obj->prim_num_frames >= src_obj->prim_num_frames_targ &&
		    src_obj->sec_num_frames >= src_obj->sec_num_frames_targ) {
			if (src_obj->sec_num_frames !=
			    src_obj->sec_num_frames_targ) {
				comp_err(dev, "process_push32(), Generated =  %d, Target = %d",
					 src_obj->sec_num_frames,
					 src_obj->sec_num_frames_targ);
				return ASRC_EC_FAILED_PUSH_MODE;
			}

			if (src_obj->time_value > TIME_VALUE_LIMIT) {
				comp_err(dev, "process_push32(), Time value = %d",
					 src_obj->time_value);
				return ASRC_EC_FAILED_PUSH_MODE;
			}

			/* Force that update_fs_ratio() will be called
			 * (this will reset the time value).
			 */
			src_obj->is_updated = false;
		}
	}

	return ASRC_EC_OK;
}

enum asrc_error_code asrc_process_pull16(struct comp_dev *dev,
					 struct asrc_farrow *src_obj,
					 int16_t **__restrict input_buffers,
					 int *input_num_frames,
					 int16_t **__restrict output_buffers,
					 int *output_num_frames,
					 int write_index,
					 int *read_index)
{
	int index_output_frame = 0;

	/* parameter error handling */
	if (!src_obj || !input_buffers || !output_buffers ||
	    !input_num_frames || !read_index)
		return ASRC_EC_INVALID_POINTER;

	if (!src_obj->is_initialised)
		return ASRC_EC_INIT_FAILED;

	if (src_obj->control_mode == ASRC_CM_FIXED && !src_obj->is_updated)
		return ASRC_EC_UPDATE_FS_FAILED;

	if (src_obj->io_buffer_mode == ASRC_BM_CIRCULAR)
		src_obj->io_buffer_idx = *read_index;
	else
		/* linear */
		src_obj->io_buffer_idx = 0;

	*input_num_frames = 0;

	/* Run state machine until number of output samples are written */
	while (index_output_frame < *output_num_frames) {
		if (src_obj->time_value_pull < TIME_VALUE_ONE) {
			/* Consume one input sample */
			if (src_obj->io_buffer_idx == write_index)
				break;

			asrc_write_to_ring_buffer16(src_obj,
						    input_buffers,
						    src_obj->io_buffer_idx);
			src_obj->io_buffer_idx++;

			/* Wrap around */
			if (src_obj->io_buffer_mode == ASRC_BM_CIRCULAR &&
			    src_obj->io_buffer_idx >= src_obj->io_buffer_length)
				src_obj->io_buffer_idx = 0;

			(*input_num_frames)++;

			/* Update time as Q5.27 */
			src_obj->time_value = (((int64_t)TIME_VALUE_ONE -
						src_obj->time_value_pull) *
					       src_obj->fs_ratio_inv) >> 27;
			src_obj->time_value_pull += src_obj->fs_ratio;
		} else {
			/* Calculate impulse response */
			(*src_obj->calc_ir)(src_obj);

			/* Filter and write output sample to output_buffer */
			asrc_fir_filter16(src_obj, output_buffers,
					  index_output_frame);

			/* Update time and index */
			src_obj->time_value += src_obj->fs_ratio_inv;
			src_obj->time_value_pull -= TIME_VALUE_ONE;
			index_output_frame++;
		}
	}
	*read_index = src_obj->io_buffer_idx;
	*output_num_frames = index_output_frame;

	/* if fixed control mode, update the frames counters and check if we
	 * have reached end of control cycle.
	 */
	if (src_obj->control_mode == ASRC_CM_FIXED) {
		src_obj->prim_num_frames += *output_num_frames;
		src_obj->sec_num_frames += *input_num_frames;
		if (src_obj->prim_num_frames >= src_obj->prim_num_frames_targ &&
		    src_obj->sec_num_frames >= src_obj->sec_num_frames_targ) {
			if (src_obj->sec_num_frames !=
			    src_obj->sec_num_frames_targ) {
				comp_err(dev, "process_pull16(), Consumed = %d, Target = %d",
					 src_obj->sec_num_frames,
					 src_obj->sec_num_frames_targ);
				return ASRC_EC_FAILED_PULL_MODE;
			}

			if (src_obj->time_value_pull > TIME_VALUE_LIMIT) {
				comp_err(dev, "process_pull16(), Time value = %d",
					 src_obj->time_value_pull);
				return ASRC_EC_FAILED_PULL_MODE;
			}

			/* Force that update_fs_ratio() will be called
			 * (this will reset the time value).
			 */
			src_obj->is_updated = false;
		}
	}

	return ASRC_EC_OK;
}

enum asrc_error_code asrc_process_pull32(struct comp_dev *dev,
					 struct asrc_farrow *src_obj,
					 int32_t **__restrict input_buffers,
					 int *input_num_frames,
					 int32_t **__restrict output_buffers,
					 int *output_num_frames,
					 int write_index,
					 int *read_index)
{
	int index_output_frame = 0;

	/* parameter error handling */
	if (!src_obj || !input_buffers || !output_buffers ||
	    !input_num_frames || !read_index)
		return ASRC_EC_INVALID_POINTER;

	if (!src_obj->is_initialised)
		return ASRC_EC_INIT_FAILED;

	if (src_obj->control_mode == ASRC_CM_FIXED && !src_obj->is_updated)
		return ASRC_EC_UPDATE_FS_FAILED;

	/* See 'process_pull16' for a more detailed description of the
	 * algorithm
	 */

	if (src_obj->io_buffer_mode == ASRC_BM_CIRCULAR)
		src_obj->io_buffer_idx = *read_index;
	else
		src_obj->io_buffer_idx = 0;

	*input_num_frames = 0;
	while (index_output_frame < *output_num_frames) {
		if (src_obj->time_value_pull < TIME_VALUE_ONE) {
			/* Consume input sample */
			if (src_obj->io_buffer_idx == write_index)
				break;

			asrc_write_to_ring_buffer32(src_obj,
						    input_buffers,
						    src_obj->io_buffer_idx);
			src_obj->io_buffer_idx++;

			/* Wrap around */
			if (src_obj->io_buffer_mode == ASRC_BM_CIRCULAR &&
			    src_obj->io_buffer_idx >= src_obj->io_buffer_length)
				src_obj->io_buffer_idx = 0;

			(*input_num_frames)++;

			/* Update time as Q5.27 */
			src_obj->time_value = (((int64_t)TIME_VALUE_ONE -
						src_obj->time_value_pull) *
					       src_obj->fs_ratio_inv) >> 27;
			src_obj->time_value_pull += src_obj->fs_ratio;
		} else {
			/* Calculate impulse response */
			(*src_obj->calc_ir)(src_obj);

			/* Filter and write output sample to output_buffer */
			asrc_fir_filter32(src_obj, output_buffers,
					  index_output_frame);

			/* Update time and index */
			src_obj->time_value += src_obj->fs_ratio_inv;
			src_obj->time_value_pull -= TIME_VALUE_ONE;
			index_output_frame++;
		}
	}
	*read_index = src_obj->io_buffer_idx;
	*output_num_frames = index_output_frame;

	/* if fixed control mode, update the frames counters and check
	 * if we have reached end of control cycle.
	 */
	if (src_obj->control_mode == ASRC_CM_FIXED) {
		src_obj->prim_num_frames += *output_num_frames;
		src_obj->sec_num_frames += *input_num_frames;
		if (src_obj->prim_num_frames >= src_obj->prim_num_frames_targ &&
		    src_obj->sec_num_frames >= src_obj->sec_num_frames_targ) {
			if (src_obj->sec_num_frames !=
			    src_obj->sec_num_frames_targ) {
				comp_err(dev, "process_pull32(), Consumed = %d, Target = %d.\n",
					 src_obj->sec_num_frames,
					 src_obj->sec_num_frames_targ);
				return ASRC_EC_FAILED_PULL_MODE;
			}

			if (src_obj->time_value_pull > TIME_VALUE_LIMIT) {
				comp_err(dev, "process_pull32(): Time value = %d",
					 src_obj->time_value_pull);
				return ASRC_EC_FAILED_PULL_MODE;
			}

			/* Force that update_fs_ratio() will be called
			 * (this will reset the time value).
			 */
			src_obj->is_updated = false;
		}
	}

	return ASRC_EC_OK;
}
