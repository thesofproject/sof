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
#include <sof/audio/module_adapter/module/generic.h>
#include <ipc4/base-config.h>
#include <user/trace.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__XCC__)
# include <xtensa/config/core-isa.h>
# if XCHAL_HAVE_HIFI3 || XCHAL_HAVE_HIFI4
#  define ARIA_HIFI3
# else
#  define ARIA_GENERIC
# endif
#else
# define ARIA_GENERIC
#endif

/** \brief Aria max gain states */
#define ARIA_MAX_GAIN_STATES 10

/** \brief Aria max att value */
#define ARIA_MAX_ATT 3

/**
 * \brief aria get data function interface
 */
typedef void (*aria_get_data_func)(struct processing_module *mod,
				   struct audio_stream *sink, int frames);

struct aria_data;
/**
 * \brief Aria gain processing function
 */
void aria_algo_calc_gain(struct aria_data *cd, size_t gain_idx,
			 struct audio_stream *source, int frames);

aria_get_data_func aria_algo_get_data_func(struct processing_module *mod);

/**
 * \brief Aria component private data.
 */
struct aria_data {
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
	/* cyclic buffer pointer data */
	int32_t *data_ptr;
	/* cyclic buffer end address of data */
	int32_t *data_end;
	/* cyclic buffer start address of data */
	int32_t *data_addr;

	/* internal buffer offset helps to keep algorithmic delay constant */
	size_t offset;

	aria_get_data_func aria_get_data;
};

extern const uint8_t INDEX_TAB[];

#endif /* __SOF_AUDIO_ARIA_H__ */
