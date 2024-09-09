/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
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

/* Core specific section descriptor
 *
 * Section descriptor defines core ID, offset and size of the circular
 * buffer in the debug window slot.
 */
struct debug_stream_section_descriptor {
	uint32_t core_id;	/* Core ID */
	uint32_t buf_words;	/* Circular buffer size in 32-bit words */
	uint32_t offset;	/* Core section offset */
} __packed;

/* Debug window slot header for Debug Stream.
 *
 * The header should be written in the beginning of the slot.
 */
struct debug_stream_slot_hdr {
	struct debug_stream_hdr hdr;
	uint32_t total_size;	/* total size of payload including all sections */
	uint32_t num_sections;	/* number of core specific sections */
	struct debug_stream_section_descriptor section_desc[];
} __packed;

struct debug_stream_circular_buf {
	uint32_t next_seqno;
	uint32_t w_ptr;
	uint32_t data[];
} __aligned(CONFIG_DCACHE_LINE_SIZE);

struct debug_stream_record;
/**
 * \brief Send debug stream records over debug window slot
 *
 * \param[in] rec the record to be written to circular buffer
 *
 * The debug window slot is initialized automatically at DSP boot
 * time, and the core specific circular buffer is selected
 * automatically.
 *
 * \return 0 on success
 *         -ENODEV if debug stream slot is not configured
 *         -ENOMEM if the record is too big
 */
int debug_stream_slot_send_record(struct debug_stream_record *rec);

#endif /* __SOC_DEBUG_WINDOW_SLOT_H__ */
