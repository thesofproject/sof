/*
 * Copyright (c) 2019, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Intel Corporation nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Lech Betlej <lech.betlej@linux.intel.com>
 */

/**
 * \file audio/selector.h
 * \brief Channel selector component header file
 * \authors Lech Betlej <lech.betlej@linux.intel.com>
 */

#ifndef SELECTOR_H
#define SELECTOR_H

#include <stdint.h>
#include <sof/audio/component.h>
#include <sof/audio/pipeline.h>
#include <sof/audio/format.h>

/** \brief Selector trace function. */
#define trace_selector(__e, ...) \
	trace_event(TRACE_CLASS_SELECTOR, __e, ##__VA_ARGS__)

/** \brief Selector trace verbose function. */
#define tracev_selector(__e, ...) \
	tracev_event(TRACE_CLASS_SELECTOR, __e, ##__VA_ARGS__)

/** \brief Selector trace error function. */
#define trace_selector_error(__e, ...) \
	trace_error(TRACE_CLASS_SELECTOR, __e, ##__VA_ARGS__)

/** \brief Supported channel count on input. */
#define SEL_SOURCE_2CH 2
#define SEL_SOURCE_4CH 4

/** \brief Supported channel count on output. */
#define SEL_SINK_1CH 1
#define SEL_SINK_2CH 2
#define SEL_SINK_4CH 4

/** \brief Selector component private data. */
struct comp_data {
	uint32_t source_period_bytes;	/**< source number of period bytes */
	uint32_t sink_period_bytes;	/**< sink number of period bytes */
	enum sof_ipc_frame source_format;	/**< source frame format */
	enum sof_ipc_frame sink_format;		/**< sink frame format */

	/* selector supports 1 input and 1 output */
	uint32_t in_channels_count;	/**< accepted values 2 or 4 */
	uint32_t out_channels_count;	/**< accepted values 1 or 2 or 4 */

	/* note: if 2 or 4 output channels selected the component works in
	 * a passthrough mode
	 */
	uint32_t sel_channel;	/**< 0..3 */

	/**< channel selector processing function */
	void (*sel_func)(struct comp_dev *dev, struct comp_buffer *sink,
		struct comp_buffer *source, uint32_t frames);
};

/** \brief Selector processing functions map. */
struct comp_func_map {
	uint16_t source;	/**< source frame format */
	uint32_t out_channels;	/**< number of output stream channels */
	/**< selector processing function */
	void (*sel_func)(struct comp_dev *dev, struct comp_buffer *sink,
			 struct comp_buffer *source, uint32_t frames);
};

/** \brief Map of formats with dedicated processing functions. */
extern const struct comp_func_map func_map[];

typedef void (*sel_func)(struct comp_dev *, struct comp_buffer *,
			 struct comp_buffer *, uint32_t);

/**
 * \brief Retrieves selector processing function.
 * \param[in,out] dev Selector base component device.
 */
sel_func sel_get_processing_function(struct comp_dev *dev);

#endif /* SELECTOR_H */
