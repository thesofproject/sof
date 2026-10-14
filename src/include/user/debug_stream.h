/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

/**
 * \file include/user/debug_stream.h
 * \brief User-space ABI stream protocol for passing abstract debug data objects to host.
 * \ingroup debug_stream_api
 */

#ifndef __SOC_DEBUG_STREAM_H__
#define __SOC_DEBUG_STREAM_H__

/** Magic identifier at start of Debug Stream header (0x1ED15EED) */
#define DEBUG_STREAM_IDENTIFIER 0x1ED15EED

/**
 * \brief Stream preamble header recognized by host debug tools.
 * \ingroup debug_stream_api
 */
struct debug_stream_hdr {
	uint32_t magic;		/**< Magic identifier (DEBUG_STREAM_IDENTIFIER) */
	uint32_t hdr_size;	/**< Total size of stream header in bytes */
} __packed;

/**
 * \brief Abstract debug record envelope carrying application-specific payloads.
 * \ingroup debug_stream_api
 */
struct debug_stream_record {
	uint32_t id;		/**< Record identifier (DEBUG_STREAM_RECORD_ID_*) */
	uint32_t seqno;		/**< Monotonically increasing sequence number */
	uint32_t size_words;	/**< Total size of the record including header in 32-bit words */
	uint32_t data[];	/**< Application-specific record payload */
} __packed;

/** \name Debug Stream Record Identifiers
 *  Payload classification types for abstract debug stream records.
 *  @{
 */
#define DEBUG_STREAM_RECORD_ID_UNINITIALIZED	0 /**< Invalid or uninitialized record marker */
#define DEBUG_STREAM_RECORD_ID_THREAD_INFO	1 /**< DSP RTOS thread state snapshot record */
#define DEBUG_STREAM_RECORD_ID_TEXT_MSG		2 /**< Formatted ASCII/UTF-8 debug text message record */
/** @} */

#endif /* __SOC_DEBUG_STREAM_H__ */
