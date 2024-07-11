/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2024 Intel Corporation.
 */

#ifndef __SOC_DEBUG_STREAM_H__
#define __SOC_DEBUG_STREAM_H__

/*
 * Debug Stream is a stream protocol for passing real-time debug
 * information from SOF system. It provides the framework for
 * passing pieces of abstract data objects from DSP side to host side
 * debugging tools.
 *
 * The details of Debug Stream protocol varies depending on transfer
 * method, but the stream should always start with a header that
 * consists of DEBUG_STREAM_IDENTIFIER and header size.
 */

#define DEBUG_STREAM_IDENTIFIER 0x1ED15EED /* value for 'magic' */

struct debug_stream_hdr {
	uint32_t magic;		/* Magic number to recognize stream start */
	uint32_t hdr_size;	/* Header size */
} __packed;

/*
 * After the header ('hdr_size' bytes from beginning of 'magic') a
 * stream of Debug Stream records should follow. Each record will
 * start with a record identifier and record size, after which the
 * record payload will follow.
 *
 * The abstract data is application specific and is passed from DSP
 * debug entity to user space debug tool for decoding and
 * presentation. The data is recognized by the 'id' and the 'size_words'
 * describes the amount of data. The 'seqno' is a running number of sent
 * record, increased by one after each record. The protocol is
 * agnostic about the contents of the records.
 */

struct debug_stream_record {
	uint32_t id;		/* Record id of abstract data record */
	uint32_t seqno;		/* Increments after each record */
	uint32_t size_words;	/* Size of the whole record in words */
	uint32_t data[];
} __packed;

/* Debug Stream record identifiers */
#define DEBUG_STREAM_RECORD_ID_UNINITIALIZED	0 /* invalid record marker */
#define DEBUG_STREAM_RECORD_ID_THREAD_INFO	1 /* Thread info record */

#endif /* __SOC_DEBUG_STREAM_H__ */
