/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2022 Intel Corporation. All rights reserved.
 *
 * Author: Adrian Bonislawski <adrian.bonislawski@intel.com>
 */

/**
 * \file audio/aria/aria.h
 * \brief Aria component header file
 * \authors Adrian Bonislawski <adrian.bonislawski@intel.com>
 */

#ifndef __SOF_AUDIO_ARIA_H__
#define __SOF_AUDIO_ARIA_H__

#include <sof/audio/component_ext.h>
#include <sof/audio/ipc-config.h>
#include <rtos/bit.h>
#include <sof/common.h>
#include <sof/trace/trace.h>
#include <ipc/stream.h>
#include <ipc4/module.h>
#include <ipc4/base-config.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

/** \brief Aria max gain states */
#define ARIA_MAX_GAIN_STATES 10

/** \brief Aria max att value */
#define ARIA_MAX_ATT 3

/**
 * \brief Aria gain processing function
 */
void aria_algo_calc_gain(struct comp_dev *dev, size_t gain_idx,
			 int32_t *__restrict data, const size_t src_size);

/**
 * \brief Aria data processing function
 */
void aria_algo_get_data(struct comp_dev *dev, int32_t *__restrict  data,
			size_t size);

int aria_algo_buffer_data(struct comp_dev *dev, int32_t *__restrict data,
			  size_t size);
int aria_process_data(struct comp_dev *dev, int32_t *__restrict dst,
		      size_t dst_size, int32_t *__restrict src, size_t src_size);

/**
 * \brief Aria component private data.
 */
struct aria_data {
	struct ipc4_base_module_cfg base;

	/* channels count */
	size_t chan_cnt;
	/* sample groups to process count */
	size_t smpl_group_cnt;
	/* Size of chunk (1ms) in samples */
	size_t buff_size;
	/* Current gain state */
	size_t gain_state;
	/* current data position in circular buffer */
	size_t buff_pos;
	/* Attenuation parameter */
	size_t att;
	/* Gain states */
	int32_t gains[ARIA_MAX_GAIN_STATES];
	/* cyclic buffer for data */
	int32_t *data;
	/* buffer for data in */
	int32_t *buf_in;
	/* buffer for data out */
	int32_t *buf_out;
	/* internal buffer offset helps to keep algorithmic delay constant */
	size_t offset;
};

#endif /* __SOF_AUDIO_ARIA_H__ */
