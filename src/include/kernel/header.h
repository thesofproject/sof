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

#ifndef __KERNEL_HEADER_H__
#define __KERNEL_HEADER_H__

#include <stdint.h>

/**
 * \brief Header for all non IPC ABI data.
 *
 * Identifies data type, size and ABI.
 * Data header used for all component data structures and binary blobs sent to
 * firmware as runtime data. This data is typically sent by userspace
 * applications and tunnelled through any OS kernel (via binary kcontrol on
 * Linux) to the firmware.
 */
struct sof_abi_hdr {
	uint32_t magic;		/**< 'S', 'O', 'F', '\0' */
	uint32_t type;		/**< component specific type */
	uint32_t size;		/**< size in bytes of data excl. this struct */
	uint32_t abi;		/**< SOF ABI version */
	uint32_t reserved[4];	/**< reserved for future use */
	uint32_t data[0];	/**< Component data - opaque to core */
} __attribute__((packed));

/**
 * struct sof_ipc4_abi_hdr - Used by any bespoke component data structures or binary blobs.
 * @magic: The magic number for the header. The value is 'S', 'O', 'F', '4'
 * @size: The size in bytes of the data, excluding this struct
 * @abi: SOF ABI version
 * @blob_type: Type of blob: INIT_INSTANCE, CONFIG_SET or LARGE_CONFIG_SET
 *             The value is of type enum sof_ipc4_module_type
 * @param_id: ID indicating which parameter to update with the new data. The validity of
 *            the param_id with blob_type is dependent on the module implementation.
 * @reserved: Reserved for future use
 * @data: Component data - opaque to core
 */
struct sof_ipc4_abi_hdr {
	uint32_t magic;
	uint32_t size;
	uint32_t abi;
	uint32_t blob_type;
	uint32_t param_id;
	uint32_t reserved[3];
	uint32_t data[];
}  __packed;

#endif /* __KERNEL_HEADER_H__ */
