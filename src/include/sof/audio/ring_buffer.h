/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 *
 */

/**
 * @file sof/audio/ring_buffer.h
 * @brief Lockless async circular ring buffer for cross-core audio processing.
 * @ingroup audio_buffer_api
 */

#ifndef __SOF_RING_BUFFER_H__
#define __SOF_RING_BUFFER_H__

#include <sof/audio/sink_api.h>
#include <sof/audio/source_api.h>
#include <sof/audio/audio_stream.h>
#include <sof/audio/audio_buffer.h>
#include <rtos/bit.h>
#include <sof/common.h>
#include <ipc/topology.h>
#include <sof/coherent.h>

/** \addtogroup audio_buffer_api
 *  @{
 */

/**
 * ring_buffer is a lockless async circular buffer
 * providing safe consumer/producer cached operations cross cores that may be write/read
 * at any time
 *
 * prerequisites:
 *  1) incoming and outgoing data rate MUST be the same
 *  2) Both data consumer and data producer declare max chunk sizes they want to use (IBS/OBS)
 *
 * required Buffer size is 2*MAX(IBS,OBS) to allow free read/write in various data chunk sizes
 * and execution periods (of course in/out data rates must be same)
 * example:
 *  Consumer reads 5bytes each 3 cycles (IBS = 5)
 *  producer writes 3bytes every 5 cycles (OBS = 3)
 *    - cycle0 buffer empty, producer starting processing, consumer must wait
 *    - cycle3 produce 3 bytes (buf occupation = 3)
 *    - cycle6 produce 3 bytes (buf occupation = 6), consumer becomes ready
 *		in consumer thread will start now - asyn to producer cycles
 *		in this example assuming it consumes data in next cycle
 *    - cycle7 consume 5 bytes, (buf occupation = 1)
 *    - cycle9 produce 3 bytes (buf occupation = 4)
 *    - cycle12 (producer goes first) produce 3 bytes (buf occupation = 7)
 *		consume 5 bytes (buf occupation = 2)
 *    - cycle15 produce 3 bytes (buf occupation = 5)
 *		consumer has enough data, but is busy processing prev data
 *    - cycle15 consume 5 bytes (buf occupation = 0)
 *
 * ===> max buf occupation = 7
 *
 * The worst case is when IBS=OBS and equal periods of consumer/producer
 * the buffer must be 2*MAX(IBS,OBS) as we do not know who goes first - consumer or producer,
 * especially when both are located on separate cores and EDF scheduling is used
 *
 *  Consumer reads 5 bytes every cycle (IBS = 5)
 *  producer writes 5 bytes every cycle (OBS = 5)
 *    - cycle0 consumer goes first - must wait (buf occupation = 0)
 *		producer produce 5 bytes (buf occupation = 5)
 *    - cycle1 producer goes first - produce 5 bytes (buf occupation = 10)
 *		consumer consumes 5 bytes (buf occupation = 5)
 * ===> max buf occupation = 10
 *
 *
 * The queue may work in 2 modes
 * 1) local mode
 *    in case both receiver and sender are located on the same core and cache coherency
 *    does not matter. ring_buffer structure is located in cached memory
 *
 * 2) shared mode
 *    In this case we need to writeback cache when new data arrive and invalidate cache on
 *    secondary core. ring_buffer structure is located in shared memory
 *
 *
 * ring_buffer is a lockless consumer/producer safe buffer. It is achieved by having only 2 shared
 * variables:
 *  _write_offset - can be modified by data producer only
 *  _read_offset - can be modified by data consumer only
 *
 *  as 32 bit operations are atomic, it is multi-thread and multi-core save
 *
 * There some explanation needed how free_space and available_data are calculated
 *
 * number of avail data in circular buffer may be calculated as:
 *	data_avail = _write_offset - _read_offset
 *   and check for wrap around
 *	if (data_avail < 0) data_avail = buffer_size - data_avail
 *
 * The problem is when _write_offset == _read_offset,
 * !!! it may mean either that the buffer is empty or the buffer is completely filled !!!
 *
 * To solve the above issue having only 2 variables mentioned before:
 *  - allow both offsets to point from 0 to DOUBLE buffer_size
 *  - when calculating pointers to data, use: data_bufer[offset % buffer_size]
 *  - use double buffer size in wrap around check when calculating available data
 *
 * And now:
 *   - _write_offset == _read_offset
 *		always means "buffer empty"
 *   - _write_offset == _read_offset + buffer_size
 *		always means "buffer full"
 */

struct comp_dev;
struct ring_buffer;
struct sof_audio_stream_params;

/**
 * \brief Lockless asynchronous circular ring buffer structure.
 */
struct ring_buffer {
	/* public: read only */
	struct sof_audio_buffer audio_buffer;	/**< Embedded audio buffer base structure */

	size_t data_buffer_size;		/**< Total allocated data buffer capacity in bytes */

	uint8_t __sparse_cache *_data_buffer;	/**< Pointer to circular data buffer memory */
	size_t _write_offset;		/**< Producer write offset in bytes */
	size_t _read_offset;		/**< Consumer read offset in bytes */
};

/**
 * \brief Create and allocate a lockless async circular ring buffer.
 * \param[in,out] dev Pointer to component device creating this buffer.
 * \param[in] min_available Minimum data available required by source API consumer.
 * \param[in] min_free_space Minimum buffer space required by sink API producer.
 * \param[in] is_shared True if buffer memory is shared across processing cores.
 * \param[in] id Stream identifier accessible via sink/source interface.
 * \return Pointer to newly allocated ring_buffer, or NULL on failure.
 */
struct ring_buffer *ring_buffer_create(struct comp_dev *dev, size_t min_available,
				       size_t min_free_space, bool is_shared,
				       uint32_t id);

/** @}*/

#endif /* __SOF_RING_BUFFER_H__ */
