/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOC_TELEMETRY2_SLOT_H__
#define __SOC_TELEMETRY2_SLOT_H__

/*
 * TELEMETRY2 is a generic memory structure for passing real-time debug
 * information from the SOF system. It only provides the framework for
 * passing pieces of abstract data from DSP side to host side debugging
 * tools.
 *
 * This example describes how TELEMETRY2 data is transferred from DSP
 * to host side using debug memory window.
 *
 * TELEMETRY2 slot is taken from SRAM window divided into several
 * chunks with simple header declared below.
 *
 * "type" describes the contents of the chunk
 * "size" describes the size of the chunk in bytes
 *
 * After each chunk another chunk is expected to follow, until an
 * empty chunk header is found. If chunk type is 0 and size 0 it
 * indicates an empty header, and that the rest of the slot is unused.
 * A new chunk, with the header, can be reserved stating at the end of
 * previous chunk (which has been aligned to the end of cache line).
 *
 * Each chunk is aligned to start at 64-byte cache line boundary.
 *
 * For example:
 *      --------------------------------------------------  ---
 *      | magic = TELEMETRY2_PAYLOAD_MAGIC               |   |
 *      | hdr_size = 64                                  |   |
 *      | total_size = 320                               |   |
 *      | abi = TELEMETRY2_PAYLOAD_V0_0                  |  64 bytes
 *      | tstamp = <aligned with host epoch>             |   |
 *      | <padding>                                      |   |
 *	--------------------------------------------------  ---
 *	| type = ADSP_DW_TELEMETRY2_ID_THREAD_INFO       |   |
 *      | size = 256                                     |  256 bytes
 *	|    chunk data                                  |   |
 *	|    ...                                         |   |
 *	--------------------------------------------------  ---
 *	| type = ADSP_DW_TELEMETRY2_TYPE_EMPTY           |
 *      | size = 0                                       |
 *	--------------------------------------------------
 *
 * The above depicts a telemetry2 slot with thread analyzer data
 * of 256 bytes (including the header) in the first chunk and
 * the rest of the slot being free.
 */

#include <stdint.h>

#define TELEMETRY2_PAYLOAD_MAGIC 0x1ED15EED

#define TELEMETRY2_PAYLOAD_V0_0 0

/* Telemetry2 payload header
 *
 * The payload header should be written in the beginning of the
 * telemetry2 payload, before the chunks.
 */
struct telemetry2_payload_hdr {
	uint32_t magic;		// used to identify valid data
	uint32_t hdr_size;	// size of this header only in bytes
	uint32_t total_size;	// total size of the whole payload in bytes
	uint32_t abi;		// ABI version, can be used by tool to warn users if too old.
	uint64_t tstamp;	// aligned with host epoch.
} __packed __aligned(CONFIG_DCACHE_LINE_SIZE);

/* Defined telemetry2 chunk types */
#define TELEMETRY2_CHUNK_ID_EMPTY		0
#define TELEMETRY2_ID_THREAD_INFO		1

/* Telemetry2 chunk header */
struct telemetry2_chunk_hdr {
	uint32_t id;		// Chunk ID
	uint32_t size;		// Size of telemetry chunk
} __packed;

void *telemetry2_chunk_get(int id, size_t size);

#endif /* __SOC_TELEMETRY2_SLOT_H__ */
