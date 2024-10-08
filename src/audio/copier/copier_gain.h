/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 *
 * Author: Ievgen Ganakov <ievgen.ganakov@intel.com>
 */

#ifndef __SOF_COPIER_GAIN_H__
#define __SOF_COPIER_GAIN_H__

#include <sof/audio/buffer.h>
#include <ipc4/base_fw.h>
#include <ipc4/gateway.h>
#include <ipc/dai.h>
#if SOF_USE_HIFI(3, COPIER) || SOF_USE_HIFI(4, COPIER) || SOF_USE_HIFI(5, COPIER)
#include <xtensa/tie/xt_hifi3.h>
#endif
/**
 * @file copier_gain.h
 * @brief Header file containing definitions and functions related to audio gain
 * processing for a copier module.
 *
 * This file provides functions, constants and structure definitions for applying gain to
 * input audio buffers, both in 16-bit and 32-bit container formats. The gain can be
 * applied in different directions (addition or subtraction) and has three modes:
 * - static gain
 * - transition gain (fade-in/fade-out)
 * - mute
 */

/* Maximum number of gain coefficients */
#define MAX_GAIN_COEFFS_CNT 4

/* Common const values for applying gain feature */
#define Q10_TO_Q31_SHIFT     6
#define Q10_TO_Q15_SHIFT     5
#define GAIN_Q10_INT_SHIFT   10

/* 16x2 store operation requires shift to middle part of 32 bit register */
#define I64_TO_I16_SHIFT  48
#define I64_TO_I32_SHIFT  32
#define MIDDLE_PART_SHIFT 8

/* Unit gain in q10 format applied by default */
#define UNITY_GAIN_4X_Q10  0x0400040004000400
#define UNITY_GAIN_GENERIC 0x0400

/* Default fade transition in ms in high quality mode (Freq > 16000Hz) */
#define GAIN_DEFAULT_HQ_TRANS_MS     500
/* Default fade transition in ms in low quality mode */
#define GAIN_DEFAULT_LQ_TRANS_MS     100

#define GAIN_ZERO_TRANS_MS           0xFFFF
#define GAIN_DEFAULT_FADE_PERIOD     0

struct dai_data;

/**
 * @brief Enumeration representing the state of the copier gain processing.
 */
enum copier_gain_state {
	MUTE = 0,    /**< Mute state, zero gain value applied */
	TRANS_GAIN,  /**< Transition gain state, used for fade-in/fade-out */
	STATIC_GAIN, /**< Static gain state, gain value is not changing over time */
};

/**
 * @brief Enumeration representing the change direction of the gain envelope in
 * fade context.
 */
enum copier_gain_envelope_dir {
	GAIN_ADD = 0,	/**< gain envelope add direction */
	GAIN_SUBTRACT, /**< gain envelope subtract direction */
};

/**
 * @brief Structure representing the parameters for copier gain processing.
 */
struct copier_gain_params {
#if SOF_USE_HIFI(3, COPIER) || SOF_USE_HIFI(4, COPIER) || SOF_USE_HIFI(5, COPIER)
	/**< Input gain coefficients in Q10 format */
	ae_int16x4 gain_coeffs[SOF_DIV_ROUND_UP(MAX_GAIN_COEFFS_CNT, 4)];
	/**< Step for fade-in lower precision */
	ae_f16x4 step_f16;
	/**< Initial gain depending on the number of channels */
	ae_f16x4 init_gain;
#else /* Generic version of gain processing */
	/**< Input gain coefficients */
	int16_t gain_coeffs[MAX_GAIN_COEFFS_CNT];
	/**< Step for fade-in */
	int16_t step_f16;
	/**< Initial gain */
	int16_t init_gain[MAX_GAIN_COEFFS_CNT];
#endif
	bool unity_gain; /**< Indicates unity gain coefficients, no processing is required */
	uint32_t silence_sg_count;  /**< Accumulates sample group spent on silence */
	uint32_t fade_in_sg_count;  /**< Accumulates sample group spent on fade-in */
	uint32_t silence_sg_length; /**< Total count of sample group spent on silence */
	uint32_t fade_sg_length;    /**< Total count of sample group spent on fade-in */
	uint64_t gain_env;  /**< Gain envelope for fade-in calculated in high precision */
	uint64_t step_i64;  /**< Step for fade-in envelope in high precision */
	uint16_t channels_count; /**< Number of channels */
};

/** Gain Coefficients IO Control
 *
 * This parameter is sent by the driver to add/modify a static gain.
 * Coefficients are encoded in Q10 format.
 */
struct gain_dma_control_data {
	uint16_t gain_coeffs[MAX_GAIN_COEFFS_CNT];
} __packed __aligned(4);

/**
 * @brief Sets gain parameters.
 *
 * This function sets the gain parameters for the copier component specified by
 * the given device and DAI data.
 *
 * @param dev The pointer to the component device structure.
 * @param gain_params The pointer to gain params structure.
 * @param fade_period The fade period in milliseconds.
 * @param dai_type DAI type
 * @return 0 on success, negative error code on failure.
 */
int copier_gain_set_params(struct comp_dev *dev, struct copier_gain_params *gain_params,
			   uint32_t fade_period, enum sof_ipc_dai_type dai_type);

/**
 * @brief Sets the basic gain parameters.
 *
 * This function sets the basic gain parameters for the copier component specified
 * by the given device and DAI data.
 *
 * @param dev The pointer to the component device structure.
 * @param gain_params The pointer to gain params structure.
 * @param ipc4_cfg The pointer to the IPC4 base module config.
 */
void copier_gain_set_basic_params(struct comp_dev *dev, struct copier_gain_params *gain_params,
				  struct ipc4_base_module_cfg *ipc4_cfg);

/**
 * @brief Sets the gain fade parameters.
 *
 * This function sets the fade gain parameters for the copier component specified
 * by the given device and DAI data.
 *
 * @param dev The pointer to the component device structure.
 * @param gain_params The pointer to gain params structure.
 * @param ipc4_cfg The pointer to the IPC4 base module config.
 * @param fade_period The fade period in milliseconds.
 * @param frames The number of frames to fade.
 * @return 0 on success, negative error code on failure.
 */
int copier_gain_set_fade_params(struct comp_dev *dev, struct copier_gain_params *gain_params,
				struct ipc4_base_module_cfg *ipc4_cfg,
				uint32_t fade_period, uint32_t frames);

/**
 * @brief Applies gain to a 16-bit container size.
 *
 * This function applies gain to the input audio buffer. There are three gain modes
 * supported: static gain, mute, and gain transition (fade-in or fade-out).
 *
 * @param buff Pointer to the input audio buffer.
 * @param state The state of the gain processing.
 * @param dir direction of the gain envelope change.
 * @param frames The number of frames to be processed.
 */
int copier_gain_input16(struct comp_buffer *buff, enum copier_gain_state state,
			enum copier_gain_envelope_dir dir,
			struct copier_gain_params *gain_params, uint32_t frames);

/**
 * @brief Applies gain to a 32-bit container size.
 *
 * This function applies gain to the input audio buffer. There are three gain modes
 * supported: static gain, mute, and gain transition (fade-in or fade-out).
 *
 * @param buff Pointer to the input audio buffer.
 * @param state The state of the gain processing.
 * @param dir Direction of the gain envelope change.
 * @param gain_params The pointer to the copier_gain_params structure.
 * @param frames The number of frames to be processed.
 */
int copier_gain_input32(struct comp_buffer *buff, enum copier_gain_state state,
			enum copier_gain_envelope_dir dir,
			struct copier_gain_params *gain_params, uint32_t frames);

/**
 * @brief Applies gain to the input audio buffer, selects the appropriate gain method.
 *
 * @param dev The pointer to the comp_dev structure representing the audio component device.
 * @param buff The pointer to the comp_buffer structure representing the input buffer.
 * @param gain_params The pointer to the copier_gain_params structure.
 * @param dir Direction of the gain envelope change.
 * @param stream_bytes The number of bytes in the input buffer.
 * @return 0 on success, negative error code on failure.
 */
int copier_gain_input(struct comp_dev *dev, struct comp_buffer *buff,
		      struct copier_gain_params *gain_params,
		      enum copier_gain_envelope_dir dir, uint32_t stream_bytes);

/**
 * Evaluates appropriate gain mode based on the current gain parameters
 *
 * @param gain_params The pointer to the copier_gain_params structure.
 * @return The state of the copier gain (enum copier_gain_state).
 */
enum copier_gain_state copier_gain_eval_state(struct copier_gain_params *gain_params);

/**
 * Sets/modify gain for a copier module in runtime.
 *
 * @param dev The copier device structure.
 * @param gain_params The pointer to the copier_gain_params structure.
 * @param gain_data The gain control data structure.
 * @param channels Number of audio channels.
 * @return 0 on success, otherwise a negative error code.
 */
int copier_set_gain(struct comp_dev *dev, struct copier_gain_params *gain_params,
		    struct gain_dma_control_data *gain_data, int channels);

/**
 * Checks for unity gain mode.
 *
 * @param gain_params The copier gain parameters structure.
 * @return true if the gain is set to unity gain, false otherwise.
 */
bool copier_is_unity_gain(struct copier_gain_params *gain_params);

/**
 * Controls the gain for a copier device using DMA Control IPC message.
 *
 * This function retrieves gain data from the DMA Control IPC message and updates
 * corresponding dai device gain params structure.
 *
 * @param node Gateway node id.
 * @param config_data The gain configuration data.
 * @param config_size The size of the gain configuration data.
 * @param dai_type The type of the DAI device.
 * @return 0 on success, otherwise a negative error code.
 */
int copier_gain_dma_control(union ipc4_connector_node_id node, const char *config_data,
			    size_t config_size, enum sof_ipc_dai_type dai_type);

#endif /* __SOF_COPIER_GAIN_H__ */
