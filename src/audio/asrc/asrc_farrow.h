/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2012-2019 Intel Corporation. All rights reserved.
 */

/*
 * @brief    API header containing sample rate converter struct and interface
 *           functions.
 *
 * @mainpage Hifi3 Implementation of the Intel TSD Sample Rate Converter
 *
 * The sample rate converter is based on the so-called Farrow structure.
 * It supports multi-channel operation, and the PCM samples can be
 * represented by int16_t or int32_t values.
 *
 * The sample rate converter can be applied for transmit and receive
 * use cases.  To support both directions, the sample rate converter
 * offers two modes of operation: push-mode and pull-mode.
 * <p>
 * @image html Push_mode-Pull_mode-Use_case_150dpi.png
 * "Push-mode vs. pull-mode operation"
 * @image rtf  Push_mode-Pull_mode-Use_case_150dpi.png
 * </p>
 * If the sample rate converter operates in push-mode, the caller can
 * specify how many input frames shall be processed with each call of
 * this function.  Depending on the current conversion ratio, the
 * number of output frames might vary from call to call. Therefore,
 * the push-mode sample rate converter is usually combined with a ring
 * buffer at its output. The push-mode operation is performed by the
 * functions process_push16() and process_push32().
 *
 * If the sample rate converter operates in pull-mode, the caller can
 * specify how many output frames shall be generated with each call of
 * this function.  Depending on the current conversion ratio, the
 * number of input frames might vary from call to call. Therefore, the
 * pull-mode sample rate converter is usually combined with a ring
 * buffer at its input. The pull-mode operation is performed by the
 * functions process_pull16() and process_pull32().
 */

#ifndef IAS_SRC_FARROW_H
#define IAS_SRC_FARROW_H

#include <sof/audio/component.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * @brief FIR filter is max 128 taps and delay lines per channel are
 * max 256 samples long.
 */
#define ASRC_MAX_FILTER_LENGTH	128

/*
 * @brief Define whether the input and output buffers shall be
 * interleaved or not.
 */
enum asrc_io_format {
	ASRC_IOF_DEINTERLEAVED, /*<! Non-interleaved, i.e. individual buffers */
				/*<! for each channel. */
	ASRC_IOF_INTERLEAVED    /*<! Interleaved buffers. */
};

/*
 * @brief Define whether the sample rate converter shall use a linear
 * buffer or a ring buffer on its secondary side.
 *
 * The secondary side corresponds to the output, if the sample rate
 * converter is a push mode device.  The secondary side corresponds to
 * the input, if the sample rate converter is a pull mode device.
 */
enum asrc_buffer_mode {
	ASRC_BM_CIRCULAR,	/*!< The buffer at the secondary side is */
				/*!< used as ring buffer. */
	ASRC_BM_LINEAR		/*!< The buffer at the secondary side is used */
				/*!< as linear buffer. */
};

/*
 * @brief Define how the ASRC controller (drift estimator) shall
 * update the conversion ratio.
 */
enum asrc_control_mode {
	ASRC_CM_FEEDBACK,	/*!< The controller specifies a factor that */
				/*!< is applied  to the fs ratio. */
				/*!< This has to be done by means of the */
				/*!< function update_drift(). */
	ASRC_CM_FIXED		/*!< The controller specifies a fixed number */
				/*!< of samples that shall be processed on */
				/*!< the secondary side during the following */
				/*!< control cycle. This has to be done by */
				/*!< means of the function update_fs_ratio(). */
};

/*
 * @brief Define whether the ASRC will be used in push-mode or in
 * pull-mode.
 */
enum asrc_operation_mode {
	ASRC_OM_PUSH,	/*!< ASRC will be used in push-mode; functions */
			/*!< process_push16() or process_push32() */
	ASRC_OM_PULL	/*!< ASRC will be used in pull-mode; functions */
			/*!< process_pull16() or process_pull32() */
};

/*
 * @brief Error code
 */
enum asrc_error_code {
	ASRC_EC_OK = 0,				/*!< Operation successful. */
	ASRC_EC_INIT_FAILED = -1,		/*!< Initialization of the component failed. */
	ASRC_EC_UPDATE_FS_FAILED = -2,		/*!< Control mode is set to CM_FIXED and update */
						/*!< drift is not called in time. */
	ASRC_EC_INVALID_POINTER = -3,		/*!< Couldn't allocate memory. Bad pointer. */
	ASRC_EC_INVALID_BUFFER_POINTER = -4,	/*!< Internal buffer pointers are invalid. */
	ASRC_EC_INVALID_SAMPLE_RATE = -5,	/*!< Sample rate is not supported. */
	ASRC_EC_INVALID_CONVERSION_RATIO = -6,	/*!< Conversion ratio is not supported. */
	ASRC_EC_INVALID_BIT_DEPTH = -7,		/*!< Bit depth is not supported. Choose either */
						/*!< 16 or 32 bit. */
	ASRC_EC_INVALID_NUM_CHANNELS = -8,	/*!< Nummber of channels must be larger */
						/*!< than zero. */
	ASRC_EC_INVALID_BUFFER_LENGTH = -9,	/*!< Buffer length must be larger than one. */
	ASRC_EC_INVALID_FRAME_SIZE = -10,	/*!< Invalid frame size: must be greater than 0 */
						/*!< for primary side and secondary side. */
	ASRC_EC_INVALID_CLOCK_SKEW = -11,	/*!< The clock drift is out of bounds. */
	ASRC_EC_INVALID_CONTROL_MODE = -12,	/*!< Call update_fs_ratio() for feedback, and */
						/*!< update_drift() for fixed control mode. */
	ASRC_EC_FAILED_PUSH_MODE = -13,		/*!< Push mode operation failed. */
	ASRC_EC_FAILED_PULL_MODE = -14,		/*!< Pull mode operation failed. */
	ASRC_EC_INVALID_FILTER_LENGTH = -15,    /*!< Length exceeds max. */
};

/*
 * asrc_farrow: Struct which stores all the setup parameters and
 * pointers.  An instance of this struct has to be passed to all
 * setter and processing functions.
 */
struct asrc_farrow {
	/* IO + ring_buffer data */
	int num_channels;	/*!< Number of channels processed */
				/*!< simultaneously */
	int buffer_length;	/*!< Length of the ring buffer for each */
				/*!< channel */
	int buffer_write_position;	/*!< Position of the ring buffer */
					/*!< to which will be written next */
	int32_t **ring_buffers32;	/*!< Pointer to the pointers to the */
					/*!< 32 bit ring buffers for each */
					/*!< channel */
	int16_t **ring_buffers16;	/*!< Pointer to the pointers to the */
					/*!< 16 bit ring buffers for each */
					/*!< channel */

	/* + IO ring_buffer status */
	enum asrc_buffer_mode io_buffer_mode; /*!< Mode in which IO buffers */
					      /*!< are operated */
	int io_buffer_idx;	/*!< Index for accessing the IO ring buffer */
	int io_buffer_length;	/*!< Size of the IO ring buffer */

	/* FILTER + filter parameters */
	int filter_length;	/*!< Length of the impulse response */
	int num_filters;	/*!< Total number of filters used */

	/* + filter coefficients */
	const int32_t *polyphase_filters; /*!< Pointer to the filter */
					  /*!< coefficients */
	int32_t *impulse_response; /*!< Pointer to the impulse response */
				   /*!< for generating one output sample */

	/* PROGRAM + general */
	bool is_initialised;	/*!< Flag is set to true after */
				/*!< initialise_asrc function */
	bool is_updated;        /*!< Flag is set to true after call to */
				/*!< update_fs_ratio */
	enum asrc_io_format input_format; /*!< Format in which the input */
					  /*!< data arrives (interleaved / */
					  /*!< deinterleaved) */
	enum asrc_io_format output_format; /*!< Format in which the output */
					   /*!< data is written (interleaved */
					   /*!< or deinterleaved) */
	int bit_depth; /*!< Bit depth of input and output signal */
	enum asrc_control_mode control_mode; /*!< Operation mode of the */
					     /*!< src's controller */
	enum asrc_operation_mode operation_mode; /*!< Operation mode of the */
						 /*!< src itself */

	/* + state machine */
	int fs_prim;		/*!< Primary sampling rate */
	int fs_sec;		/*!< Secondary sampling rate */
	uint32_t fs_ratio;	/*!< Conversion ratio: Input_rate/Output_rate */
				/*!< (5q27) */
	uint32_t fs_ratio_inv;	/*!< Reciprocal conversion ratio for */
				/*!< synchronous pull mode (5q27) */
	uint32_t time_value;	/*!< Relative position between two input */
				/*!< samples (5q27) */
	uint32_t time_value_pull; /*!< Time value used for synchronised */
				  /*!< pull mode (5q27) */
	int prim_num_frames;	/*!< Counts number of samples on */
				/*!< primary side */
	int prim_num_frames_targ;	/*!< Target number of samples on */
					/*!< primary side during one control */
					/*!< loop */
	int sec_num_frames;	/*!< Counts number of samples on */
				/*!< secondary side */
	int sec_num_frames_targ;	/*!< Target number of samples on */
					/*!< secondary side during one */
					/*!< control loop */

	/* + function pointer */
	void (*calc_ir)(struct asrc_farrow *src_obj);	/*!< Pointer */
	/*!< to the function which calculates the impulse response */
};

/*
 * @brief ASRC component struct storing the configuration.
 */

/*
 * @brief Returns the required size in bytes to be allocated for the
 * ASRC struct and buffers.
 *
 * This function should be called before creating and initialising an
 * instance of ias_src_farrow.  It lies on the user to allocate the
 * required memory and let the ias_src_farrow instance point to the
 * beginning.
 *
 * @returns    Error code.
 * @param[out] required_size The required size in bytes that has to be
 *                           allocated by the application.
 * @param[in]  num_channels  The number of channels the ASRC shall be used for.
 * @param[in]  bit_depth     The wordlength that will be used for representing
 *                           the PCM samples, must be 16 or 32.
 */
enum asrc_error_code asrc_get_required_size(struct processing_module *mod,
					    int *required_size,
					    int num_channels,
					    int bit_depth);

/*
 * @brief Initialises the ias_src_farrow instance.
 *
 * This function should be called immediately after memory allocation.
 * Requires a pointer to the instance to be initialised.
 *
 * @param[in] src_obj        Pointer to the ias_src_farrow.
 * @param[in] num_channels   Number of channels that should be processed.
 * @param[in] fs_prim        Primary signal sampling rate.
 * @param[in] fs_sec         Secondary signal sampling rate.
 * @param[in] input_format   Configures how input data will be read.
 *                           Choose 'interleaved' for interleaved input data
 *                           or 'deinterleaved' instead.
 * @param[in] output_format  Configures how output data will be written.
 *                           Choose 'interleaved' for interleaved input data
 *                           or 'deinterleaved' instead.
 * @param[in] buffer_mode    Choose linear or circular; this defines whether a
 *			     linear buffer or a ring buffer shall be used at
 *                           the secondary side.
 * @param[in] buffer_length  Length of the I/O buffer
 * @param[in] bit_depth      Bit depth of input and output signal. Valid
 *                           values are 16 and 32!
 * @param[in] control_mode   Choose the kind of controller used to operate the
 *                           ASRC.
 *                           Select 'feedback' if the controller returns a
 *                           factor applied to the fs ratio.
 *                           Select 'fixed' if the controller returns a fixed
 *                           number of samples to be generated
 *                           in the following control cycle.
 * @param[in] operation_mode Choose 'push' or 'pull', depending on the mode
 *                           you want your ASRC to operate in.
 */
enum asrc_error_code asrc_initialise(struct processing_module *mod,
				     struct asrc_farrow *src_obj,
				     int num_channels,
				     int32_t fs_prim,
				     int32_t fs_sec,
				     enum asrc_io_format input_format,
				     enum asrc_io_format output_format,
				     enum asrc_buffer_mode buffer_mode,
				     int buffer_length,
				     int bit_depth,
				     enum asrc_control_mode control_mode,
				     enum asrc_operation_mode operation_mode);

/*
 * @brief Free polyphase filters
 *
 * @param[in] src_obj        Pointer to the ias_src_farrow.
 */
void asrc_free_polyphase_filter(struct processing_module *mod, struct asrc_farrow *src_obj);

/*
 * @brief Process the sample rate converter for one frame; the frame
 *        consists of @p input_num_frames samples within @p num_channels
 *        channels.
 *
 * This function represents the push-mode implementation of the sample
 * rate converter.  This means that the caller can specify how many
 * input samples shall be processed with each call of this
 * function. The function returns by means of the output parameter @p
 * output_num_frames how many output frames have been generated.
 *
 * @param[in] src_obj            Pointer to the ias_src_farrow instance.
 * @param[in] input_buffers      Pointer to the pointers to the 16 bit input
 *                               data arrays.  Should contain <tt>
 *                               input_buffer_size *
 *                               ias_src_farrow.m_num_channels </tt>
 *                               samples.  Data can be stored in
 *                               interleaved or deinterleaved format,
 *                               this should also be correctly
 *                               configured in the @p src_obj.
 * @param[in, out] input_num_frames
 *				 Number of samples in each channel of input
 *                               data (NOT the number of total samples).
 * @param[out] output_buffers    Pointer to the pointers to the 16 bit
 *                               buffers the generated samples should
 *                               be stored in.  Written in interleaved
 *                               or deinterleaved format, depending on
 *                               the configuration in @p src_obj.
 * @param[out] output_num_frames Number of samples written to the output
 *                               buffer for each channel.
 * @param[out] write_index       If I/O buffer mode is set to BM_CIRCULAR,
 *                               this parameter returns the position
 *                               within the output_buffers, where the
 *                               next call of this function would write
 *                               to. In other words: when this
 *                               function returns, the output buffer
 *                               provides the most recent samples up to
 *                               positions < @p write_index.
 * @param[in] read_index         If I/O buffer mode is set to BM_CIRCULAR,
 *                               this parameter must specify until
 *                               which position the application has
 *                               already read samples from the
 *                               output_buffers (the parameter must
 *                               specify the next position, which has
 *                               not been read so far).  The sample
 *                               rate converter only writes samples to
 *                               positions < @p read_index in order to
 *                               avoid that unconsumed samples are
 *                               overwritten.
 */
enum asrc_error_code asrc_process_push16(struct comp_dev *dev,
					 struct asrc_farrow *src_obj,
					 int16_t **__restrict input_buffers,
					 int *input_num_frames,
					 int16_t **__restrict output_buffers,
					 int *output_num_frames,
					 int *write_index,
					 int read_index);

/*
 * @brief Process the sample rate converter for one frame; the frame
 *        consists of @p input_num_frames samples within @p num_channels
 *        channels.
 *
 * This function represents the push-mode implementation of the sample
 * rate converter.  This means that the caller can specify how many
 * input samples shall be processed with each call of this
 * function. The function returns by means of the output parameter @p
 * output_num_frames how many output frames have been generated.
 *
 * @param[in] src_obj             Pointer to the ias_src_farrow instance.
 * @param[in] input_buffers       Pointer to the pointers to the 32 bit input
 *                                data arrays. Should contain <tt>
 *                                input_buffer_size *
 *                                ias_src_farrow.m_num_channels </tt>
 *                                samples.Data can be stored in
 *                                interleaved or deinterleaved format,
 *                                this should also be correctly
 *                                configured in the @p src_obj.
 * @param[in, out] input_num_frames
 *				  Number of samples in each channel of
 *                                input data (NOT the number of total samples).
 * @param[out] output_buffers     Pointer to the pointers to the 32 bit
 *                                buffers the generated samples should
 *                                be stored in.  Written in interleaved
 *                                or deinterleaved format, depending on
 *                                the configuration in @p src_obj.
 * @param[out] output_num_frames  Number of samples written to the output
 *                                buffer for each channel.
 * @param[out] write_index        If I/O buffer mode is set to BM_CIRCULAR,
 *                                this parameter returns the position
 *                                within the output_buffers, where the
 *                                next call of this function would write
 *                                to.  In other words: when this
 *                                function returns, the output buffer
 *                                provides the most recent samples up to
 *                                positions < @p write_index.
 * @param[in] read_index          If I/O buffer mode is set to BM_CIRCULAR,
 *                                this parameter must specify until
 *                                which position the application has
 *                                already read samples from the
 *                                output_buffers (the parameter must
 *                                specify the next position, which has
 *                                not been read so far).  The sample
 *                                rate converter only writes samples to
 *                                positions < @p read_index in order to
 *                                avoid that unconsumed samples are
 *                                overwritten.
 */
enum asrc_error_code asrc_process_push32(struct comp_dev *dev,
					 struct asrc_farrow *src_obj,
					 int32_t **__restrict input_buffers,
					 int *input_num_frames,
					 int32_t **__restrict output_buffers,
					 int *output_num_frames,
					 int *write_index,
					 int read_index);

/*
 * @brief Process the sample rate converter and generate one frame of
 *        @p num_output_samples samples within @p num_channels channels.
 *
 * This function represents the pull-mode implementation of the sample
 * rate converter.  This means that the caller can specify how many
 * output samples shall be generated with each call of this
 * function. The function returns by means of the output parameter @p
 * input_num_frames how many input frames have been consumed.
 *
 * @param[in] src_obj           Pointer to the ias_src_farrow instance.
 *
 * @param[in] input_buffers     Pointer to the pointers to the 16 bit input
 *                              data arrays.  Should contain <tt>
 *                              input_buffer_size *
 *                              ias_src_farrow.m_num_channels </tt>
 *                              samples.  Data can be stored in
 *                              interleaved or deinterleaved format,
 *                              this should also be correctly
 *                              configured in the @p src_obj.
 * @param[out] input_num_frames Number of samples in each channel that
 *                              have been consumed from the input buffers.
 * @param[out] output_buffers   Pointer to the pointers to the 16 bit
 *                              buffers the generated samples should
 *                              be stored in.  Written in interleaved
 *                              or deinterleaved format, depending on
 *                              the configuration in @p src_obj.  No
 *                              support for output ring buffers yet.
 * @param[in, out] output_num_frames
 *				Number of samples that shall be written
 *                              to the output buffer for each channel.
 * @param[in] write_index       Index within the input ring buffer, must
 *                              point to the next frame, which has not
 *                              been written by the application so
 *                              far. The sample rate converter only
 *                              reads frames from before this
 *                              position. If I/O buffer mode is set to
 *                              BM_LINEAR, the @p write_index is
 *                              equivalent to the number of frames
 *                              provided by the application within the
 *                              input buffer.
 * @param[in,out] read_index    If I/O buffer mode is set to BM_CIRCULAR,
 *                              this parameter specifies the index
 *                              within the input ring buffer where the
 *                              sample rate converter shall start
 *                              reading frames from.  When the
 *                              function returns, this parameter
 *                              returns the index of the next frame,
 *                              which has not been read so far.  If
 *                              I/O buffer mode is set to BM_LINEAR,
 *                              the function starts reading always
 *                              from the beginning of the input
 *                              buffer, so this parameter is ignored.
 *                              When the function returns, this
 *                              parameter returns the number of frames
 *                              that have been read.
 */
enum asrc_error_code asrc_process_pull16(struct comp_dev *dev,
					 struct asrc_farrow *src_obj,
					 int16_t **__restrict input_buffers,
					 int *input_num_frames,
					 int16_t **__restrict output_buffers,
					 int *output_num_frames,
					 int write_index,
					 int *read_index);

/*
 * @brief Process the sample rate converter and generate one frame of
 *        @p num_output_samples samples within @p num_channels channels.
 *
 * This function represents the pull-mode implementation of the sample
 * rate converter.  This means that the caller can specify how many
 * output samples shall be generated with each call of this
 * function. The function returns by means of the output parameter @p
 * input_num_frames how many input frames have been consumed.
 *
 * @param[in] src_obj           Pointer to the ias_src_farrow instance.
 * @param[in] input_buffers     Pointer to the pointers to the 32 bit input
 *                              data arrays.  Should contain <tt>
 *                              input_buffer_size *
 *                              ias_src_farrow.m_num_channels </tt>
 *                              samples.  Data can be stored in
 *                              interleaved or deinterleaved format,
 *                              this should also be correctly
 *                              configured in the @p src_obj.
 * @param[out] input_num_frames Number of samples in each channel that
 *                              have been consumed from the input buffers.
 * @param[out] output_buffers   Pointer to the pointers to the 32 bit
 *                              buffers the generated samples should
 *                              be stored in.  Written in interleaved
 *                              or deinterleaved format, depending on
 *                              the configuration in @p src_obj.  No
 *                              support for output ring buffers yet.
 * @param[in, out] output_num_frames
 *				Number of samples that shall be written
 *                              to the output buffer for each channel.
 * @param[in] write_index       Index within the input ring buffer, must
 *                              point to the next frame, which has not
 *                              been written by the application so
 *                              far. The sample rate converter only
 *                              reads frames from before this
 *                              position. If I/O buffer mode is set to
 *                              BM_LINEAR, the @p write_index is
 *                              equivalent to the number of frames
 *                              provided by the application within the
 *                              input buffer.
 * @param[in,out] read_index    If I/O buffer mode is set to BM_CIRCULAR,
 *                              this parameter specifies the index
 *                              within the input ring buffer where the
 *                              sample rate converter shall start
 *                              reading frames from.  When the
 *                              function returns, this parameter
 *                              returns the index of the next frame,
 *                              which has not been read so far.  If
 *                              I/O buffer mode is set to BM_LINEAR,
 *                              the function starts reading always
 *                              from the beginning of the input
 *                              buffer, so this parameter is ignored.
 *                              When the function returns, this
 *                              parameter returns the number of frames
 *                              that have been read.
 */
enum asrc_error_code asrc_process_pull32(struct comp_dev *dev,
					 struct asrc_farrow *src_obj,
					 int32_t **__restrict input_buffers,
					 int *input_num_frames,
					 int32_t **__restrict output_buffers,
					 int *output_num_frames,
					 int write_index,
					 int *read_index);

/*
 * @brief Updates the clock drift
 *
 * Compensate the drift in primary and secondary clock sources by
 * applying the clock skew to the fs ratio. Only call this function if
 * the controller mode is set to 'feedback'.
 *
 * @param[in] src_obj    Pointer to the ias_src_farrow instance.
 * @param[in] clock_skew Allows compensation of the clock drift.
 *                       Value should be passed as 2q30 fixed point value.
 *                       For synchrounous operation pass (1 << 30).
 */
enum asrc_error_code asrc_update_drift(struct comp_dev *dev,
				       struct asrc_farrow *src_obj,
				       uint32_t clock_skew);

/*
 * @brief Updates the fs ratio
 *
 * Ensure generation/consumption of a certain number of samples on
 * primary and secondary side.  This function should be called just
 * after the controller function AND only if the controller mode is
 * set to 'fixed'.  In push mode @p secondary_num_frames samples will be
 * generated out of @p primary_num_frames samples per channel.  In pull
 * mode @p primary_num_frames samples will be generated out of @p
 * secondary_num_frames samples per channel.
 *
 * @param[in] src_obj              Pointer to the ias_src_farrow instance.
 * @param[in] primary_num_frames   Number of samples per channel counted
 *                                 in one controller loop on the
 *                                 primary (ALSA) side
 * @param[in] secondary_num_frames Number of samples per channel counted
 *                                 in one controller loop on the
 *                                 secondary (SSP) side
 */
enum asrc_error_code asrc_update_fs_ratio(struct comp_dev *dev,
					  struct asrc_farrow *src_obj,
					  int primary_num_frames,
					  int secondary_num_frames);

/*
 * @brief Changes the input and output sampling rate.
 *
 * Configures the src for given sampling rates.
 * Shouldn't be called on the fly because the buffers get flushed when called.
 * Release buffers BEFORE call this function and allocate them again after function returns.
 *
 * Available sample rates [Hz] are:
 * 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000.
 *
 * @param[in] src_obj  Pointer to the ias_src_farrow instance.
 * @param[in] fs_prim  Primary sampling rate.
 * @param[in] fs_sec   Secondary sampling rate.
 */
enum asrc_error_code asrc_set_fs_ratio(struct processing_module *mod,
				       struct asrc_farrow *src_obj,
				       int32_t fs_prim, int32_t fs_sec);

/*
 * @brief Changes how input data will be read.
 *
 * Choose either 'interleaved' or 'deinterleaved' of the @e format enum.
 *
 * @param[in] src_obj      Pointer to the ias_src_farrow instance.
 * @param[in] input_format Format parameter.
 */
enum asrc_error_code asrc_set_input_format(struct comp_dev *dev,
					   struct asrc_farrow *src_obj,
					   enum asrc_io_format input_format);

/*
 * @brief Changes how output data will be written.
 *
 * Choose either 'interleaved' or 'deinterleaved' of the @e format enum.
 *
 * @param[in] src_obj        Pointer to the ias_src_farrow instance.
 * @param[in] output_format  Format parameter.
 */
enum asrc_error_code asrc_set_output_format(struct comp_dev *dev,
					    struct asrc_farrow *src_obj,
					    enum asrc_io_format output_format);

/*
 * Write value of 16 bit input buffers to the channels of the ring buffer
 */
void asrc_write_to_ring_buffer16(struct asrc_farrow *src_obj,
				 int16_t **input_buffers,
				 int index_input_frame);

/*
 * Write value of 32 bit input buffers to the channels of the ring buffer
 */
void asrc_write_to_ring_buffer32(struct asrc_farrow *src_obj,
				 int32_t **input_buffers,
				 int index_input_frame);

/*
 * Filter the 32 bit ring buffer values with impulse_response
 */
void asrc_fir_filter16(struct asrc_farrow *src_obj,
		       int16_t **output_buffers,
		       int index_output_frame);

/*
 * Filter the 32 bit ring buffer values with impulse_response
 */
void asrc_fir_filter32(struct asrc_farrow *src_obj,
		       int32_t **output_buffers,
		       int index_output_frame);

/*
 * Calculates the impulse response. This impulse response is then
 * applied to the buffered signal, in order to generate the output.
 * There are four versions, from whom one is pointed to by the
 * ias_src_farrow struct.  This depends on the number of polyphase
 * filters given for the current conversion ratio.
 */
void asrc_calc_impulse_response_n4(struct asrc_farrow *src_obj);
void asrc_calc_impulse_response_n5(struct asrc_farrow *src_obj);
void asrc_calc_impulse_response_n6(struct asrc_farrow *src_obj);
void asrc_calc_impulse_response_n7(struct asrc_farrow *src_obj);

#endif /* IAS_SRC_FARROW_H */
