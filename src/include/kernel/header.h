/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2018 Intel Corporation. All rights reserved.
 *
 * Author: Liam Girdwood <liam.r.girdwood@linux.intel.com>
 *         Keyon Jie <yang.jie@linux.intel.com>
 */

/**
 * \file include/kernel/header.h
 * \brief Non IPC command header
 * \author Liam Girdwood <liam.r.girdwood@linux.intel.com>
 * \author Keyon Jie <yang.jie@linux.intel.com>
 */

#ifndef __INCLUDE_UAPI_USER_HEADER_H__
#define __INCLUDE_UAPI_USER_HEADER_H__

/**
 * \brief Header for all non IPC ABI data.
 *
 * Identifies data type, size and ABI.
 * Used by any bespoke component data structures or binary blobs.
 */
struct sof_abi_hdr {
	uint32_t magic;		/**< 'S', 'O', 'F', '\0' */
	uint32_t type;		/**< component specific type */
	uint32_t size;		/**< size in bytes of data excl. this struct */
	uint32_t abi;		/**< SOF ABI version */
	uint32_t reserved[4];	/**< reserved for future use */
	uint32_t data[0];	/**< Component data - opaque to core */
} __attribute__((packed));

#endif
