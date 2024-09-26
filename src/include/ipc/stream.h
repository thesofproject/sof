/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 - 2023 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/ipc/stream.h
 * \brief IPC definitions for streams.
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __IPC_STREAM_H__
#define __IPC_STREAM_H__

#include <module/ipc/stream.h>
#include <ipc/header.h>
#include <stdint.h>

/*
 * Stream configuration.
 */
/* common sample rates for use in masks */
#define SOF_RATE_8000		(1 <<  0) /**< 8000Hz  */
#define SOF_RATE_11025		(1 <<  1) /**< 11025Hz */
#define SOF_RATE_12000		(1 <<  2) /**< 12000Hz */
#define SOF_RATE_16000		(1 <<  3) /**< 16000Hz */
#define SOF_RATE_22050		(1 <<  4) /**< 22050Hz */
#define SOF_RATE_24000		(1 <<  5) /**< 24000Hz */
#define SOF_RATE_32000		(1 <<  6) /**< 32000Hz */
#define SOF_RATE_44100		(1 <<  7) /**< 44100Hz */
#define SOF_RATE_48000		(1 <<  8) /**< 48000Hz */
#define SOF_RATE_64000		(1 <<  9) /**< 64000Hz */
#define SOF_RATE_88200		(1 << 10) /**< 88200Hz */
#define SOF_RATE_96000		(1 << 11) /**< 96000Hz */
#define SOF_RATE_176400		(1 << 12) /**< 176400Hz */
#define SOF_RATE_192000		(1 << 13) /**< 192000Hz */

/* continuous and non-standard rates for flexibility */
#define SOF_RATE_CONTINUOUS	(1 << 30)  /**< range */
#define SOF_RATE_KNOT		(1 << 31)  /**< non-continuous */

/* generic PCM flags for runtime settings */
#define SOF_PCM_FLAG_XRUN_STOP	(1 << 0) /**< Stop on any XRUN */

/* stream buffer format */
enum sof_ipc_buffer_format {
	SOF_IPC_BUFFER_INTERLEAVED,
	SOF_IPC_BUFFER_NONINTERLEAVED,
	/* other formats here */
};

/* stream direction */
enum sof_ipc_stream_direction {
	SOF_IPC_STREAM_PLAYBACK = 0,
	SOF_IPC_STREAM_CAPTURE,
};

/* stream ring info */
struct sof_ipc_host_buffer {
	struct sof_ipc_hdr hdr;
	uint32_t phy_addr;
	uint32_t pages;
	uint32_t size;
	uint32_t reserved[3];
} __attribute__((packed, aligned(4)));

struct sof_ipc_stream_params {
	struct sof_ipc_hdr hdr;
	struct sof_ipc_host_buffer buffer;
	uint32_t direction;	/**< enum sof_ipc_stream_direction */
	uint32_t frame_fmt;	/**< enum sof_ipc_frame */
	uint32_t buffer_fmt;	/**< enum sof_ipc_buffer_format */
	uint32_t rate;
	uint16_t stream_tag;
	uint16_t channels;
	uint16_t sample_valid_bytes;
	uint16_t sample_container_bytes;

	uint32_t host_period_bytes;
	uint16_t no_stream_position; /**< 1 means don't send stream position */
	uint8_t cont_update_posn; /**< 1 means continuous update stream position */
	uint8_t reserved0;
	uint16_t ext_data_length; /**< 0 means no extended data */

	uint8_t reserved[2];
	uint16_t chmap[SOF_IPC_MAX_CHANNELS];	/**< channel map - SOF_CHMAP_ */
	int8_t data[]; /**< extended data */
} __attribute__((packed, aligned(4)));

/* PCM params info - SOF_IPC_STREAM_PCM_PARAMS */
struct sof_ipc_pcm_params {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t comp_id;
	uint32_t flags;		/**< generic PCM flags - SOF_PCM_FLAG_ */
	uint32_t reserved[2];
	struct sof_ipc_stream_params params;
} __attribute__((packed, aligned(4)));

/* PCM params info reply - SOF_IPC_STREAM_PCM_PARAMS_REPLY */
struct sof_ipc_pcm_params_reply {
	struct sof_ipc_reply rhdr;
	uint32_t comp_id;
	uint32_t posn_offset;
} __attribute__((packed, aligned(4)));

/* free stream - SOF_IPC_STREAM_PCM_PARAMS */
struct sof_ipc_stream {
	struct sof_ipc_cmd_hdr hdr;
	uint32_t comp_id;
} __attribute__((packed, aligned(4)));

/* flags indicating which time stamps are in sync with each other */
#define	SOF_TIME_HOST_SYNC	(1 << 0)
#define	SOF_TIME_DAI_SYNC	(1 << 1)
#define	SOF_TIME_WALL_SYNC	(1 << 2)
#define	SOF_TIME_STAMP_SYNC	(1 << 3)

/* flags indicating which time stamps are valid */
#define	SOF_TIME_HOST_VALID	(1 << 8)
#define	SOF_TIME_DAI_VALID	(1 << 9)
#define	SOF_TIME_WALL_VALID	(1 << 10)
#define	SOF_TIME_STAMP_VALID	(1 << 11)

/* flags indicating time stamps are 64bit else 3use low 32bit */
#define	SOF_TIME_HOST_64	(1 << 16)
#define	SOF_TIME_DAI_64		(1 << 17)
#define	SOF_TIME_WALL_64	(1 << 18)
#define	SOF_TIME_STAMP_64	(1 << 19)

struct sof_ipc_stream_posn {
	struct sof_ipc_reply rhdr;
	uint32_t comp_id;	/**< host component ID */
	uint32_t flags;		/**< SOF_TIME_ */
	uint32_t wallclock_hz;	/**< frequency of wallclock in Hz */
	uint32_t timestamp_ns;	/**< resolution of timestamp in ns */
	uint64_t host_posn;	/**< host DMA position in bytes */
	uint64_t dai_posn;	/**< DAI DMA position in bytes */
	uint64_t comp_posn;	/**< comp position in bytes */
	uint64_t wallclock;	/**< audio wall clock */
	uint64_t timestamp;	/**< system time stamp */
	uint32_t xrun_comp_id;	/**< comp ID of XRUN component */
	int32_t xrun_size;	/**< XRUN size in bytes */
} __attribute__((packed, aligned(4)));

#endif /* __IPC_STREAM_H__ */
