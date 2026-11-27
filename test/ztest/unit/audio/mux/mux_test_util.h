/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2026 Intel Corporation.
 *
 * These contents may have been developed with support from one or more
 * Intel-operated generative artificial intelligence solutions.
 */

#ifndef __TEST_ZTEST_UNIT_AUDIO_MUX_TEST_UTIL_H__
#define __TEST_ZTEST_UNIT_AUDIO_MUX_TEST_UTIL_H__

#include <sof/audio/buffer.h>
#include <sof/audio/component.h>
#include <sof/lib/uuid.h>

#include <mux/mux.h>

#include <stdint.h>

/**
 * @brief Create a buffer that the component under test writes to.
 * @param dev Component the buffer is connected to as a sink, or NULL.
 * @param pipeline_id Pipeline ID of the buffer, used by mux and demux to
 *		      find the stream configuration of the connection.
 * @param frame_fmt Frame format of the buffer.
 * @param channels Number of channels of the buffer.
 * @param buffer_size Buffer size in bytes.
 * @return The new buffer, never NULL.
 */
struct comp_buffer *mux_test_create_sink(struct comp_dev *dev, uint32_t pipeline_id,
					 uint32_t frame_fmt, uint16_t channels,
					 uint16_t buffer_size);

/**
 * @brief Free a buffer created by mux_test_create_sink().
 * @param buffer Buffer to free, NULL is ignored.
 */
void mux_test_free_sink(struct comp_buffer *buffer);

/**
 * @brief Create a buffer that the component under test reads from.
 * @param dev Component the buffer is connected to as a source, or NULL.
 * @param pipeline_id Pipeline ID of the buffer, used by mux and demux to
 *		      find the stream configuration of the connection.
 * @param frame_fmt Frame format of the buffer.
 * @param channels Number of channels of the buffer.
 * @param buffer_size Buffer size in bytes.
 * @return The new buffer, never NULL.
 */
struct comp_buffer *mux_test_create_source(struct comp_dev *dev, uint32_t pipeline_id,
					   uint32_t frame_fmt, uint16_t channels,
					   uint16_t buffer_size);

/**
 * @brief Free a buffer created by mux_test_create_source().
 * @param buffer Buffer to free, NULL is ignored.
 */
void mux_test_free_source(struct comp_buffer *buffer);

/**
 * @brief Create a mux or demux component through the IPC3 comp_new() path.
 *
 * The component is configured with MUX_MAX_STREAMS streams, stream i on
 * pipeline i with the routing masks in @p mask.
 *
 * @param uuid UUID of the mux or the demux driver.
 * @param mask Routing masks, one row per stream, or NULL for all zero masks.
 * @return The new component, or NULL if comp_new() failed.
 */
struct comp_dev *mux_test_comp_new(const struct sof_uuid *uuid,
				   const uint8_t mask[MUX_MAX_STREAMS]
						     [CONFIG_PLATFORM_MAX_CHANNELS]);

#endif /* __TEST_ZTEST_UNIT_AUDIO_MUX_TEST_UTIL_H__ */
