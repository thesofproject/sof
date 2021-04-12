/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Intel Corporation. All rights reserved.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

/**
 * \file audio/selector.h
 * \brief Channel selector component header file
 * \authors Lech Betlej <lech.betlej@linux.intel.com>
 */

#ifndef __SOF_AUDIO_SELECTOR_H__
#define __SOF_AUDIO_SELECTOR_H__

#include <sof/trace/trace.h>
#include <ipc/stream.h>
#include <user/selector.h>
#include <user/trace.h>
#include <stdint.h>

struct comp_buffer;
struct comp_dev;

/** \brief Supported channel count on input. */
#define SEL_SOURCE_2CH 2
#define SEL_SOURCE_4CH 4

/** \brief Supported channel count on output. */
#define SEL_SINK_1CH 1
#define SEL_SINK_2CH 2
#define SEL_SINK_4CH 4

/** \brief selector processing function interface */
typedef void (*sel_func)(struct comp_dev *dev, struct audio_stream *sink,
			 const struct audio_stream *source, uint32_t frames);

/** \brief Selector component private data. */
struct comp_data {
	uint32_t source_period_bytes;	/**< source number of period bytes */
	uint32_t sink_period_bytes;	/**< sink number of period bytes */
	enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */
	struct sof_sel_config config;	/**< component configuration data */
	sel_func sel_func;	/**< channel selector processing function */
};

/** \brief Selector processing functions map. */
struct comp_func_map {
	uint16_t source;	/**< source frame format */
	uint32_t out_channels;	/**< number of output stream channels */
	sel_func sel_func;	/**< selector processing function */
};

/** \brief Map of formats with dedicated processing functions. */
extern const struct comp_func_map func_map[];

/**
 * \brief Retrieves selector processing function.
 * \param[in,out] dev Selector base component device.
 */
sel_func sel_get_processing_function(struct comp_dev *dev);

#ifdef UNIT_TEST
void sys_comp_selector_init(void);
#endif

#endif /* __SOF_AUDIO_SELECTOR_H__ */
