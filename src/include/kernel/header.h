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
 * Only in case of IPC3 the data header used for all component data structures
 * and binary blobs sent to firmware as runtime data. This data is typically sent
 * by userspace applications and tunnelled through any OS kernel (via binary
 * kcontrol on Linux) to the firmware.
 * With IPC4 the ABI header is used between user space and kernel for verification
 * purposes and to provide information about the attached binary blob, like the
 * param_id of it.
 */
struct sof_abi_hdr {
	uint32_t magic;		/**< Magic number for validation */
				/**< for IPC3 data: 0x00464F53 ('S', 'O', 'F', '\0') */
				/**< for IPC4 data: 0x34464F53 ('S', 'O', 'F', '4') */
	uint32_t type;		/**< module specific parameter */
				/**< for IPC3: Component specific type */
				/**< for IPC4: parameter ID (param_id) of the data */
	uint32_t size;		/**< size in bytes of data excl. this struct */
	uint32_t abi;		/**< SOF ABI version. */
				/**< The version is valid in scope of the 'magic', */
				/**< IPC3 and IPC4 ABI version numbers have no relationship. */
	uint32_t reserved[4];	/**< reserved for future use */
	uint32_t data[];	/**< Component data - opaque to core */
} __attribute__((packed));

#endif /* __KERNEL_HEADER_H__ */
