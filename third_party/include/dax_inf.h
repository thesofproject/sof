/* SPDX-License-Identifier: BSD-3-Clause
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE
 *
 * Copyright(c) 2025 Dolby Laboratories. All rights reserved.
 *
 * Author: Jun Lai <jun.lai@dolby.com>
 */

#ifndef DAX_INF_H
#define DAX_INF_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum dax_frame_fmt {
	DAX_FMT_UNSUPPORTED = -1,
	DAX_FMT_SHORT_16 = 4,
	DAX_FMT_INT = 5,
	DAX_FMT_FLOAT = 7,
};

enum dax_sample_rate {
	DAX_RATE_UNSUPPORTED = -1,
};

enum dax_channels {
	DAX_CHANNLES_UNSUPPORTED = -1,
};

enum dax_buffer_fmt {
	DAX_BUFFER_LAYOUT_UNSUPPORTED = -1,
	DAX_BUFFER_LAYOUT_INTERLEAVED,
	DAX_BUFFER_LAYOUT_NONINTERLEAVED,
};

enum dax_param_id {
	DAX_PARAM_ID_ENABLE = 0x08001026,
	DAX_PARAM_ID_TUNING_FILE = 0x08001027,
	DAX_PARAM_ID_PROFILE = 0x08001028,
	DAX_PARAM_ID_ENDPOINT = 0x08001029,
	DAX_PARAM_ID_TUNING_DEVICE = 0x08001030,
	DAX_PARAM_ID_CP_ENABLE = 0x08001031,
	DAX_PARAM_ID_OUT_DEVICE = 0x08001032,
	DAX_PARAM_ID_ABSOLUTE_VOLUME = 0x08001033,
	DAX_PARAM_ID_CTC_ENABLE = 0x08001034,
};

struct dax_media_fmt {
	enum dax_frame_fmt data_format;
	uint32_t sampling_rate;
	uint32_t num_channels;
	enum dax_buffer_fmt layout;
	uint32_t bytes_per_sample;
};

struct dax_buffer {
	void *addr;
	uint32_t size;	/* Total buffer size in bytes */
	uint32_t avail;  /* Available bytes for reading */
	uint32_t free;	/* Free bytes for writing */
};

struct sof_dax {
	/* SOF module parameters */
	uint32_t sof_period_bytes;

	/* DAX state parameters */
	uint32_t period_bytes;
	uint32_t period_us;
	int32_t endpoint;
	int32_t tuning_device;
	void    *blob_handler;
	void    *p_dax;
	struct dax_media_fmt input_media_format;
	struct dax_media_fmt output_media_format;

	/* DAX control parameters */
	int32_t enable;
	int32_t profile;
	int32_t out_device;
	int32_t ctc_enable;
	int32_t content_processing_enable;
	int32_t volume;
	uint32_t update_flags;

	/* DAX buffers */
	struct dax_buffer persist_buffer; /* Used for dax instance */
	struct dax_buffer scratch_buffer; /* Used for dax process */
	struct dax_buffer input_buffer;
	struct dax_buffer output_buffer;
	struct dax_buffer tuning_file_buffer;
};

/**
 * @brief Query the persistent memory requirements for the DAX module
 *
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return Size of required persistent memory in bytes
 */
uint32_t dax_query_persist_memory(struct sof_dax *dax_ctx);

/**
 * @brief Query the scratch memory requirements for the DAX module
 *
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return Size of required scratch memory in bytes
 */
uint32_t dax_query_scratch_memory(struct sof_dax *dax_ctx);

/**
 * @brief Query the number of frames in a processing period
 *
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return Number of frames per period
 */
uint32_t dax_query_period_frames(struct sof_dax *dax_ctx);

/**
 * @brief Free the DAX module
 *
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * This function free all resources built on persistent buffer.
 * DO NOT USE THE INSTANCE AFTER CALLING FREE.
 *
 * @return 0 on success, negative error code on failure
 */
int dax_free(struct sof_dax *dax_ctx);

/**
 * @brief Initialize the DAX module
 *
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return 0 on success, negative error code on failure
 */
int dax_init(struct sof_dax *dax_ctx);

/**
 * @brief Process audio data through the DAX module
 *
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return Bytes of processed. negative error code on failure
 */
int dax_process(struct sof_dax *dax_ctx);

/**
 * @brief Set a parameter value for the DAX module
 *
 * @param[in] id Parameter identifier
 * @param[in] val Pointer to parameter value
 * @param[in] val_sz Size of parameter value in bytes
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return 0 on success, negative error code on failure
 */
int dax_set_param(uint32_t id, const void *val, uint32_t val_sz, struct sof_dax *dax_ctx);

/**
 * @brief Enable/Disable the DAX module
 *
 * @param[in] enable 0:disable, 1:enable.
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return 0 on success, negative error code on failure
 */
int dax_set_enable(int32_t enable, struct sof_dax *dax_ctx);

/**
 * @brief Set the volume for the DAX module
 *
 * @param[in] volume Value to apply
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return 0 or positive code on success, negative error code on failure
 */
int dax_set_volume(int32_t volume, struct sof_dax *dax_ctx);

/**
 * @brief Update the output device configuration
 *
 * @param[in] out_device Output device identifier. Supported devices:
 *		0: speaker
 *		1: headphone
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return 0 on success, negative error code on failure
 */
int dax_set_device(int32_t out_device, struct sof_dax *dax_ctx);

/**
 * @brief Enable/Disable crosstalk cancellation feature
 *
 * @param[in] enable 0:disable, 1:enable.
 * @param[in] dax_ctx Pointer to the DAX context structure
 *
 * @return 0 on success, negative error code on failure
 */
int dax_set_ctc_enable(int32_t enable, struct sof_dax *dax_ctx);

/**
 * @brief Get the DAX module version string
 *
 * @return Pointer to null-terminated version string
 */
const char *dax_get_version(void);

/**
 * @brief Find parameters in a buffer based on query criteria
 *
 * @param[in]  query_id ID of the parameter to search for. Supported query IDs:
 *		- DAX_PARAM_ID_PROFILE
 *		- DAX_PARAM_ID_TUNING_DEVICE
 *		- DAX_PARAM_ID_CP_ENABLE
 * @param[in]  query_val Value to match when searching
 * @param[out] query_sz Pointer to store the size of the found parameters
 * @param[in]  dax_ctx Pointer to the DAX context structure
 *
 * @return Pointer to the found parameters, or NULL if not found
 */
void *dax_find_params(uint32_t query_id,
		      int32_t query_val,
		      uint32_t *query_sz,
		      struct sof_dax *dax_ctx);

#endif /* DAX_INF_H */
