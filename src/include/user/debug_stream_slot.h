/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

/**
 * \file include/user/debug_stream_slot.h
 * \brief SRAM Memory Window 2 Debug Slot mapping and per-core circular buffers.
 * \ingroup debug_stream_api
 */

#ifndef __SOC_DEBUG_WINDOW_SLOT_H__
#define __SOC_DEBUG_WINDOW_SLOT_H__

#include <user/debug_stream.h>

/*
 * This file describes how Debug Stream can be transported over debug
 * window memory slot. The debug stream is an abstract media API for
 * passing debug data records from SOF DSP to the host system for
 * presentation. The debug stream records are described in debug_stream.h.
 *
 * This example describes how Debug Stream data is transferred from
 * DSP to host side using a debug memory window slot. To learn more
 * see soc/intel/intel_adsp/common/include/adsp_debug_window.h
 * under Zephyr source tree.
 *
 * DEBUG_STREAM slot is reserved from SRAM window and a header is
 * written in the beginning of the slot. The header is a static data
 * structure that is initialized once at DSP boot time. All elements
 * in bellow example are 32-bit unsigned integers:
 *
 *      --------------------------------------------------
 *      | id = DEBUG_STREAM_IDENTIFIER                   |
 *      | total_size = 4096                              |
 *      | num_sections = CONFIG_MP_MAX_NUM_CPUS *        |
 *      | section_descriptor [] = {                      |
 *      |   {                                            |
 *      |      core_id = 0                               |
 *      |      size = 1344                               |
 *      |      offset = 64                               |
 *      |   }                                            |
 *      |   {                                            |
 *      |      core_id = 1                               |
 *      |      size = 1344                               |
 *      |      offset = 1344+64                          |
 *      |   }                                            |
 *      |   {                                            |
 *      |      core_id = 2                               |
 *      |      size = 1344                               |
 *      |      offset = 2*1344+64                        |
 *      |   }                                            |
 *      | }                                              |
 *      | <padding>                                      |
 *      -------------------------------------------------- n * 64 bytes
 *   * CONFIG_MP_MAX_NUM_CPUS is 3 in this example
 *
 * The header contains generic information like identifier, total
 * size, and number of sections. After the generic fields there is an
 * array of section descriptors. The array has 'num_sections' number
 * of elements. Each element in the array describes a circular buffer,
 * one for each DSP core.
 *
 * The remaining memory in the debug window slot is divided between
 * those sections. The buffers are not necessarily of equal size, like
 * in this example. The circular buffers are all cache line aligned,
 * 64 in this example. One section looks like this:
 *
 *      --------------------------------------------------  ---
 *      | next_seqno = <counter for written objects>     |   |
 *      | w_ptr = <write position in 32-bit words>       | 1344 bytes
 *      | buffer_data[1344/4-2] = {                      |   |
 *      |    <debug data records>                        |   |
 *      | }                                              |   |
 *      --------------------------------------------------  ---
 *
 * The data records are described in debug_strem.h. In the case of
 * debug window memory slot the next record should always be aligned
 * to word (4-byte) boundary.
 *
 * The debug stream writes the records of abstract data to the
 * circular buffer, and updates the w_ptr when the record is
 * completely written. The host side receiver tries to keep up with the
 * w_ptr and keeps track of its read position.
 *
 * The size of the record is written - again - after each record and
 * before the next. This is to allow parsing the stream backwards in
 * an overrun recovery situation. The w_ptr value is updated last,
 * when the record is completely written.
 */

#include <stdint.h>

/**
 * \brief Core-specific section descriptor within the debug window slot.
 * \ingroup debug_stream_api
 *
 * Each descriptor maps a core-specific circular buffer inside Memory Window 2.
 * The `offset` field defines the byte offset from the start of `debug_stream_slot_hdr`
 * to the core's `debug_stream_circular_buf` structure (aligned to cache line and 4-byte
 * word boundaries). The `buf_words` field defines the capacity of the circular buffer
 * `data[]` array in 32-bit words.
 */
struct debug_stream_section_descriptor {
	uint32_t core_id;	/**< Reporting DSP core identifier */
	uint32_t buf_words;	/**< Circular buffer data capacity in 32-bit words */
	uint32_t offset;	/**< Byte offset from slot base (4-byte aligned) */
} __packed;

/**
 * \brief Preamble header placed at the beginning of Memory Window 2 Debug Slot.
 * \ingroup debug_stream_api
 */
struct debug_stream_slot_hdr {
	struct debug_stream_hdr hdr;				/**< Base stream identifier and size */
	uint32_t total_size;					/**< Total size of payload across all core sections */
	uint32_t num_sections;					/**< Number of active core-specific sections */
	struct debug_stream_section_descriptor section_desc[];	/**< Array of per-core section descriptors */
} __packed;

/**
 * \brief Per-core circular buffer layout within a debug stream slot.
 * \ingroup debug_stream_api
 */
struct debug_stream_circular_buf {
	uint32_t next_seqno;	/**< Sequence counter of next record to write */
	uint32_t w_ptr;		/**< Current write position in 32-bit words */
	uint32_t data[];	/**< Circular data buffer holding stream records */
} __aligned(CONFIG_DCACHE_LINE_SIZE);

struct debug_stream_record;

/**
 * \brief Send a debug stream record over the memory window debug slot.
 * \ingroup debug_stream_api
 * @param[in] rec Pointer to abstract debug stream record to transmit.
 * @return 0 on success, -ENODEV if debug slot is unconfigured, or -ENOMEM if record exceeds capacity.
 */
#if defined(CONFIG_USERSPACE)
__syscall int debug_stream_slot_send_record(struct debug_stream_record *rec);

#include <zephyr/syscalls/debug_stream_slot.h>
#else
int z_impl_debug_stream_slot_send_record(struct debug_stream_record *rec);
#define debug_stream_slot_send_record z_impl_debug_stream_slot_send_record
#endif

#endif /* __SOC_DEBUG_WINDOW_SLOT_H__ */
